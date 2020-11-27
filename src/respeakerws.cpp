#include "respeakerws.hpp"

using namespace std;
using namespace respeaker;

using json = nlohmann::json;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

// MQTT settings
string mqttAddress;
string mqttUser;
string mqttPassword;
string wakeTopic;
string sleepTopic;
MQTTAsync mqttClient;
MQTTAsync_message mqttMessage = MQTTAsync_message_initializer;
atomic<bool> isMqttDisconnected(false);

// WebSocket settings
string wsAddress;
ix::WebSocket wsClient;
atomic<bool> isWsConnected(false);

// Respeaker settings
string kwsPath("/usr/share/respeaker/snowboy/resources/");
string kwsResourcesPath = kwsPath + "common.res";
string kwsModelPath;
string kwsSensitivityLevel;
string inputSource("default");
int listeningTimeout;
int wakeWordDetectionOffset;
int gainLevel;

// Common flow flags
atomic<bool> isTranscribeReceived(false);
atomic<bool> isWakeWordDetected(false);
// It's not atomic as we have to pass bool reference to respeaker API
static bool shouldStopListening = false;

/**
 * Read key configuration from JSON file for better flexibility.
 */
void readConfig()
{
  ifstream jStream(CONFIG_FILE);
  try
  {
    json config = json::parse(jStream);
    mqttAddress = config["mqttAddress"];
    mqttUser = config["mqttUser"];
    mqttPassword = config["mqttPassword"];
    wakeTopic = config["wakeTopic"];
    sleepTopic = config["sleepTopic"];
    wsAddress = config["wsAddress"];
    string modelName = config["kwsModelName"];
    kwsModelPath = kwsPath + modelName;
    kwsSensitivityLevel = to_string(config["kwsSensitivity"]);
    listeningTimeout = config["listeningTimeout"];
    wakeWordDetectionOffset = config["wakeWordDetectionOffset"];
    gainLevel = config["gainLevel"];
  }
  catch (const exception &ex)
  {
    cout << ex.what() << endl;
    if (jStream.is_open())
    {
      jStream.close();
    }
  }
}

/**
 * Make sure we correctly handle exit signals to close resouces.
 */
