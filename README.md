# respeaker-websockets

C++ firmware for a **ReSpeaker Core v2** board. It is the kid-facing edge of
a Ukrainian voice-chat system: the child says a wake word, talks, and a remote
"brain" (a Claude agent + speech-to-text + text-to-speech, all running on a
separate dev box) answers back through the board's speaker.

This repo is **only the board side**. It speaks to the dev box over two
WebSockets and knows nothing about Claude — it ships microphone audio out and
plays answer audio back, gated so the two never fight each other.

Two binaries, both run under **pm2** (`asr` and `speaker`):

| Binary | pm2 name | Job |
|---|---|---|
| `respeaker_core` | `asr` | Runs the librespeaker mic chain (Alango VEP + AEC + beamforming + Snowboy wake-word). When the activation window is open, streams mic PCM to the dev-box **STT** WebSocket. Drives the LED ring. |
| `respeaker_speaker` | `speaker` | Subscribes to the dev-box **voice** server's `/audio` WebSocket, plays PCM16 answer audio to ALSA, and writes the tmpfs flags that tell `respeaker_core` to stop listening while audio is playing. |

## How it works

### Wake word → activation window

The mic is **off by default** — no audio leaves the board, so random
parent↔kid chatter never reaches the server. Saying **"Alexa"**
(`alexa.umdl` Snowboy model) opens a **1-minute activation window**; while it
is open, no wake word is needed and conversation flows naturally.

Each real dialog turn **slides the window forward**: the voice server pushes a
fresh deadline to the board, so an active conversation never lapses. After one
minute of silence the window closes and the mic goes off again until the next
"Alexa".

```
"Alexa"  ──►  respeaker_core opens a local 1-min window  ──►  mic streams to STT
                       ▲                                            │
   server slides it ───┘                                           ▼
   (/audio "activation" frame → /tmp/respeaker_active_until_ms)   STT → Claude → TTS
                                                                    │
                       answer audio  ◄── /audio WS ── respeaker_speaker ◄┘
```

The board self-bootstraps the window locally on wake (so the first command
right after "Alexa" is never clipped) and also honours the server-pushed
deadline; the two compose with `max()`. Tunables live in `src/main.cpp`:
`ACTIVE_WINDOW_MS` (window length) and `WAKE_LED_HOLD_MS` (wake-LED hold).

### Half-duplex (no echo)

The board is **half-duplex**: the mic is muted whenever the speaker is
playing or while Claude is thinking. This kills acoustic self-feedback by
construction (no barge-in, by design). Three signals decide it:

| Signal | Set by | Cleared by |
|---|---|---|
| `_thinkingSetMs` (in-process `atomic<long long>`) | `SttFinal` event in `ws_transport.cpp` (Claude is now thinking) | `speakerActive` rising edge **OR** clear-file > set **OR** 30 s safety cap |
| `/tmp/respeaker_speaking_until_ms` | `respeaker_speaker` on each PCM frame: `max(prev, now) + chunk_ms + 500 ms` | `interrupted` frame or `/audio` close (writes 0) |
| `/tmp/respeaker_thinking_clear_ms` | `respeaker_speaker` on `interrupted` frame or `/audio` close | reader compares against `_thinkingSetMs` |

So the mic streams only when: **activation window open AND not speaking AND
not thinking AND WS connected**.

Tmpfs writes use *write-temp + `rename(2)`* so a reader can never observe a
half-truncated value mid-write.

### LED ring

12-LED APA102 ring, animated in its own pthread (`src/state_handler.c`); the
main loop only flips state via `changePixelRingState()` (never writes pixels
directly — that would race the animation thread).

| State | Trigger | Animation |
|---|---|---|
| `ON_IDLE` | nothing happening | single LED breathing, teal, 3 s cycle |
| `ON_WAKE` | "Alexa" detected | whole-ring cyan breathing pulse, held `WAKE_LED_HOLD_MS` (~1.5 s) |
| `ON_LISTEN` | Claude thinking | 4-LED comet trail, blue |
| `ON_SPEAK` | answer audio playing | 12-LED rainbow chase |

Steady-state selection each ~10 ms audio chunk:
`speakerActive ? ON_SPEAK : (thinking ? ON_LISTEN : ON_IDLE)` — overridden by
`ON_WAKE` for the hold window right after a wake. No hardcoded LED timers;
states follow the `speakerActive`/`thinking`/wake signals.

## Hardware

ReSpeaker Core V2 — **ARMv7l 32-bit**, Debian 9, ~984 MB RAM, **no swap**,
4 cores, Snips/Alango DSP stack. 8-mic seeed-8ch array in, 2-channel sink out,
Grove speaker on the 3.5 mm jack.

> **ARMv7 quirk:** 64-bit writes are **not atomic**. Any cross-thread 64-bit
> field must be `std::atomic<long long>` or you get torn reads. See
> `include/ws_transport.hpp::_thinkingSetMs`.

> **No swap, ~1 GB RAM:** build **single-job** (plain `make`, never `-j`) and
> stop pm2 services before building, or the box OOMs. The Makefile does this.

## Setup

### Build prerequisites (once on the board)

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

### Config

