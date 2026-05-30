#include "main.hpp"

// Wake-word activation window. Saying "Alexa" opens it locally so the command right
// after the keyword streams immediately (no wait for the server round-trip). The voice
// server slides it forward on every accepted turn via /tmp/respeaker_active_until_ms
// (written by respeaker_speaker on an `activation` frame). 30 s, matches VOICE_ACTIVATION_TTL_S.
#define ACTIVE_WINDOW_MS (30 * 1000LL)
// How long the wake/DOA animation holds before the steady LED logic resumes.
#define WAKE_LED_HOLD_MS 1500LL

static long long nowEpochMs()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void enablePixelRing(Config* config)
{
  setupPixelRing(config);

  if (-1 == setPowerPin() || -1 == cAPA102_Init(RUNTIME.LEDs, GLOBAL_BRIGHTNESS)) {
    cleanup(EXIT_FAILURE);
  }

  RUNTIME.curr_state = RUNTIME.if_mute ? TO_MUTE : TO_UNMUTE;
  state_machine_update();

  // It makes no sense to continue if WS is unavailable.
  wsClient = new WsTransport();
  if (!wsClient->connect(config->webSocketAddress()))
  {
    verbose(VV_INFO, stdout, "Unable to connect to WS server. Quitting...");
    cleanup(EXIT_FAILURE);
  }
}

/**
 * Exit program and release all the resources.
 */
void cleanup(int status)
{
  if (wsClient != nullptr) {
    wsClient->disconnect();
  }
  resetPowerPin();
  cAPA102_Close();
  pthread_cancel(RUNTIME.curr_thread);
  exit(status);
}

/**
 * Make sure we correctly handle exit signals to close resouces.
 */
void handleQuit(int signal)
{
  verbose(VV_INFO, stdout, "Caught signal %d. Terminating...", signal);
  shouldStopListening = true;
  RUNTIME.if_terminate = 1;
  pthread_cancel(RUNTIME.curr_thread);
}

void configureSignalHandler()
{
  struct sigaction sig_int_handler;
  sig_int_handler.sa_handler = handleQuit;
  sigemptyset(&sig_int_handler.sa_mask);
  sig_int_handler.sa_flags = 0;
  sigaction(SIGINT, &sig_int_handler, NULL);
  sigaction(SIGTERM, &sig_int_handler, NULL);
}

