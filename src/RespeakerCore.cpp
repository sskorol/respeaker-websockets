#include "RespeakerCore.hpp"

// Respeaker settings
string kwsPath("/usr/share/respeaker/snowboy/resources/");
string kwsResourcesPath = kwsPath + "common.res";
string inputSource("default");

// Common flow flags
static bool isWakeWordDetected = false;
static bool shouldStopListening = false;

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
  cout << "\nCaught signal " << signal << ", terminating..." << endl;
  shouldStopListening = true;
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

/**
 * Setup Mqtt and WS clients.
 */
bool setupTransports(json config) {
  bool shouldContinue = true;

  if (!connectToMqtt(config)) {
    cerr << "\nUnable to connect to MQTT broker." << endl;
    shouldContinue = false;
  }
  
  if (!connectToWS(config)) {
    cerr << "\nUnable to connect to WS server." << endl;
    shouldContinue = false;
  }

  return shouldContinue;
}

void cleanupTransports() {
  if (isMqttConnected) {
    disconnectFromMqtt();
  }

  if (isWsConnected) {
    disconnectFromWS();
  }
}

int main(int argc, char *argv[])
{
  configureSignalHandler();

  // ToDo: verify if all the required variables are set.
  json config = readConfig();
  if (config.is_discarded()) {
    cerr << "\nUnable to read json config. Quitting..." << endl;
    return -1;
  }

  // It makes no sense to continue if Mqtt or WS is unavailable.
  if (!setupTransports(config)) {
    cerr << "Unable to complete setup. Quitting..." << endl;
    cleanupTransports();
    return -1;
  }

  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
  string modelName = config["kwsModelName"];
  string kwsModelPath = kwsPath + modelName;
  string kwsSensitivityLevel = to_string(config["kwsSensitivity"]);
  int listeningTimeout = config["listeningTimeout"];
  int wakeWordDetectionOffset = config["wakeWordDetectionOffset"];
  int gainLevel = config["gainLevel"];

  unique_ptr<PulseCollectorNode> collectorNode;
  unique_ptr<VepAecBeamformingNode> beamformingNode;
  unique_ptr<SnowboyMbDoaKwsNode> hotwordNode;
  unique_ptr<ReSpeaker> respeaker;

  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, false, 6, false));
  hotwordNode.reset(SnowboyMbDoaKwsNode::Create(kwsResourcesPath, kwsModelPath, kwsSensitivityLevel, 10, true));

  hotwordNode->DisableAutoStateTransfer();
  hotwordNode->SetAgcTargetLevelDbfs(gainLevel);

  // Create audio DSP chain: convert Pulse audio to 16k -> do beamforming, AEC, NR -> detect hotword.
  beamformingNode->Uplink(collectorNode.get());
  hotwordNode->Uplink(beamformingNode.get());

  respeaker.reset(ReSpeaker::Create());
  respeaker->RegisterChainByHead(collectorNode.get());
  respeaker->RegisterOutputNode(hotwordNode.get());
  respeaker->RegisterDirectionManagerNode(hotwordNode.get());
  respeaker->RegisterHotwordDetectionNode(hotwordNode.get());

  // If we couldn't start respeaker chain, we have to cleanup transports.
  if (!respeaker->Start(&shouldStopListening))
  {
    cerr << "\nUnable to start the respeaker node chain. Quitting..." << endl;
    cleanupTransports();
    return -1;
  } else {
    size_t channels = respeaker->GetNumOutputChannels();
    int rate = respeaker->GetNumOutputRate();
    cout << "\nChannels: " << channels << ". Rate: " << rate << endl;
  }

  int wakeWordIndex = 0, angle = 0;
  TimePoint detectTime;
  string audioChunk;

  while (!shouldStopListening && isMqttConnected && isWsConnected)
  {
    audioChunk = respeaker->DetectHotword(wakeWordIndex);

    // When wake word is detected, we save a timestamp for further usage.
    if (wakeWordIndex >= 1 && isMqttConnected)
    {
      isWakeWordDetected = true;
      isTranscribeReceived = false;
      detectTime = SteadyClock::now();
      angle = respeaker->GetDirection();
      cout << "\nWake word is detected. Angle: " << angle << endl;
      sendMqttMessage(MQTT_WAKE_TOPIC, angle);
    }

    // Wait for some time before audio chunks' sending to WS server. Otherwise, wake word might be transribed as well.
    if (isWakeWordDetected && isWsConnected && (SteadyClock::now() - detectTime) > chrono::milliseconds(wakeWordDetectionOffset))
    {
      sendAudioChunk(audioChunk);
    }

    // Reset wake word detection flag when wait timeout occurs or if we received a final transcribe from WS server.
    if (isWakeWordDetected && isMqttConnected && ((SteadyClock::now() - detectTime) > chrono::milliseconds(listeningTimeout) || isTranscribeReceived))
    {
      isWakeWordDetected = false;
      isTranscribeReceived = false;
      sendMqttMessage(MQTT_SLEEP_TOPIC);
    }
  }

  cout << "\nStopping worker threads..." << endl;
  respeaker->Stop();
  cleanupTransports();

  return 0;
}
