#include "main.hpp"

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

  int wakeWordIndex = 0, direction = 0;
  TimePoint detectTime;
  string audioChunk;

  while (!shouldStopListening && trackPixelRingState())
  {
    audioChunk = respeakerCore->processAudio(wakeWordIndex);

    if (wakeWordIndex >= 1)
    {
      isWakeWordDetected = true;
      wsClient->isTranscribed(false);
      detectTime = SteadyClock::now();
      direction = respeakerCore->soundDirection();
      verbose(VV_INFO, stdout, "Wake word is detected, direction = %d.", direction);
      changePixelRingState(TO_UNMUTE);
    }

    // Always-stream mode: server does VAD/endpointing on /stt/vosk.
    // Wake-word detection above is kept for DOA logging + LED feedback only.
    if (wakeWordIndex < 1 && wsClient->isConnected())
    {
      wsClient->send(audioChunk);
    }
  }

  respeakerCore->stopAudioProcessing();
  cleanup(EXIT_SUCCESS);
}