int main(int argc, char *argv[])
{
  configureSignalHandler();
  setVerbose(VV_INFO);
  
  config = new Config(CONFIG_FILE);
  if (!config->isRead())
  {
    verbose(VV_INFO, stdout, "Unable to read json config. Quitting...");
    exit(EXIT_FAILURE);
  }

  respeakerCore = new RespeakerCore(config);
  if (!respeakerCore->startListening(&shouldStopListening))
  {
    verbose(VV_INFO, stdout, "Unable to start the respeaker node chain. Quitting...");
    cleanup(EXIT_FAILURE);
  }
  else
  {
    enablePixelRing(config);
    verbose(VV_INFO, stdout, "Press CTRL-C to exit");
  }

  int wakeWordIndex = 0;
  TimePoint detectTime;
  string audioChunk;
  STATE prevLedState = ON_IDLE;
  long long wakeDeadlineMs = 0;   // local activation bootstrap (set on "Alexa")
  long long wakeLedUntilMs = 0;   // hold the wake/DOA animation until this time
  bool prevStreaming = false;     // edge-detect mic streaming to force-flush STT on close
  bool prevTurnGate = false;      // edge-detect turn end to reopen the listening window

  while (!shouldStopListening && trackPixelRingState())
  {
    audioChunk = respeakerCore->processAudio(wakeWordIndex);
    long long now_ms = nowEpochMs();

    // Three-state LED feedback driven by half-duplex signals:
    //   ON_SPEAK  — speaker actively playing TTS (highest precedence).
    //   ON_LISTEN — STT emitted `final`, Claude is thinking, no audio yet.
    //   ON_IDLE   — quiet, waiting for the kid.
    // Speaker rising edge clears the thinking deadline so the LED can't ghost-stay in
    // ON_LISTEN once playback starts. Thinking deadline carries a 10 s safety cap
    // inside WsTransport for the case where /prompt is 409-rejected (no playback).
    // Turn signals, all computed up front so wake / LED / mic share ONE gate.
    //   speakerActive — TTS playing (rolling per-chunk cursor; LED SPEAK precedence).
    //   busy          — voice's HARD turn gate, true for the whole turn with no gaps.
    //   thinking      — board-local, set on STT `final`; covers the STT-final → voice-busy
    //                   latency (~1 s) and any 409 path (self-clears at 10 s cap).
    bool speakerActive = WsTransport::isSpeakerActive();
    bool busy = WsTransport::isBusy();
    if (speakerActive) wsClient->clearThinking();
    bool thinking = wsClient->isThinking();
    // Single hard gate covering the WHOLE turn, gapless. Each signal owns one segment:
    // thinking = STT-final→busy latency, busy = whole turn, speakerActive = playback tail.
    // mic→STT streams and the wake word re-arms ONLY when this is false — i.e. only during
    // LISTENING, the kid's turn. Random speech in any other state is never streamed.
    bool turnGate = speakerActive || thinking || busy;

    // Turn just ENDED (playback fully drained: speakerActive + thinking + busy all clear).
    // Reopen the LISTENING window from THIS instant — when audio actually stopped — so the
    // kid can answer Claude's question for the full TTL WITHOUT re-saying "Alexa". Done
    // board-local on purpose: the voice server refreshes activation when it finishes SENDING
    // the last chunk, which can precede real playback end (buffering / clock skew) and let
    // the window lapse mid-answer. The board owns the true playback-end, so it owns this.
    if (prevTurnGate && !turnGate)
    {
      wakeDeadlineMs = now_ms + ACTIVE_WINDOW_MS;
    }
    prevTurnGate = turnGate;

    // Wake handling, guarded twice:
    //   • !turnGate — Snowboy is NOT half-duplex-gated, so TTS leaking into the mic fires
    //     false "Alexa" during a turn. Without this each false wake re-arms ON_WAKE forever
    //     (echo storm) and the kid's real speech never gets through. Gating on the full
    //     turnGate also suppresses a stray wake during the brief THINKING phase.
    //   • prevLedState != ON_WAKE — re-triggering a live ON_WAKE makes state_machine_update
    //     pthread_join a thread whose `while (curr_state == ON_WAKE)` is still true → it
    //     never exits → main loop deadlocks. Dedup keeps us from re-entering our own state.
    if (wakeWordIndex >= 1 && !turnGate && prevLedState != ON_WAKE)
    {
      isWakeWordDetected = true;
      wsClient->isTranscribed(false);
      detectTime = SteadyClock::now();
      // Open the activation window locally so the command after "Alexa" streams now.
      wakeDeadlineMs = now_ms + ACTIVE_WINDOW_MS;
      verbose(VV_INFO, stdout, "Wake word is detected, direction = %d.",
              respeakerCore->soundDirection());
      // Echo-Dot-style wake+DOA animation; hold it briefly before steady LED resumes.
      changePixelRingState(ON_WAKE);
      prevLedState = ON_WAKE;
      wakeLedUntilMs = now_ms + WAKE_LED_HOLD_MS;
    }

    // Hold the wake/DOA animation for its window; don't let the steady logic yank it away.
    if (now_ms >= wakeLedUntilMs)
    {
      // SPEAK = audio playing; LISTEN(comet) = THINKING (busy, no audio yet); else IDLE.
      STATE targetLed = speakerActive ? ON_SPEAK : ((thinking || busy) ? ON_LISTEN : ON_IDLE);
      if (targetLed != prevLedState)
      {
        changePixelRingState(targetLed);
        prevLedState = targetLed;
      }
    }

    // Wake-gated half-duplex window: stream mic→STT only while the activation window is
    // open (now < local wake deadline OR the server-pushed deadline) AND no turn is in
    // flight (turnGate). When inactive the mic is fully off — no STT, no GPU, no spurious
    // replies, AEC stays quiet between sessions. Say "Alexa" to re-open.
    long long activeUntilMs = wakeDeadlineMs;
    long long serverUntilMs = WsTransport::activeUntilMs();
    if (serverUntilMs > activeUntilMs) activeUntilMs = serverUntilMs;
    bool active = now_ms < activeUntilMs;
    // Streaming ⟺ it's the kid's turn: window open AND no turn in flight. This is the
    // SINGLE condition under which mic audio reaches STT — nothing leaks in any other state.
    bool streaming = active && !turnGate;
    // Mic gate just CLOSED. Flush STT so a partial captured in the last pre-gate chunks
    // can't bleed into the NEXT turn's transcript. TWO distinct closes, TWO frames:
    //   • turn started (turnGate) → `end`: finalize the partial; voice 409-drops it (a
    //     turn is already running) — safe, and clears STT's buffer.
    //   • window lapsed (→ ASLEEP) → `reset`: SILENT drop. Must NOT `end` here — voice is
    //     idle, so a finalize would POST /prompt and spawn an unwanted turn on a fragment.
    if (prevStreaming && !streaming && wsClient->isConnected())
    {
      wsClient->sendText(turnGate ? "{\"type\":\"end\"}" : "{\"type\":\"reset\"}");
    }
    prevStreaming = streaming;
    if (wakeWordIndex < 1 && streaming && wsClient->isConnected())
    {
      wsClient->send(audioChunk);
    }
  }

  respeakerCore->stopAudioProcessing();
  cleanup(EXIT_SUCCESS);
}
