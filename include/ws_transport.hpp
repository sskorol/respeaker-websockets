#ifndef WS_TRANSPORT_HPP
#define WS_TRANSPORT_HPP

#define WS_PING_INTERVAL 45
#define WS_CONNECTION_TIMEOUT 5000
#define MICRO_TIMEOUT 1

// Hard upper bound on how long isThinking() can stay true without external clear.
// Defends against the deadlock path where voice 409s the /prompt (turn-in-progress
// or self-echo) — no TTS playback follows, so neither the speaker rising edge nor
// the `interrupted` text frame fires, and _thinkingSetMs would otherwise stick
// forever, leaving the mic→WS send-gate closed and the LED stuck in ON_LISTEN.
#define THINKING_MAX_AGE_MS 30000

/**
 * See WebSocket docs: https://machinezone.github.io/IXWebSocket/
 */
extern "C"
{
#include "verbose.h"
}
#include <ixwebsocket/IXWebSocket.h>
#include "json.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace std;
using json = nlohmann::json;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

class WsTransport
{
private:
  // The active socket for the CURRENT reconnect attempt. Recreated fresh each attempt by
  // the reconnect thread (see ws_transport.cpp for why a fresh object per attempt is
  // mandatory on ixwebsocket 11.0.4), so it's a raw pointer published under _clientMu
  // rather than an owned member. Owner is the reconnect thread's local unique_ptr.
  ix::WebSocket *_client{nullptr};
  std::mutex _clientMu;

  // STT WS URL, captured by connect() and used by every reconnect attempt.
  std::string _wsAddress;

  // Reconnect manager: a background thread drives bounded-backoff reconnection so a
  // transient dev-box outage (or a boot-time race where STT isn't up yet) self-heals
  // instead of crashing the process — which pm2 used to respawn into a tight loop.
  std::thread _reconnectThread;
  std::atomic<bool> _shouldStop{false};
  // UTC epoch ms the current attempt reached Open; 0 if it never did. The loop reads it
  // on close to tell a healthy session (reset backoff) from a flap (grow it).
  std::atomic<long long> _lastOpenMs{0};

  // Written on the ixwebsocket callback thread, read on the main loop — atomic to avoid
  // a data race (benign on ARMv7 for a bool, fixed for correctness/portability).
  std::atomic<bool> _isConnected;
  std::atomic<bool> _isTranscribeReceived;
  // UTC epoch ms timestamp of when SttFinal arrived. 0 means "not thinking". The
  // event-driven clear path is speakerActive rising edge (LED owner clears in main
  // loop) OR respeaker_speaker writes /tmp/respeaker_thinking_clear_ms > this value
  // on receiving an `interrupted` text frame from voice. No hardcoded deadline.
  // Atomic because the WS callback runs on a separate thread from the LED main
  // loop and 64-bit writes are NOT atomic on the board's 32-bit ARM core — without
  // this the main thread could see a torn value (0) and skip the ON_LISTEN state.
  std::atomic<long long> _thinkingSetMs;

  // Build + fully configure a fresh socket (URL, ping, callbacks). Built-in auto-reconnect
  // is disabled — this class owns reconnection so an Open-then-drop flap can't reset the
  // backoff and storm the board.
  std::unique_ptr<ix::WebSocket> makeSocket();
  // Background reconnect loop: fresh socket per attempt, bounded exponential backoff.
  void runReconnectLoop();

public:
  WsTransport();
  ~WsTransport();
  // Start the reconnect manager for `wsAddress`. Returns whether the FIRST attempt reached
  // Open within WS_CONNECTION_TIMEOUT — informational only; the caller must NOT treat false
  // as fatal (the manager keeps retrying and the mic→WS loop guards sends on isConnected()).
  bool connect(string wsAddress);
  void disconnect();
  void send(string audioChunk);
  // Send a JSON control frame to the STT WS (e.g. {"type":"end"}). Used to force-flush
  // STT's utterance buffer the instant the mic gate closes, so a partial captured just
  // before a turn can't bleed into the next turn's transcript.
  void sendText(const string &text);
  bool isConnected();
  bool isTranscribeReceived();
  void isTranscribed(bool state);

  // Half-duplex gate. Reads UTC epoch-ms deadline written by respeaker_speaker
  // (/tmp/respeaker_speaking_until_ms). Returns true while now < deadline. Used by
  // the mic→WS loop in main.cpp to suppress sending audio while the Grove speaker is
  // playing TTS — eliminates hardware-AEC convergence leak + Whisper self-echo entirely.
  static bool isSpeakerActive();

  // Wake-word activation deadline pushed by the voice server. Reads the UTC epoch-ms
  // value respeaker_speaker writes to /tmp/respeaker_active_until_ms on an `activation`
  // text frame. The mic→WS gate in main.cpp streams only while now < this (or the local
  // wake bootstrap). 0 = none. Each accepted dialog turn slides it forward (15-min TTL).
  static long long activeUntilMs();

  // Hard turn-busy gate (/tmp/respeaker_busy_until_ms). True for the WHOLE turn while
  // voice holds the deadline (THINKING + SPEAKING). Single gapless signal the mic→WS
  // loop and the wake handler use to stay muted across the turn — no per-chunk leak.
  static bool isBusy();

  // "Claude is thinking" LED feedback gate. True while a `final` transcript was just
  // received and TTS hasn't started yet. Hard-capped at THINKING_MAX_AGE_MS as a safety
  // net so a rejected /prompt (no playback, no `interrupted` frame) can't leave the
  // mic gate closed forever.
  bool isThinking();
  void clearThinking();
};

#endif
