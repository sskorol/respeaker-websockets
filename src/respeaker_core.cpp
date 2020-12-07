#include "respeaker_core.hpp"

/**
 * Setup main resources.
 */
void setup(json config)
{
  setupPixelRing(config);

  if (-1 == setPowerPin())
    cleanup(EXIT_FAILURE);

  RUNTIME.curr_state = RUNTIME.if_mute ? TO_MUTE : TO_UNMUTE;
  RUNTIME.if_update = 1;

  if (-1 == cAPA102_Init(RUNTIME.LEDs.number,
                         RUNTIME.LEDs.spi_bus,
                         RUNTIME.LEDs.spi_dev,
                         GLOBAL_BRIGHTNESS))
    cleanup(EXIT_FAILURE);

  // It makes no sense to continue if WS is unavailable.
  wsClient = new WsTransport();
  if (!wsClient->connect(config[C_WS_ADDRESS_STR]))
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
 * Read key configuration from JSON file for better flexibility.
 */
json readConfig()
{
  ifstream jStream(CONFIG_FILE);
  json config = json::parse(jStream, nullptr, false);
  jStream.close();
  return config;
}

/**
 * Make sure we correctly handle exit signals to close resouces.
 */
void handleQuit(int signal)
{
  verbose(VV_INFO, stdout, "Caught signal %d. Terminating...", signal);
  shouldStopListening = true;
  RUNTIME.if_terminate = 1;
  this_thread::sleep_for(chrono::seconds(1));
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
  setVerbose(VV_INFO);

  // ToDo: verify if all the required variables are set.
  json config = readConfig();
  if (config.is_discarded())
  {
    verbose(VV_INFO, stdout, "Unable to read json config. Quitting...");
    exit(EXIT_FAILURE);
  }

  configureSignalHandler();
  setup(config);

  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
  string modelName = config[C_RESPEAKER_STR][RSP_KWS_MODEL_STR];
  string kwsModelPath = kwsPath + modelName;
  string kwsSensitivityLevel = to_string(config[C_RESPEAKER_STR][RSP_KWS_SENSITIVITY_STR]);
  int listeningTimeout = config[C_RESPEAKER_STR][RSP_LISTENING_TIMEOUT_STR];
  int wakeWordDetectionOffset = config[C_RESPEAKER_STR][RSP_WAKEWORD_DETECTION_OFFSET_STR];
  int gainLevel = config[C_RESPEAKER_STR][RSP_GAIN_LEVEL_STR];

  unique_ptr<PulseCollectorNode> collectorNode;
  unique_ptr<VepAecBeamformingNode> beamformingNode;
  // ToDo: investigate why multi-beam kws node doesn't work
  unique_ptr<Snowboy1bDoaKwsNode> hotwordNode;
  unique_ptr<ReSpeaker> respeaker;

  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, true, 6, false));
  hotwordNode.reset(Snowboy1bDoaKwsNode::Create(kwsResourcesPath, kwsModelPath, kwsSensitivityLevel, 10, true));

  hotwordNode->DisableAutoStateTransfer();
  hotwordNode->SetAgcTargetLevelDbfs(gainLevel);

  // Create audio DSP chain: convert Pulse audio to 16k -> do beamforming, AEC, NR -> detect hotword.
  beamformingNode->Uplink(collectorNode.get());
  hotwordNode->Uplink(beamformingNode.get());

  respeaker.reset(ReSpeaker::Create(ERROR_LOG_LEVEL));
  respeaker->RegisterChainByHead(collectorNode.get());
  respeaker->RegisterOutputNode(hotwordNode.get());
  respeaker->RegisterDirectionManagerNode(hotwordNode.get());
  respeaker->RegisterHotwordDetectionNode(hotwordNode.get());

  // If we couldn't start respeaker chain, we have to cleanup transports.
  if (!respeaker->Start(&shouldStopListening))
  {
    verbose(VV_INFO, stdout, "Unable to start the respeaker node chain. Quitting...");
    cleanup(EXIT_FAILURE);
  }
  else
  {
    size_t channels = respeaker->GetNumOutputChannels();
    int rate = respeaker->GetNumOutputRate();
    verbose(VV_INFO, stdout, "Channels ............ %d", channels);
    verbose(VV_INFO, stdout, "Rate ................ %d", rate);
    dumpInfo();
  }

  int wakeWordIndex = 0, angle = 0;
  TimePoint detectTime;
  string audioChunk;

  while (!shouldStopListening && wsClient->isConnected())
  {
    audioChunk = respeaker->DetectHotword(wakeWordIndex);

    // When wake word is detected, we save a timestamp for further usage.
    if (wakeWordIndex >= 1)
    {
      isWakeWordDetected = true;
      wsClient->isTranscribed(false);
      detectTime = SteadyClock::now();
      angle = respeaker->GetDirection();
      verbose(VV_INFO, stdout, "Wake word is detected, angle = %d.", angle);
      changePixelRingState(TO_UNMUTE);
    }

    // Wait for some time before audio chunks' sending to WS server. Otherwise, wake word might be transribed as well.
    if (isWakeWordDetected && wsClient->isConnected() && (SteadyClock::now() - detectTime) > chrono::milliseconds(wakeWordDetectionOffset))
    {
      wsClient->send(audioChunk);
    }

    // Reset wake word detection flag when wait timeout occurs or if we received a final transcribe from WS server.
    if (isWakeWordDetected && ((SteadyClock::now() - detectTime) > chrono::milliseconds(listeningTimeout) || wsClient->isTranscribeReceived()))
    {
      isWakeWordDetected = false;
      wsClient->isTranscribed(false);
      changePixelRingState(TO_MUTE);
    }

    if (!trackPixelRingState())
    {
      break;
    }
  }

  respeaker->Stop();
  cleanup(EXIT_SUCCESS);
}
