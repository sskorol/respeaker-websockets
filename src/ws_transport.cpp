#include "ws_transport.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
constexpr const char *kSpeakingUntilFile = "/tmp/respeaker_speaking_until_ms";
constexpr const char *kThinkingClearFile = "/tmp/respeaker_thinking_clear_ms";
constexpr const char *kActiveUntilFile = "/tmp/respeaker_active_until_ms";
constexpr const char *kBusyUntilFile = "/tmp/respeaker_busy_until_ms";

long long readEpochMsFile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr) return 0;
    long long v = 0;
    int parsed = fscanf(f, "%lld", &v);
    fclose(f);
    return (parsed == 1 && v > 0) ? v : 0;
}
}

WsTransport::WsTransport()
{
  _isTranscribeReceived = false;
  _isConnected = false;
  _thinkingSetMs = 0;
}

bool WsTransport::connect(string wsAddress)
{
  client.setUrl(wsAddress);
  client.setPingInterval(WS_PING_INTERVAL);
  client.disablePerMessageDeflate();
  // Cap ixwebsocket's auto-reconnect backoff. A long dev-box outage otherwise retries with
  // an unbounded-growing wait; the 10 s ceiling bounds worst-case dead-air on this 1 GB
  // no-swap board. (This vendored 11.0.4 lacks setMinWaitBetweenReconnectionRetries; the
  // speaker process uses a manual fresh-object loop for the harsher flap case.)
  client.setMaxWaitBetweenReconnectionRetries(10000);
  client.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
    auto type = msg->type;

    if (type == ix::WebSocketMessageType::Message)
    {
      // Typed JSON events from STT (`/stt/stream`): SttFinal, SttPartial, SttDropped.
      // Wire shape mirrors shared/protocol.py — discriminated by `type`. STT POSTs voice
      // /prompt itself; the board only needs these events for LED feedback.
      auto payload = json::parse(msg->str, nullptr, false);
      if (payload.is_discarded() || !payload.contains("type"))
      {
        return;
      }
      const std::string frameType = payload["type"].get<std::string>();
      if (frameType == "final")
      {
        const std::string text = payload.value("text", "");
        verbose(VV_INFO, stdout, "Final: %s", text.c_str());
        this->_isTranscribeReceived = true;
        // Mark thinking-set timestamp. Clear paths (no cap, event-driven):
        //   • Speaker rising edge → clearThinking() from main loop.
        //   • respeaker_speaker writes thinking-clear file > this value when voice
        //     sends an `interrupted` text frame (turn cancelled, no playback).
        using namespace std::chrono;
        long long now_ms = duration_cast<milliseconds>(
                               system_clock::now().time_since_epoch())
                               .count();
        this->_thinkingSetMs = now_ms;
      }
      else if (frameType == "dropped")
      {
        verbose(VV_INFO, stdout, "Dropped: reason=%s",
                payload.value("reason", "").c_str());
      }
    }
    else if (type == ix::WebSocketMessageType::Open)
    {
      verbose(VV_INFO, stdout, "Connected to STT server");
      this->_isConnected = true;
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      verbose(VV_INFO, stdout, "Disconnected from STT server");
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

void WsTransport::sendText(const string &text)
{
  if (_isConnected) {
    client.sendText(text);
  }
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

bool WsTransport::isThinking() {
  long long set_ms = _thinkingSetMs.load();
  if (set_ms <= 0) return false;
  // External cancel: respeaker_speaker bumps thinking-clear-ms on `interrupted`.
  long long clear_ms = readEpochMsFile(kThinkingClearFile);
  if (clear_ms > set_ms) return false;
  // Safety cap: voice's 409 paths (turn-in-progress, self-echo) never trigger
  // playback or `interrupted`, so without this cap _thinkingSetMs sticks forever
  // and the mic→WS gate stays closed. Self-clear after THINKING_MAX_AGE_MS.
  using namespace std::chrono;
  long long now_ms = duration_cast<milliseconds>(
                         system_clock::now().time_since_epoch())
                         .count();
  if (now_ms - set_ms > THINKING_MAX_AGE_MS) {
    _thinkingSetMs = 0;
    return false;
  }
  return true;
}

void WsTransport::clearThinking() {
  _thinkingSetMs = 0;
}

bool WsTransport::isSpeakerActive() {
  long long deadline_ms = readEpochMsFile(kSpeakingUntilFile);
  if (deadline_ms <= 0) return false;
  using namespace std::chrono;
  long long now_ms = duration_cast<milliseconds>(
                         system_clock::now().time_since_epoch())
                         .count();
  return now_ms < deadline_ms;
}

long long WsTransport::activeUntilMs() {
  return readEpochMsFile(kActiveUntilFile);
}

// Hard turn-busy gate: true for the whole turn (THINKING + SPEAKING) while voice holds
// the deadline. Single authoritative signal — no inter-chunk gaps like speaking_until.
bool WsTransport::isBusy() {
  long long deadline_ms = readEpochMsFile(kBusyUntilFile);
  if (deadline_ms <= 0) return false;
  using namespace std::chrono;
  long long now_ms = duration_cast<milliseconds>(
                         system_clock::now().time_since_epoch())
                         .count();
  return now_ms < deadline_ms;
}
