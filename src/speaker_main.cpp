// respeaker_speaker — subscribes to the dev box /audio WS, streams PCM16 mono to ALSA.
//
// Protocol (server → client):
//   • text JSON {"type":"start","sample_rate":48000,"channels":1,"format":"s16le"} once on connect
//   • binary PCM16 LE mono frames thereafter
//   • text JSON {"type":"interrupted"} → flush ALSA ring buffer immediately
//
// Designed for low latency: ALSA `default` device routes through PulseAudio, which lets the
// existing Alango/librespeaker AEC tap the playback as reference (no extra wiring needed).

#include <alsa/asoundlib.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "json.hpp"

extern "C"
{
#include "verbose.h"
}

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace
{
constexpr const char *kDefaultWsUrl = "ws://192.168.0.95:9000/audio";
constexpr const char *kDefaultPcmDevice = "default";
// Cross-process flag (UTC epoch ms deadline) read by the ASR pipeline (`ws_transport.cpp`)
// to suppress /prompt POSTs while the Grove speaker is playing TTS — guards against the
// mic-→-STT-→-/prompt self-listen loop when hardware AEC is not killing all leakage.
constexpr const char *kSpeakingUntilFile = "/tmp/respeaker_speaking_until_ms";
// Cross-process flag (UTC epoch ms timestamp) bumped whenever voice signals turn
// cancel without producing playback — i.e. /prompt was 409-rejected or the turn
// watchdog fired. respeaker_core compares against its in-process thinking-set
// timestamp to drop the "thinking" LED state without needing a hardcoded cap.
constexpr const char *kThinkingClearFile = "/tmp/respeaker_thinking_clear_ms";
// Cross-process flag (UTC epoch ms deadline) for the wake-word activation window. Voice
// pushes it on every accepted turn via an `activation` text frame; respeaker_core reads it
// (ws_transport.cpp) to keep the mic→STT stream open. Lapses 15 min after the last dialog.
constexpr const char *kActiveUntilFile = "/tmp/respeaker_active_until_ms";
// Just covers ALSA ring buffer drain + a touch of reverb. Self-loop is double-guarded
// by the voice server's /prompt 409-while-turn-in-progress reject and Whisper's uk-only
// language constraint, so we can be generous about reopening the mic right after Claude
// finishes — otherwise the kid can't answer immediately and gets ignored.
constexpr int64_t kPlaybackTailMarginMs = 500;
// 500 ms ring buffer absorbs LAN/jitter and lets a full TTS chunk (~1.5 s typical, up to a
// few s) write without back-to-back -EPIPE underruns + prepare() cycles, which drop audio
// (each prepare resets the ring → tail is lost).
constexpr unsigned int kBufferTimeUs = 500000;
constexpr unsigned int kPeriodTimeUs = 50000;
constexpr int kWsPingIntervalSec = 45;

std::atomic<bool> shouldExit{false};
std::atomic<int64_t> playbackUntilMs{0};
// Tool-window mic-mute deadline. Kept SEPARATE from playbackUntilMs so that
// real PCM chunks don't compound on top of a hold deadline (max(prev,now)+durMs
// against an inflated playback cursor would push the speaking-until file
// hold_deadline + total_audio_duration into the future). isSpeakerActive
// returns true if EITHER playback OR hold is still in the future.
std::atomic<int64_t> holdUntilMs{0};

// Upper clamp on a single hold window. Defense-in-depth: any LAN-reachable
// /audio client could otherwise send {"until_ms":9999999999999} and mute the
// kid mic forever. 30 s comfortably covers WebSearch/WebFetch round-trips;
// past that the next /prompt watchdog cycle has already kicked in.
constexpr int64_t kMaxHoldMs = 30000;

int64_t nowEpochMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Atomic tmpfs write: write to a sibling .tmp, then rename onto the target. rename(2)
// is atomic on tmpfs (and on any POSIX filesystem) — the reader either sees the old
// file contents or the new ones, never a half-truncated empty file. With fopen("w")
// the reader could land in the µs window between truncate and fprintf and parse 0,
// which would briefly flip isSpeakerActive() to false mid-playback and let one block
// of speaker leak reach the STT WS.
bool writeEpochMsAtomic(const char *path, int64_t value)
{
    std::string tmp = std::string(path) + ".tmp";
    FILE *f = fopen(tmp.c_str(), "w");
    if (f == nullptr) return false;
    fprintf(f, "%lld\n", static_cast<long long>(value));
    fflush(f);
    fclose(f);
    if (rename(tmp.c_str(), path) != 0)
    {
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

// Writes the deadline (UTC epoch ms) to a tmpfs file so the ASR-side process can decide
// whether the mic is currently capturing the speaker's own playback.
void writeSpeakingUntil(int64_t deadline_ms)
{
    writeEpochMsAtomic(kSpeakingUntilFile, deadline_ms);
}

// Bump the thinking-clear timestamp (UTC epoch ms). respeaker_core compares this
// against its in-process thinking-set timestamp; if clear > set, LED drops out of
// ON_LISTEN. Used to signal "turn cancelled, no playback coming."
void writeThinkingClear(int64_t now_ms)
{
    writeEpochMsAtomic(kThinkingClearFile, now_ms);
}

void handleSignal(int sig)
{
    verbose(VV_INFO, stdout, "Caught signal %d. Terminating speaker...", sig);
    shouldExit.store(true);
}

class AlsaSink
{
public:
    explicit AlsaSink(std::string device) : _device(std::move(device)) {}

    ~AlsaSink() { close(); }

    bool open(unsigned int sample_rate, unsigned int channels)
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_pcm != nullptr && sample_rate == _rate && channels == _channels)
        {
            return true;
        }
        if (_pcm != nullptr)
        {
            snd_pcm_close(_pcm);
            _pcm = nullptr;
        }

        int err = snd_pcm_open(&_pcm, _device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0)
        {
            verbose(V_NORMAL, stderr, "snd_pcm_open(%s) failed: %s", _device.c_str(), snd_strerror(err));
            _pcm = nullptr;
            return false;
        }

        err = snd_pcm_set_params(
            _pcm,
            SND_PCM_FORMAT_S16_LE,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            channels,
            sample_rate,
            1,  // soft_resample
            kBufferTimeUs);
        if (err < 0)
        {
            verbose(V_NORMAL, stderr, "snd_pcm_set_params failed: %s", snd_strerror(err));
            snd_pcm_close(_pcm);
            _pcm = nullptr;
            return false;
        }

        applySwParams(sample_rate);

        _rate = sample_rate;
        _channels = channels;
        verbose(VV_INFO, stdout, "ALSA opened: device=%s rate=%u ch=%u", _device.c_str(), sample_rate, channels);
        return true;
    }

    // Apply the no-auto-stop + low-first-byte-latency sw params. snd_pcm_recover() and
    // snd_pcm_prepare() both reset sw params to ALSA defaults — must re-apply after every
    // recovery or the next underrun reverts to stop-on-xrun and the device goes silent
    // even though writei keeps returning success.
    void applySwParams(unsigned int sample_rate)
    {
        snd_pcm_sw_params_t *swp = nullptr;
        snd_pcm_sw_params_alloca(&swp);
        if (snd_pcm_sw_params_current(_pcm, swp) != 0) return;
        snd_pcm_uframes_t boundary = 0;
        if (snd_pcm_sw_params_get_boundary(swp, &boundary) == 0)
        {
            snd_pcm_sw_params_set_stop_threshold(_pcm, swp, boundary);
        }
        snd_pcm_uframes_t period_frames = sample_rate * kPeriodTimeUs / 1000000;
        snd_pcm_sw_params_set_start_threshold(_pcm, swp, period_frames);
        if (snd_pcm_sw_params(_pcm, swp) < 0)
        {
            verbose(V_NORMAL, stderr, "snd_pcm_sw_params failed; using defaults");
        }
    }

    // Writes interleaved s16 frames. Blocks when the ring buffer is full → natural backpressure.
    void write(const char *bytes, size_t len)
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_pcm == nullptr || len == 0)
            return;

        snd_pcm_uframes_t frames = static_cast<snd_pcm_uframes_t>(len / (2 * _channels));
        snd_pcm_uframes_t total = frames;
        const char *cursor = bytes;
        int zeroReturnRetries = 0;
        while (frames > 0)
        {
            snd_pcm_sframes_t written = snd_pcm_writei(_pcm, cursor, frames);
            if (written < 0)
            {
                // snd_pcm_recover handles EPIPE (underrun), ESTRPIPE (suspended), and EIO —
                // covers everything we used to handle ad-hoc plus the EIO path that was
                // wedging the device after the sw_params stop_threshold=boundary tweak
                // (no auto-stop on underrun + naive snd_pcm_prepare leaves PA's wrapper
                // in a state where every subsequent writei returns EIO instantly).
                int rc = snd_pcm_recover(_pcm, static_cast<int>(written), 1);
                if (rc < 0)
                {
                    verbose(V_NORMAL, stderr, "snd_pcm_recover failed: %s (orig=%s)",
                            snd_strerror(rc), snd_strerror(written));
                    return;
                }
                // snd_pcm_recover() calls snd_pcm_prepare() internally which resets the
                // sw params we set in open(). Re-apply them or the next underrun reverts
                // to stop-on-xrun and the device silently stops accepting playback.
                applySwParams(_rate);
                verbose(VV_INFO, stdout, "ALSA recovered from %s", snd_strerror(written));
                continue;
            }
            if (written == 0)
            {
                if (++zeroReturnRetries >= 10)
                {
                    verbose(V_NORMAL, stderr,
                            "snd_pcm_writei returned 0 frames 10 times in a row; bailing on this chunk (wrote=%zu/%zu)",
                            static_cast<size_t>(total - frames), static_cast<size_t>(total));
                    return;
                }
                std::this_thread::sleep_for(5ms);
                continue;
            }
            zeroReturnRetries = 0;
            frames -= written;
            cursor += written * 2 * _channels;
        }
    }

    void drop()
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_pcm == nullptr) return;
        snd_pcm_drop(_pcm);
        snd_pcm_prepare(_pcm);
        // Pre-feed 200 ms of silence so the device has audio in the ring buffer by the
        // time the real chunk arrives. Without this, snd_pcm_writei on the first chunk
        // pays the PulseAudio plug-layer wake latency (~100-200 ms) and the first
        // phoneme of a short utterance gets eaten — critical for kid responses like
        // "Так!" or "Дім!".
        if (_rate > 0 && _channels > 0)
        {
            snd_pcm_uframes_t silence_frames = _rate * 200 / 1000;
            std::vector<int16_t> silence(silence_frames * _channels, 0);
            const char *bytes = reinterpret_cast<const char *>(silence.data());
            snd_pcm_uframes_t total = silence_frames;
            const char *cursor = bytes;
            while (total > 0)
            {
                snd_pcm_sframes_t written = snd_pcm_writei(_pcm, cursor, total);
                if (written < 0)
                {
                    if (written == -EPIPE) snd_pcm_prepare(_pcm);
                    break;
                }
                if (written == 0) break;
                total -= written;
                cursor += written * 2 * _channels;
            }
        }
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_pcm == nullptr) return;
        snd_pcm_drain(_pcm);
        snd_pcm_close(_pcm);
        _pcm = nullptr;
    }

    unsigned int rate() const { return _rate; }
    unsigned int channels() const { return _channels; }

