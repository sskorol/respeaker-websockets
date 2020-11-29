#ifndef WS_TRANSPORT_HPP
#define WS_TRANSPORT_HPP

#define WS_PING_INTERVAL 45
#define WS_CONNECTION_TIMEOUT 5000
#define MICRO_TIMEOUT 1

/**
 * See WebSocket docs: https://machinezone.github.io/IXWebSocket/
 */
#include <ixwebsocket/IXWebSocket.h>

ix::WebSocket wsClient;
static bool isWsConnected = false;
static bool isTranscribeReceived = false;

bool connectToWS(json config)
{
  string wsAddress = config["wsAddress"];
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

  wsClient.start();

  TimePoint connectTime = SteadyClock::now();
  while (!isWsConnected && (SteadyClock::now() - connectTime < chrono::milliseconds(WS_CONNECTION_TIMEOUT)))
  {
    this_thread::sleep_for(chrono::seconds(MICRO_TIMEOUT));
  }

  return isWsConnected;
}

void disconnectFromWS() {
  wsClient.stop();
}

void sendAudioChunk(string audioChunk) {
  wsClient.sendBinary(audioChunk);
}

#endif
