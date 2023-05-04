#include "main.hpp"

void setAlsaMasterVolume(long volume)
{
  try
  {
    long min, max;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "Master";

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

    snd_mixer_close(handle);
  }
  catch (const std::exception &e)
  {
    logInfo("Error: ", e.what());
  }
}

string runUnixCommandAndCaptureOutput(string cmd)
{
  char buffer[128];
  string result = "";
  FILE *pipe = popen(cmd.c_str(), "r");

  if (pipe)
  {
    try
    {
      while (!feof(pipe))
      {
        if (fgets(buffer, 128, pipe) != NULL)
          result += buffer;
      }
    }
    catch (...)
    {
      logInfo("Unable to execute command: {}", cmd);
    }
    pclose(pipe);
  }

  return result;
}

void enablePixelRing(Config *config)
{
  setupPixelRing(config);

  if (-1 == setPowerPin() || -1 == cAPA102_Init(RUNTIME.LEDs, GLOBAL_BRIGHTNESS))
  {
    cleanup(EXIT_FAILURE);
  }

  RUNTIME.curr_state = RUNTIME.if_mute ? TO_MUTE : TO_UNMUTE;
  state_machine_update();

  // It makes no sense to continue if WS is unavailable.
  wsClient = new WsTransport(config->room());
  if (!wsClient->connect(config->webSocketAddress()))
  {
    logInfo("Unable to connect to WS server. Quitting...");
    cleanup(EXIT_FAILURE);
  }
}

/**
 * Exit program and release all the resources.
 */
void cleanup(int status)
{
  if (wsClient != nullptr)
  {
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
  shouldStopListening = true;
  RUNTIME.if_terminate = 1;
  // pthread_cancel(RUNTIME.curr_thread);
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
  registerLoggers();
  logInfo("---> Starting new session...");
  configureSignalHandler();
  setVerbose(VV_INFO);

  config = new Config(CONFIG_FILE);
  if (!config->isRead())
  {
    logInfo("Unable to read json config. Quitting...");
    exit(EXIT_FAILURE);
  }

  respeakerCore = new RespeakerCore(config);
  if (!respeakerCore->startListening(&shouldStopListening))
  {
    logInfo("Unable to start the respeaker node chain. Quitting...");
    cleanup(EXIT_FAILURE);
  }
  else
  {
    enablePixelRing(config);
    logInfo("Press CTRL-C to exit");
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
      logInfo("Wake word is detected, direction = {}.", direction);
      changePixelRingState(TO_UNMUTE);
      runUnixCommandAndCaptureOutput("pactl set-sink-volume 0 20%");
    }

    // Skip the chunk with a hotword to avoid sending it for transciption.
    if (isWakeWordDetected && wakeWordIndex < 1 && wsClient->isConnected())
    {
      wsClient->send(audioChunk);
    }

    // Reset wake word detection flag when wait timeout occurs or if we received a final transcribe from WS server.
    if (isWakeWordDetected && ((SteadyClock::now() - detectTime) > chrono::milliseconds(config->listeningTimeout()) || wsClient->isTranscribeReceived()))
    {
      isWakeWordDetected = false;
      wsClient->isTranscribed(false);
      changePixelRingState(TO_MUTE);
      runUnixCommandAndCaptureOutput("pactl set-sink-volume 0 70%");
    }
  }

  respeakerCore->stopAudioProcessing();
  cleanup(EXIT_SUCCESS);
}