private:
    std::string _device;
    snd_pcm_t *_pcm{nullptr};
    unsigned int _rate{0};
    unsigned int _channels{0};
    std::mutex _mu;
};

void handleTextFrame(AlsaSink &sink, const std::string &payload)
{
    json msg = json::parse(payload, nullptr, false);
    if (msg.is_discarded() || !msg.contains("type"))
    {
        verbose(VVV_DEBUG, stdout, "Ignoring malformed text frame");
        return;
    }
    const std::string type = msg["type"].get<std::string>();
    if (type == "start")
    {
        unsigned int sr = msg.value("sample_rate", 48000);
        unsigned int ch = msg.value("channels", 1);
        sink.open(sr, ch);
        // Voice restart / reconnect MUST drain any audio left in the ALSA ring buffer
        // from the previous session — otherwise the kid hears the tail of last run's
        // TTS before the new greeting starts. drop() flushes + re-primes silence.
        sink.drop();
        playbackUntilMs.store(0);
        holdUntilMs.store(0);
        writeSpeakingUntil(0);
        writeThinkingClear(nowEpochMs());
    }
    else if (type == "hold")
    {
        // Voice fired a tool call (WebSearch/WebFetch) — no PCM will flow until the
        // tool result lands and Claude resumes streaming. Extend the mic-mute
        // deadline (held SEPARATELY from playbackUntilMs so PCM frames don't
        // compound onto it). isSpeakerActive() honours the max of both cursors.
        int64_t until_ms = msg.value("until_ms", static_cast<int64_t>(0));
        if (until_ms <= 0) return;
        int64_t now = nowEpochMs();
        // Clamp to a hard ceiling so a malformed or hostile payload can't mute
        // the mic forever.
        int64_t clamped = std::min(until_ms, now + kMaxHoldMs);
        // Allow SHRINK semantics too — PostToolUse explicitly requests a
        // smaller hold once the tool returns so the deadline can fall toward
        // the natural playback end. Voice owns ordering here.
        holdUntilMs.store(clamped);
        int64_t effective = std::max(clamped, playbackUntilMs.load());
        writeSpeakingUntil(effective + kPlaybackTailMarginMs);
        verbose(VV_INFO, stdout, "Tool hold until epoch ms=%lld (effective=%lld)",
                static_cast<long long>(clamped), static_cast<long long>(effective));
    }
    else if (type == "interrupted")
    {
        verbose(VV_INFO, stdout, "Interrupt received; dropping ALSA buffer");
        sink.drop();
        playbackUntilMs.store(0);
        holdUntilMs.store(0);
        writeSpeakingUntil(0);
        writeThinkingClear(nowEpochMs());
    }
    else if (type == "activation")
    {
        // Voice slid the wake-word window forward (accepted dialog turn). Persist the new
        // deadline so respeaker_core keeps streaming the mic until then. 0 = clear/expire.
        int64_t until_ms = msg.value("until_ms", static_cast<int64_t>(0));
        writeEpochMsAtomic(kActiveUntilFile, until_ms);
        verbose(VV_INFO, stdout, "Activation window until epoch ms=%lld",
                static_cast<long long>(until_ms));
    }
    else
    {
        verbose(VVV_DEBUG, stdout, "Unknown frame type: %s", type.c_str());
    }
}

}  // namespace

