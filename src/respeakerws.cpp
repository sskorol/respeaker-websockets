#include <cstring>
#include <memory>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

// Audio DSP provided by Alango: https://wiki.seeedstudio.com/ReSpeaker_Core_v2.0/#closed-source-solution
#include <respeaker.h>
#include <chain_nodes/pulse_collector_node.h>
#include <chain_nodes/vep_aec_beamforming_node.h>
#include <chain_nodes/snowboy_mb_doa_kws_node.h>

// WS client should be built locally first: https://machinezone.github.io/IXWebSocket/build/
#include <ixwebsocket/IXWebSocket.h>

// Required for parsing ASR transcribe. See JSON lib docs: https://github.com/nlohmann/json
#include "json.hpp"

using namespace std;
using namespace respeaker;
using json = nlohmann::json;
using SteadyClock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<SteadyClock>;

#define BLOCK_SIZE_MS 8
#define LISTENING_TIMEOUT 8000
#define HOT_WORD_DETECTION_OFFSET 300
#define SOCKET_PING_INTERVAL 45
#define GAIN_LEVEL 10

// WebSocket settings
ix::WebSocket wsClient;
std::string url("ws://localhost:2700/");
static bool isTranscribeReceived = false;
static bool isWsConnected = false;

// Respeaker settings
std::string kwsPath("/usr/share/respeaker/snowboy/resources/");
std::string kwsResourcesPath = kwsPath + "common.res";
std::string kwsModelPath = kwsPath + "snowboy.umdl";
std::string kwsSensitivityLevel("0.6");
std::string inputSource("default");

static bool shouldStopListening = false;

void handleQuit(int signal)
{
  cerr << "Caught signal " << signal << ", terminating..." << endl;
  shouldStopListening = true;
  isTranscribeReceived = false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * Make sure we correctly handle exit signals to close resouces.
 */
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
 *  See WebSocket docs: https://machinezone.github.io/IXWebSocket/
 */
void configureWebSocket()
{
  wsClient.setUrl(url);
  wsClient.setPingInterval(SOCKET_PING_INTERVAL);
  wsClient.disablePerMessageDeflate();
  wsClient.setOnMessageCallback([](const ix::WebSocketMessagePtr &msg) {
    auto type = msg->type;

    if (type == ix::WebSocketMessageType::Message)
    {
      // When we receive a final transcibe, it'll contain "result" and "text" props.
      auto payload = json::parse(msg->str);
      auto text = payload["text"];
      auto result = payload["result"];

      if (result != nullptr && !text.empty())
      {
        cout << endl
             << "Transcribe: " << text << endl;
        isTranscribeReceived = true;
      }
    }
    else if (type == ix::WebSocketMessageType::Open)
    {
      cout << "Connected to WS server" << endl;
      isWsConnected = true;
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      cout << "Disconnected from WS server" << endl;
      isWsConnected = false;
    }
  });
}

int main(int argc, char *argv[])
{
  configureSignalHandler();
  configureWebSocket();

  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
  unique_ptr<PulseCollectorNode> collectorNode;
  unique_ptr<VepAecBeamformingNode> beamformingNode;
  unique_ptr<SnowboyMbDoaKwsNode> hotwordNode;
  unique_ptr<ReSpeaker> respeaker;

  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, false, 6, false));
  hotwordNode.reset(SnowboyMbDoaKwsNode::Create(kwsResourcesPath, kwsModelPath, kwsSensitivityLevel, 10, true));

  hotwordNode->DisableAutoStateTransfer();
  hotwordNode->SetAgcTargetLevelDbfs(GAIN_LEVEL);

  // Create audio DSP chain: convert Pulse audio to 16k -> do beamforming, AEC, NR -> detect hotword
  beamformingNode->Uplink(collectorNode.get());
  hotwordNode->Uplink(beamformingNode.get());

  respeaker.reset(ReSpeaker::Create());
  respeaker->RegisterChainByHead(collectorNode.get());
  respeaker->RegisterOutputNode(hotwordNode.get());
  respeaker->RegisterDirectionManagerNode(hotwordNode.get());
  respeaker->RegisterHotwordDetectionNode(hotwordNode.get());

  if (!respeaker->Start(&shouldStopListening))
  {
    cout << "Can not start the respeaker node chain." << endl;
    return -1;
  }

  int hotwordIndex = 0, angle = 0;
  bool isHotwordDetected = false;
  TimePoint detectTime;
  string audioChunk;

  size_t channels = respeaker->GetNumOutputChannels();
  int rate = respeaker->GetNumOutputRate();
  cout << "Channels: " << channels << ". Rate: " << rate << endl;

  // Start WebSocket client before the actual audio data processing.
  wsClient.start();

  while (!shouldStopListening)
  {
    audioChunk = respeaker->DetectHotword(hotwordIndex);

    // When hotword is detected, we save a timestamp for further usage.
    if (hotwordIndex >= 1)
    {
      isTranscribeReceived = false;
      isHotwordDetected = true;
      detectTime = SteadyClock::now();
      angle = respeaker->GetDirection();
      cout << endl
           << "Hotword detected. Angle: " << angle << endl;
    }

    // Wait for some time before audio chunks sending to WS server. Otherwise, hotword might be transribed as well.
    if (isHotwordDetected && isWsConnected && (SteadyClock::now() - detectTime) > std::chrono::milliseconds(HOT_WORD_DETECTION_OFFSET))
    {
      wsClient.sendBinary(audioChunk);
    }
    else
    {
      // Visualize when we don't send audio to WS server.
      cout << "." << flush;
    }

    // Reset hotword detection flag when wait timeout occurs or if we received a final transcribe from WS server.
    if ((SteadyClock::now() - detectTime) > std::chrono::milliseconds(LISTENING_TIMEOUT) || isTranscribeReceived)
    {
      isHotwordDetected = false;
    }
  }

  cout << "Stopping the respeaker and WS worker thread..." << endl;
  respeaker->Stop();
  wsClient.stop();

  return 0;
}
