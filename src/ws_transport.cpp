#include "ws_transport.hpp"

WsTransport::WsTransport(string location) {
  _isTranscribeReceived = false;
  _isConnected = false;
  _location = location;
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
      try
      {
        auto message = msg->str;

        if (!message.empty())
        {
          auto payload = json::parse(message);
          auto partial = payload.value("partial", "");
          auto text = payload.value("text", "");

          if (!text.empty())
          {
            logInfo("Transcribe: {}", text);
            this->_isTranscribeReceived = true;
          }
          else
          {
            if (!partial.empty())
            {
              logInfo("Partial: {}", partial);
            }
            this->_isTranscribeReceived = false;
          }
        }
      }
      catch (json::parse_error& pe)
      {
        logInfo("JSON parse error: {}", pe.what());
        this->_isTranscribeReceived = false;
      }
      catch(nlohmann::detail::type_error& te)
      {
        logInfo("TypeError: {}", te.what());
        this->_isTranscribeReceived = false;
      }
    }
    else if (type == ix::WebSocketMessageType::Open)
    {
      logInfo("Connected to ASR server");
      this->_isConnected = true;

      // Send default device location to server
      json response;
      response["location"] = this->_location;
      client.sendText(response.dump());
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      logInfo("Disconnected from ASR server: code - {}, reason - {}", msg->closeInfo.code, msg->closeInfo.reason);
      this->_isConnected = false;
    }
    else if (type == ix::WebSocketMessageType::Error)
    {
      logInfo(
        "Error: {}. Retries: {}. Wait time(ms): {}. HTTP Status: {}.",
        msg->errorInfo.reason,
        msg->errorInfo.retries,
        msg->errorInfo.wait_time,
        msg->errorInfo.http_status
      );
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
