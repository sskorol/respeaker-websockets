#include "ws_transport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
constexpr const char *kSpeakingUntilFile = "/tmp/respeaker_speaking_until_ms";
constexpr const char *kThinkingClearFile = "/tmp/respeaker_thinking_clear_ms";
constexpr const char *kActiveUntilFile = "/tmp/respeaker_active_until_ms";
constexpr const char *kBusyUntilFile = "/tmp/respeaker_busy_until_ms";

// Reconnect backoff bounds, mirroring respeaker_speaker. A flap (Open-then-instant-drop)
// grows the delay; a healthy session resets it. Cap = worst-case dead-air per attempt.
constexpr int kMinBackoffMs = 1000;
constexpr int kMaxBackoffMs = 10000;          // backoff 1→2→4→8→10→10…
constexpr int kConnectTimeoutMs = WS_CONNECTION_TIMEOUT;  // 5 s to reach Open
constexpr long long kHealthyMs = 10000;       // a session must last this long to count as healthy

long long nowMsEpoch()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

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

WsTransport::~WsTransport()
{
  disconnect();
}

std::unique_ptr<ix::WebSocket> WsTransport::makeSocket()
{
  auto ws = std::make_unique<ix::WebSocket>();
  ws->setUrl(_wsAddress);
  ws->setPingInterval(WS_PING_INTERVAL);
  ws->disablePerMessageDeflate();
  // We own reconnection (runReconnectLoop). ixwebsocket's built-in auto-reconnect RESETS
  // its backoff the instant a socket reaches Open, so an Open-then-instant-drop flap (e.g.
  // STT mid-restart) would hammer reconnect and peg this 1 GB no-swap board. Disable it and
  // drive a bounded manual backoff instead — same hard-won pattern as respeaker_speaker.
  ws->disableAutomaticReconnection();
  ws->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
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
        this->_thinkingSetMs = nowMsEpoch();
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
      this->_lastOpenMs.store(nowMsEpoch());
      this->_isConnected = true;
    }
    else if (type == ix::WebSocketMessageType::Close)
    {
      verbose(VV_INFO, stdout, "Disconnected from STT server");
      this->_isConnected = false;
    }
  });
  return ws;
}

void WsTransport::runReconnectLoop()
{
  int backoffMs = kMinBackoffMs;

  while (!_shouldStop.load())
  {
    // Fresh WebSocket per attempt. ixwebsocket 11.0.4 flaps when ONE object is reused across
    // stop()/start() (re-started socket reaches Open then drops within seconds), so build a
    // brand-new object each time — it then holds the connection like a fresh process.
    std::unique_ptr<ix::WebSocket> ws = makeSocket();
    _lastOpenMs.store(0);
    _isConnected = false;
    // Publish so send()/sendText() use this socket. Done before start() — sends are still
    // gated on _isConnected, which only the Open callback sets, so nothing leaks pre-Open.
    {
      std::lock_guard<std::mutex> lk(_clientMu);
      _client = ws.get();
    }
    ws->start();  // non-blocking; Open/Close arrive on the message callback

    // start() is async — wait for the attempt to RESOLVE to Open (or give up after the
    // connect timeout) before monitoring for drop.
    int waited = 0;
    while (!_shouldStop.load() && ws->getReadyState() != ix::ReadyState::Open &&
           waited < kConnectTimeoutMs)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      waited += 100;
    }
    // If we reached Open, hold the session until it drops back out of Open.
    while (!_shouldStop.load() && ws->getReadyState() == ix::ReadyState::Open)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Stop publishing the socket BEFORE tearing it down so a concurrent send() can't touch
    // a half-closed object.
    {
      std::lock_guard<std::mutex> lk(_clientMu);
      _client = nullptr;
    }
    _isConnected = false;
    ws->stop();  // fully tear down the socket + reader thread before the next attempt
    ws.reset();
    if (_shouldStop.load()) break;

    long long opened = _lastOpenMs.load();
    long long livedMs = opened > 0 ? (nowMsEpoch() - opened) : 0;
    if (livedMs >= kHealthyMs)
      backoffMs = kMinBackoffMs;  // healthy session ended → retry promptly
    else
      backoffMs = std::min(backoffMs * 2, kMaxBackoffMs);  // flapping / never connected → back off

    verbose(VV_INFO, stdout, "STT WS down (session lived %lld ms); reconnecting in %d ms",
            livedMs, backoffMs);

    // Interruptible backoff sleep so disconnect()/SIGTERM during the wait exits promptly.
    for (int slept = 0; slept < backoffMs && !_shouldStop.load(); slept += 100)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool WsTransport::connect(string wsAddress)
{
  _wsAddress = std::move(wsAddress);
  _shouldStop = false;
  _reconnectThread = std::thread(&WsTransport::runReconnectLoop, this);

  // Give the first attempt a chance to reach Open so the caller can log status — but DON'T
  // require success. The manager keeps retrying with bounded backoff and the main mic→WS
  // loop guards every send on isConnected(). (Returning false used to be treated as fatal
  // in main.cpp, crashing the asr process at boot whenever STT wasn't up yet → pm2 respawn
  // tight loop.)
  TimePoint connectTime = SteadyClock::now();
  while (!_isConnected &&
         (SteadyClock::now() - connectTime < chrono::milliseconds(WS_CONNECTION_TIMEOUT)))
  {
    this_thread::sleep_for(chrono::milliseconds(50));
  }

  return _isConnected;
}

void WsTransport::disconnect()
{
  _shouldStop = true;
  if (_reconnectThread.joinable())
  {
    _reconnectThread.join();
  }
}

void WsTransport::send(string audioChunk)
{
  std::lock_guard<std::mutex> lk(_clientMu);
  if (_isConnected.load() && _client != nullptr)
  {
    _client->sendBinary(audioChunk);
  }
}

void WsTransport::sendText(const string &text)
{
  std::lock_guard<std::mutex> lk(_clientMu);
  if (_isConnected.load() && _client != nullptr)
  {
    _client->sendText(text);
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
  long long now_ms = nowMsEpoch();
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
  return nowMsEpoch() < deadline_ms;
}

long long WsTransport::activeUntilMs() {
  return readEpochMsFile(kActiveUntilFile);
}

// Hard turn-busy gate: true for the whole turn (THINKING + SPEAKING) while voice holds
// the deadline. Single authoritative signal — no inter-chunk gaps like speaking_until.
bool WsTransport::isBusy() {
  long long deadline_ms = readEpochMsFile(kBusyUntilFile);
  if (deadline_ms <= 0) return false;
  return nowMsEpoch() < deadline_ms;
}