`config.json` (in the repo root, copied into `build/` at build time) holds the
**LAN address of your dev box**, so it is **gitignored**. Create it from the
template and point it at your dev-box STT endpoint:

```sh
cp config.example.json config.json
# edit "webSocketAddress" → ws://<your-dev-box-ip>:8766/stt/stream
```

```jsonc
{
  "webSocketAddress": "ws://<DEV_BOX_IP>:8766/stt/stream",
  "respeaker": {
    "kwsModelName": "alexa.umdl",   // Snowboy wake-word model under models/
    "kwsSensitivity": "0.6",        // raise toward 0.65 if "Alexa" under-triggers
    "listeningTimeout": 8000,
    "gainLevel": 10,
    "singleBeamOutput": false,
    "enableWavLog": false,          // off — wav logging just burdens the CPU
    "agc": true,
    "mic0Angle": 0,                 // physical mount angle of mic0 (DOA frame)
    "triggerConfirmMs": 0           // MB-DoA post-trigger confirm window
  },
  "pixelRing": { "ledBrightness": 20, "idleColor": "teal", "listenColor": "blue", "speakColor": "purple", "isMutedOnStart": false },
  "hardware":  { "ledsAmount": 12, "spiBus": 0, "spiDev": 0, "power": { "gpioPin": 66, "gpioVal": 0 } }
}
```

## Build / deploy / run

Driven by `Makefile`. Run on the board, or from the dev box via
`ssh respeaker make -C ~/projects/respeaker-websockets <target>`:

```sh
make help          # list targets
make build         # single-job compile (+ syncs config.json into build/)
make restart       # pm2 restart asr + speaker
make deploy        # stop services → build → restart  (safe for the 1 GB box)
make logs          # tail pm2 logs (both procs)
make status        # pm2 list + tmpfs flag files
make tmpfs-reset   # clear /tmp/respeaker_*_ms flags (debug)
make clean         # wipe build/ and reconfigure cmake
```

First-time pm2 registration:

```sh
pm2 start ./build/respeaker_core    --name asr     --time --watch
pm2 start ./build/respeaker_speaker --name speaker --time
pm2 save
```

`respeaker_speaker` takes optional positional args `ws_url alsa_device`
(defaults `ws://127.0.0.1:9000/audio` and `default`) — pm2 passes the real
dev-box `/audio` URL.

## Wire protocol

### Board → dev box

- **mic → STT `/stt/stream`** (`respeaker_core`): binary PCM16 LE @ 16 kHz
  mono. Sent only while the activation window is open and not speaking/thinking.
  STT itself POSTs the transcript to the voice `/prompt` — the board never
  relays text.

### Dev box → board

- **voice `/audio`** (→ `respeaker_speaker`): text JSON
  `AudioStart{sample_rate, channels, format}` on connect, then binary PCM16 LE
  frames. Text control frames: `{"type":"interrupted"}` (cancel → flush ALSA),
  `{"type":"hold","until_ms":…}` (mute mic during a tool window),
  `{"type":"activation","until_ms":…}` (slide the activation window).
- **STT `/stt/stream` events** (→ `respeaker_core`): `SttFinal` flips the LED
  to `ON_LISTEN` (Claude thinking); `SttDropped` noted.

## File layout

```
include/
  ws_transport.hpp     STT WS client; isThinking / isSpeakerActive / activeUntilMs
  common.h             STATE enum (idle/listen/speak/wake/...), config keys
  animation.h, pixel_ring.hpp, config.hpp, ...
src/
  main.cpp             respeaker_core: librespeaker chain, wake gate, LED main loop
  speaker_main.cpp     respeaker_speaker: /audio subscriber, ALSA, tmpfs flags
  ws_transport.cpp     STT event parsing + half-duplex / activation signals
  animation.c          LED animations (idle / wake / listen / speak / mute)
  state_handler.c      animation-thread state machine
  config.cpp, respeaker_core.cpp, cAPA102.c, gpio_rw.c, verbose.c
models/                Snowboy wake-word models (alexa.umdl)
config.example.json    template → copy to config.json (gitignored) and edit
config.json            live config with your dev-box address (gitignored)
Makefile               build / restart / deploy / logs / status
CMakeLists.txt         two targets: respeaker_core + respeaker_speaker
```

## Troubleshooting

```sh
make status                  # pm2 + tmpfs flags at a glance
make tmpfs-reset             # clear half-duplex / activation flags by hand
pm2 logs asr     --raw       # mic chain + wake + LED loop
pm2 logs speaker --raw       # /audio subscriber + ALSA
ls -la /tmp/respeaker_*_ms   # speaking / thinking-clear / active-until flags
```

- **LED stuck in `ON_LISTEN`, mic feels dead:** a turn started thinking but no
  audio played. The 30 s cap self-clears it; `make tmpfs-reset` does it now.
- **"Alexa" under-/over-triggers:** tune `kwsSensitivity` (0.5–0.65).
- **Board won't reach the server:** the dev-box STT must bind `0.0.0.0`
  (default `127.0.0.1` is unreachable from the board); check `webSocketAddress`.

## License

Inherits from the original sskorol/respeaker-websockets project (MIT).
