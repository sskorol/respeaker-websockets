#include "ws_transport.hpp"

WsTransport::WsTransport() {
  _isTranscribeReceived = false;
  _isConnected = false;
}

bool WsTransport::connect(string wsAddress)
{
  client.setUrl(wsAddress);
  client.setPingInterval(WS_PING_INTERVAL);
  client.disablePerMessageDeflate();
  client.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
    auto type = msg->type;

    if (type == ix::WebSocketMessageType::Message)
    {
      // When we receive a final transcibe from Vosk server, it'll contain "result" and "text" props.
      auto payload = json::parse(msg->str);
      auto result = payload["result"];
      string text = payload["text"];

      if (result != nullptr && !text.empty())
      {
        verbose(VV_INFO, stdout, "Transcribe: %s", text.c_str());
        this->_isTranscribeReceived = true;
      }
    }
    else if (type == ix::WebSocketMessageType::Open)
    {
      verbose(VV_INFO, stdout, "Connected to ASR server");
      this->_isConnected = true;
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      verbose(VV_INFO, stdout, "Disconnected from ASR server");
      this->_isConnected = false;
    }
  });

  client.start();

  TimePoint connectTime = SteadyClock::now();
  while (!_isConnected && (SteadyClock::now() - connectTime < chrono::milliseconds(WS_CONNECTION_TIMEOUT)))
  {
    this_thread::sleep_for(chrono::seconds(MICRO_TIMEOUT));
  }

  return _isConnected;
}

void WsTransport::disconnect()
{
  if (_isConnected) {
    client.stop();
  }
}

void WsTransport::send(string audioChunk)
{
  client.sendBinary(audioChunk);
}

bool WsTransport::isConnected() {
  return _isConnected;
}

bool WsTransport::isTranscribeReceived() {
  return _isTranscribeReceived;
}

void WsTransport::isTranscribed(bool state) {
  _isTranscribeReceived = state;
}