void handleQuit(int signal)
{
  cerr << "\nCaught signal " << signal << ", terminating..." << endl;
  shouldStopListening = true;
  isTranscribeReceived = false;
  isWakeWordDetected = false;
  isWsConnected = false;
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
 * A set of MQTT handlers.
 */
void onMqttDisconnect(void *context, MQTTAsync_successData *response)
{
  cout << "\nDisconnected from MQTT broker" << endl;
  isMqttDisconnected = true;
}

void onMqttConnectionFailure(void *context, MQTTAsync_failureData *response)
{
  cout << "\nMQTT connection failed" << endl;
  exit(EXIT_FAILURE);
}

void onMqttConnectionLost(void *context, char *cause)
{
  cout << "\nMQTT connection lost. Reconnecting..." << endl;
}

void onMqttConnect(void *context, char *cause)
{
  cout << "\nConnected to MQTT broker" << endl;
}

void sendMqttMessage(const char *topic, int angle = -1)
{
  auto payload = (angle >= 0 ? to_string(angle) : "").c_str();
  mqttMessage.payload = const_cast<char *>(payload);
  mqttMessage.payloadlen = strlen(payload);
  mqttMessage.qos = MQTT_QOS;
  mqttMessage.retained = 0;
  MQTTAsync_sendMessage(mqttClient, topic, &mqttMessage, NULL);
}

void configureMqttClient()
{
  MQTTAsync_create(&mqttClient, mqttAddress.c_str(), MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  MQTTAsync_setCallbacks(mqttClient, NULL, onMqttConnectionLost, NULL, NULL);
  MQTTAsync_setConnected(mqttClient, NULL, onMqttConnect);
}

void startMqttClient()
{
  MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
  opts.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
  opts.cleansession = 1;
  opts.automaticReconnect = 1;
  opts.context = mqttClient;
  opts.username = mqttUser.c_str();
  opts.password = mqttPassword.c_str();
  opts.onFailure = onMqttConnectionFailure;
  MQTTAsync_connect(mqttClient, &opts);
}

void stopMqttClient()
{
  int rc;
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.onSuccess = onMqttDisconnect;

  TimePoint disconnectTime = SteadyClock::now();
  if ((rc = MQTTAsync_disconnect(mqttClient, &opts)) != MQTTASYNC_SUCCESS)
  {
    cout << "\nFailed to start disconnecting" << endl;
    exit(EXIT_FAILURE);
  }

  // Mqtt might not be disconnect at this point yet, so we have to wait until a corresponding flag is set in disconnect handler or timeout occured.
  while (!isMqttDisconnected.load() || (SteadyClock::now() - disconnectTime > chrono::milliseconds(listeningTimeout)))
  {
    this_thread::sleep_for(chrono::seconds(1));
  }

  MQTTAsync_destroy(&mqttClient);
}

/**
 *  See WebSocket docs: https://machinezone.github.io/IXWebSocket/
 */
void configureWSClient()
{
  wsClient.setUrl(wsAddress);
  wsClient.setPingInterval(WS_PING_INTERVAL);
  wsClient.disablePerMessageDeflate();
  wsClient.setOnMessageCallback([](const ix::WebSocketMessagePtr &msg) {
    auto type = msg->type;

    if (type == ix::WebSocketMessageType::Message)
    {
      // When we receive a final transcibe from Vosk server, it'll contain "result" and "text" props.
      auto payload = json::parse(msg->str);
      auto text = payload["text"];
      auto result = payload["result"];

      if (result != nullptr && !text.empty())
      {
        cout << "\nTranscribe: " << text << endl;
        isTranscribeReceived = true;
      }
    }
    else if (type == ix::WebSocketMessageType::Open)
    {
      cout << "\nConnected to ASR server" << endl;
      isWsConnected = true;
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      cout << "\nDisconnected from ASR server" << endl;
      isWsConnected = false;
    }
  });
}

int main(int argc, char *argv[])
{
  configureSignalHandler();
  // ToDo: verify if all the required variables are set.
  readConfig();

  configureWSClient();
  configureMqttClient();

  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
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

  if (!respeaker->Start(&shouldStopListening))
  {
    cout << "\nCan not start the respeaker node chain." << endl;
    return -1;
  }

  int wakeWordIndex = 0, angle = 0;
  TimePoint detectTime;
  string audioChunk;

  size_t channels = respeaker->GetNumOutputChannels();
  int rate = respeaker->GetNumOutputRate();
  cout << "\nChannels: " << channels << ". Rate: " << rate << endl;

  // Start MQTT and WS clients before the actual audio data processing.
  startMqttClient();
  wsClient.start();

  while (!shouldStopListening)
  {
    audioChunk = respeaker->DetectHotword(wakeWordIndex);

    // When wake word is detected, we save a timestamp for further usage.
    if (wakeWordIndex >= 1)
    {
      isWakeWordDetected = true;
      isTranscribeReceived = false;
      detectTime = SteadyClock::now();
      angle = respeaker->GetDirection();
      cout << "\nWake word is detected. Angle: " << angle << endl;
      sendMqttMessage(wakeTopic.c_str(), angle);
    }

    // Wait for some time before audio chunks sending to WS server. Otherwise, wake word might be transribed as well.
    if (isWakeWordDetected.load() && isWsConnected.load() && (SteadyClock::now() - detectTime) > chrono::milliseconds(wakeWordDetectionOffset))
    {
      wsClient.sendBinary(audioChunk);
    }

    // Reset wake word detection flag when wait timeout occurs or if we received a final transcribe from WS server.
    if (isWakeWordDetected.load() && ((SteadyClock::now() - detectTime) > chrono::milliseconds(listeningTimeout) || isTranscribeReceived.load()))
    {
      isWakeWordDetected = false;
      isTranscribeReceived = false;
      sendMqttMessage(sleepTopic.c_str());
    }
  }

  cout << "\nStopping worker threads..." << endl;
  respeaker->Stop();
  stopMqttClient();
  wsClient.stop();

  return 0;
}
