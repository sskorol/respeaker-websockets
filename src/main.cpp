#include "main.hpp"

// Wake-word activation window. Saying "Alexa" opens it locally so the command right
// after the keyword streams immediately (no wait for the server round-trip). The voice
// server slides it forward on every accepted turn via /tmp/respeaker_active_until_ms
// (written by respeaker_speaker on an `activation` frame). 1 min, matches VOICE_ACTIVATION_TTL_S.
#define ACTIVE_WINDOW_MS (1 * 60 * 1000LL)
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

  while (!shouldStopListening && trackPixelRingState())
  {
    audioChunk = respeakerCore->processAudio(wakeWordIndex);
    long long now_ms = nowEpochMs();

    if (wakeWordIndex >= 1)
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

    // Three-state LED feedback driven by half-duplex signals:
    //   ON_SPEAK  — speaker actively playing TTS (highest precedence).
    //   ON_LISTEN — STT emitted `final`, Claude is thinking, no audio yet.
    //   ON_IDLE   — quiet, waiting for the kid.
    // Speaker rising edge clears the thinking deadline so the LED can't ghost-stay in
    // ON_LISTEN once playback starts. Thinking deadline carries a 10 s safety cap
    // inside WsTransport for the case where /prompt is 409-rejected (no playback).
    bool speakerActive = WsTransport::isSpeakerActive();
    if (speakerActive) wsClient->clearThinking();
    bool thinking = wsClient->isThinking();
    // Hold the wake/DOA animation for its window; don't let the steady logic yank it away.
    if (now_ms >= wakeLedUntilMs)
    {
      STATE targetLed = speakerActive ? ON_SPEAK : (thinking ? ON_LISTEN : ON_IDLE);
      if (targetLed != prevLedState)
      {
        changePixelRingState(targetLed);
        prevLedState = targetLed;
      }
    }

    // Wake-gated half-duplex window: stream mic→STT only while the activation window is
    // open (now < local wake deadline OR the server-pushed deadline), AND the speaker is
    // idle and Claude isn't thinking. When inactive the mic is fully off — no STT, no GPU,
    // no spurious replies, AEC stays quiet between sessions. Say "Alexa" to re-open.
    long long activeUntilMs = wakeDeadlineMs;
    long long serverUntilMs = WsTransport::activeUntilMs();
    if (serverUntilMs > activeUntilMs) activeUntilMs = serverUntilMs;
    bool active = now_ms < activeUntilMs;
    if (wakeWordIndex < 1 && active && wsClient->isConnected() && !speakerActive && !thinking)
    {
      wsClient->send(audioChunk);
    }
  }

  respeakerCore->stopAudioProcessing();
  cleanup(EXIT_SUCCESS);
}
