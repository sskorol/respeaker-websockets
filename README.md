# respeaker-websockets

C++ on a ReSpeaker Core v2 board. Two binaries:

- **`respeaker_core`** — librespeaker mic chain (Alango VEP + AEC +
  Beamforming + Snowboy MB-DoA KWS) → STT WS client; LED state machine
  (idle / listen / speak) driven by tmpfs flags.
- **`respeaker_speaker`** — subscribes to a remote voice server's `/audio`
  WS, plays PCM16 to ALSA `default` (PulseAudio bridge), writes the
  half-duplex tmpfs flags that `respeaker_core` reads.

Both run under pm2 as `asr` and `speaker`.

This board is the kid-facing edge of a larger system whose brain (Claude
agent SDK + STT + TTS) runs on a dev box. Cross-process architecture,
WS protocols, and the half-duplex contract live in the server repo at
[`docs/ARCHITECTURE.md`](https://github.com/sskorol/styletts2-ukrainian/blob/main/docs/ARCHITECTURE.md).

## Hardware

ReSpeaker Core V2 (ARMv7l 32-bit, Debian 9 + Snips/Alango stack).
8-mic seeed-8ch array on the input side; 2-channel seeed-2ch sink on the
output side. Grove speaker on the 3.5 mm jack.

Memory-relevant quirk: 64-bit writes are NOT atomic on ARMv7. Any
cross-thread 64-bit field (`std::atomic<long long>`) — torn reads
otherwise. See `include/ws_transport.hpp::_thinkingSetMs`.

## Build prerequisites

Done once on the board:

```sh
sudo apt-get update && sudo apt-get install -y \
  cmake libupm1 mraa-tools zlib1g-dev \
  librespeaker librespeaker-dev libsndfile1-dev libasound2-dev
```

IXWebSocket from source (board's apt has no package):

```sh
git clone https://github.com/machinezone/IXWebSocket.git
cd IXWebSocket && mkdir build && cd build
cmake .. && make -j && sudo make install
```

`nlohmann/json.hpp` is vendored under `include/`.

## Build / deploy / run

Driven by `Makefile`. Run on the board (or from the dev box via
`ssh respeaker make -C ~/projects/respeaker-websockets <target>`):

```sh
make help               # list targets

make build              # cmake → make -j4 (the board's cmake is too old
                        # for `cmake --build`; we shell out to make directly)
make restart            # pm2 restart asr + speaker
make deploy             # build + restart
make logs               # tail pm2 logs (both procs, raw)
make status             # pm2 list + tmpfs flag files
make tmpfs-reset        # manually clear /tmp/respeaker_*_ms files (debug)
make clean              # wipe build/ and reconfigure cmake
```

First-time pm2 registration:

```sh
pm2 start ./build/respeaker_core    --name asr     --time --watch
pm2 start ./build/respeaker_speaker --name speaker --time
pm2 save
```

(`respeaker_speaker` takes optional positional args `ws_url alsa_device`,
default `ws://192.168.0.95:9100/audio` and `default`.)

## Configuration

`config.json` in the build dir (auto-copied from repo root by cmake):

```json
{
  "webSocketAddress": "ws://192.168.0.95:8766/stt/stream",
  "respeaker": {
    "kwsModelName": "snowboy.umdl",
    "kwsSensitivity": "0.6",
    "gainLevel": 10,
    "singleBeamOutput": false,
    "agc": true
  },
  "pixelRing": {
    "ledBrightness": 20,
    "idleColor": "teal",
    "listenColor": "blue",
    "speakColor": "purple",
    "isMutedOnStart": false
  },
  "hardware": {
    "ledsAmount": 12,
    "spiBus": 0, "spiDev": 0,
    "power": { "gpioPin": 66, "gpioVal": 0 }
  }
}
```

Change `webSocketAddress` to point at your STT server's `/stt/stream`
endpoint (the dev-box STT process).

## What goes over the wire

### From the board

- **mic → STT WS `/stt/stream`** (`respeaker_core` → dev box)
  Binary PCM16 LE @ 16 kHz mono. Suppressed while `isSpeakerActive()` or
  `isThinking()` (half-duplex gate).

### Into the board

- **voice `/audio` WS** (dev-box voice → `respeaker_speaker`)
  Text JSON `AudioStart{sample_rate, channels, format}` on connect, then
  binary PCM16 LE frames. One text `{"type":"interrupted"}` on cancel.
- **STT `/stt/stream` WS events** (dev-box STT → `respeaker_core`)
  `SttFinal` flips LED to `ON_LISTEN` (Claude thinking); `SttDropped`
  noted. STT POSTs voice `/prompt` itself — board never relays text.

## Half-duplex contract

```
mic→WS suppressed while:
  isSpeakerActive()   /tmp/respeaker_speaking_until_ms > now    (TTS playing)
  OR isThinking()     _thinkingSetMs set, clear file < set,     (Claude thinking)
                      AND now − _thinkingSetMs < 30 000 ms      (safety cap)
```

Three signals:

| Signal | Set by | Cleared by |
|---|---|---|
| `_thinkingSetMs` (atomic long long, in-process) | `SttFinal` event in `ws_transport.cpp` | `speakerActive` rising edge (main loop) **OR** clear file > set **OR** > 30 s cap |
| `/tmp/respeaker_speaking_until_ms` | `respeaker_speaker` on every PCM frame: `max(prev, now) + chunk_ms + 500 ms` | `interrupted` frame or `/audio` close — speaker writes 0 |
| `/tmp/respeaker_thinking_clear_ms` | `respeaker_speaker` on `interrupted` frame or `/audio` close | n/a — reader compares vs `_thinkingSetMs` |

Tmpfs writes use `write-temp + rename(2)` for atomicity so the reader
can't observe a half-truncated zero between truncate and fprintf.

## LED state machine

Evaluated each audio chunk (~10 ms) in `respeaker_core`'s main loop:

```
speakerActive ? ON_SPEAK : (thinking ? ON_LISTEN : ON_IDLE)
```

| State | Animation | Source |
|---|---|---|
| `ON_IDLE` | random single LED breathing, teal, 3 s cycles | `src/animation.c::idle_loop` |
| `ON_LISTEN` (thinking) | 4-LED comet trail, blue, 70 ms/frame, ~840 ms/cycle | `on_listen_loop` |
| `ON_SPEAK` | 12-LED HSV rainbow chase, 40 ms/frame, ~2.4 s/cycle | `on_speak_loop` |

No hardcoded timer caps on LED states — they're driven entirely by
`speakerActive` + `thinking` signals.

## File layout

```
include/
  ws_transport.hpp       STT WS client + LED gate signals
  config.hpp, ...
src/
  main.cpp               respeaker_core: librespeaker chain + LED main loop
  speaker_main.cpp       respeaker_speaker: /audio WS subscriber + ALSA
  ws_transport.cpp       STT event parsing, isThinking, isSpeakerActive
  animation.c            LED animations (idle / listen / speak / mute)
  config.cpp, respeaker_core.cpp
  pixel_ring/, hotword/, ...
build/                    (gitignored; cmake output, runtime config.json)
docs/STT_INTEGRATION_PLAN.md
Makefile                  build / restart / deploy / logs / status
CMakeLists.txt            two targets: respeaker_core + respeaker_speaker
config.json               source of truth for KWS sensitivity, GPIO, LED palette
```

## Common operations

```sh
make status                       # pm2 list + tmpfs files (debug peek)
make tmpfs-reset                  # clear both half-duplex flags
pm2 logs asr     --raw            # board ASR + LED loop output
pm2 logs speaker --raw            # board /audio subscriber
ls -la /tmp/respeaker_*_ms        # half-duplex tmpfs flags
```

When the LED is stuck in `ON_LISTEN` (thinking) and the kid mic feels
muted: 30 s safety cap clears it. If sooner, `make tmpfs-reset` does it
by hand. Root cause is usually that voice 409'd a `/prompt` without
producing playback — see issue tracking in the server repo.

## License

Inherits from the original sskorol/respeaker-websockets project (MIT).