int main(int argc, char *argv[])
{
    setVerbose(VV_INFO);

    std::string wsUrl = (argc > 1) ? argv[1] : kDefaultWsUrl;
    std::string pcmDevice = (argc > 2) ? argv[2] : kDefaultPcmDevice;

    struct sigaction sa{};
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    ix::initNetSystem();

    // Reset the speaking-until file on startup so a previous crash doesn't leave a stale
    // future deadline that suppresses POSTs indefinitely. Same for thinking-clear so
    // a stale future timestamp can't pre-empt the very first SttFinal of the session.
    writeSpeakingUntil(0);
    writeThinkingClear(0);

    AlsaSink sink(pcmDevice);
    // Pre-open with sane defaults so the first chunk doesn't pay the open cost.
    // Channels=2 matches current dev-box broadcast default (stereo channel-duplicated mono)
    // so AudioStart doesn't trigger a reopen. If server is configured for mono broadcast,
    // AudioStart will trigger one reopen — small cost, once per session.
    sink.open(48000, 2);

    ix::WebSocket ws;
    ws.setUrl(wsUrl);
    ws.setPingInterval(kWsPingIntervalSec);
    ws.disablePerMessageDeflate();

    std::atomic<uint64_t> binaryFrames{0};
    ws.setOnMessageCallback([&sink, &wsUrl, &binaryFrames](const ix::WebSocketMessagePtr &msg) {
        switch (msg->type)
        {
        case ix::WebSocketMessageType::Open:
            verbose(VV_INFO, stdout, "Connected to audio WS: %s", wsUrl.c_str());
            break;
        case ix::WebSocketMessageType::Message:
            if (msg->binary)
            {
                uint64_t n = ++binaryFrames;
                verbose(VV_INFO, stdout, "PCM frame #%llu (%zu bytes)",
                        static_cast<unsigned long long>(n), msg->str.size());
                unsigned int rate = sink.rate();
                unsigned int chs = sink.channels();
                int64_t durMs = (rate > 0 && chs > 0)
                                    ? static_cast<int64_t>(msg->str.size()) * 1000 /
                                          static_cast<int64_t>(rate * chs * 2)
                                    : 0;
                int64_t now = nowEpochMs();
                // Track end-of-audio without the tail margin so it advances by exactly
                // one chunk duration per frame. Stacking +tail per frame inflated the
                // deadline by N × tail for an N-chunk reply — kid had to wait several
                // seconds after audio physically ended before mic unmuted.
                int64_t prevAudioEnd = playbackUntilMs.load();
                int64_t audioEnd = std::max(prevAudioEnd, now) + durMs;
                playbackUntilMs.store(audioEnd);
                // First real PCM after a hold collapses the hold cursor — Claude is
                // actively speaking now, so the synthetic mute deadline is moot.
                holdUntilMs.store(0);
                writeSpeakingUntil(audioEnd + kPlaybackTailMarginMs);
                sink.write(msg->str.data(), msg->str.size());
            }
            else
                handleTextFrame(sink, msg->str);
            break;
        case ix::WebSocketMessageType::Close:
            verbose(VV_INFO, stdout, "Audio WS closed (reason=%s)", msg->closeInfo.reason.c_str());
            // Voice server went away — flush any queued audio + clear the half-duplex
            // gate so mic isn't permanently muted while we wait for reconnect.
            sink.drop();
            playbackUntilMs.store(0);
            writeSpeakingUntil(0);
            writeThinkingClear(nowEpochMs());
            break;
        case ix::WebSocketMessageType::Error:
            verbose(V_NORMAL, stderr, "Audio WS error: %s", msg->errorInfo.reason.c_str());
            break;
        default:
            break;
        }
    });

    verbose(VV_INFO, stdout, "respeaker_speaker starting: ws=%s alsa=%s", wsUrl.c_str(), pcmDevice.c_str());
    ws.start();

    while (!shouldExit.load())
    {
        std::this_thread::sleep_for(200ms);
    }

    verbose(VV_INFO, stdout, "Shutting down...");
    ws.stop();
    sink.close();
    ix::uninitNetSystem();
    return EXIT_SUCCESS;
}
