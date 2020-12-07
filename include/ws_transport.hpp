#ifndef WS_TRANSPORT_HPP
#define WS_TRANSPORT_HPP

#define WS_PING_INTERVAL 45
#define WS_CONNECTION_TIMEOUT 5000
#define MICRO_TIMEOUT 1

/**
 * See WebSocket docs: https://machinezone.github.io/IXWebSocket/
 */
extern "C"
{
#include "verbose.h"
}
#include <ixwebsocket/IXWebSocket.h>
#include "json.hpp"
#include <chrono>

using namespace std;
using json = nlohmann::json;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

class WsTransport
{
private:
  ix::WebSocket client;
  bool _isConnected;
  bool _isTranscribeReceived;

public:
  WsTransport();
  bool connect(string wsAddress);
  void disconnect();
  void send(string audioChunk);
  bool isConnected();
  bool isTranscribeReceived();
  void isTranscribed(bool state);
};

#endif
