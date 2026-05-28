# STT integration plan — ReSpeaker Core v2 → dev box Vosk endpoint

Branch: `dev/stt-integration` (board) — commit `9f20c8e` (NOT pushed)
Date: 2026-05-27
Status: **All phases complete. End-to-end working. Not pushed yet.**

## Outcome (2026-05-27)

- Board pm2 `asr` process (PID 8511 at last check) streams continuously
  via `ws://192.168.0.95:8766/stt/vosk`.
- Dev box `python -m stt` (commit `8ac18df` on `main`) endpoints with
  silero-VAD, transcribes with Whisper large-v3, returns Vosk-shaped
  JSON. Endpointer reason = `silence` on natural pauses (no more 30s
  `max_utterance` caps).
- Sample transcripts confirmed against spoken input — accurate on
  natural speech, weak on tech vocab, occasional Whisper hallucination
  on ambiguous tails (filterable).
- pm2-logrotate installed on board: max 5MB × 5 retain, gz, daily.

## Open follow-ups

- Whisper hallucination filter — extend `_filter_hallucinations` in
  `stt/services/faster_whisper_engine.py` with new artifacts
  ("Амінь.", "Давай подивимось.", "Я не дуже розумію, чи..."). Or
  drop sub-1.5s outputs whose char/sec ratio is too low.
- Whisper `initial_prompt` for context.
- VAD threshold tuning (`STT_VOSK_VAD_THRESHOLD`, default 0.5).
- pm2 stdbuf wrapper for live transcript visibility in pm2 logs
  (currently board side flushes only when buffer fills).
- Push branches once happy.

## Goal

Redirect the on-board `respeaker_core` binary from the dead ASR server
(`ws://192.168.0.187:2700`) to the dev-box STT service
(`ws://192.168.0.95:8766/stt/vosk`) and drop wake-word gating so the
board streams continuously. Server-side VAD/energy endpointing on the
dev box does utterance segmentation.

## Architecture

```
[ReSpeaker board 192.168.0.82]              [Dev box 192.168.0.95]
mic array (8ch @ 48 kHz)
  ↓
respeakerd.service          (librespeaker DSP daemon — DO NOT TOUCH)
  ↓ PulseAudio
respeaker_core binary       (this repo)
  • PulseCollectorNode 48→16 kHz
  • VepAecBeamformingNode    (AEC + beamform + NS)
  • SnowboyMbDoaKwsNode      (kept for DOA/AGC; wake-word IGNORED)
  ↓ binary PCM16 mono 16 kHz
WS client (IXWebSocket) ─────────→ /stt/vosk  (FastAPI, bind 0.0.0.0:8766)
                                        ↓
                                   stt service (Whisper large-v3)
                                        ↓ Vosk-shaped JSON
                                   {"result":[...],"text":"..."}
                                        ↓
                                   voice/ → claude → tts/ → speaker
```

## Constraints

- Board: armv7l, glibc 2.24, Debian 9, 984 MB RAM no swap, 4 cores.
  Build with `make -j2` max — `-j` will OOM.
- pm2 `asr` process has `watch: true`. Stop process before edits or it
  auto-restarts mid-build. `pm2 stop asr` makes watch dormant.
- `respeakerd.service` (separate, `/lib/systemd/system/respeakerd.service`)
  is the librespeaker DSP daemon. Our binary depends on it via
  PulseAudio. **Never stop it.**
- STT server on dev box must bind `0.0.0.0` (default `127.0.0.1`
  unreachable from board). Set `STT_HOST=0.0.0.0`.

## Phases

### Phase 1 — Backup + branch ✅

```
cd ~/projects/respeaker-websockets
git config user.email respeaker@v2.local
git config user.name respeaker
: > ~/.pm2/logs/asr-out.log              # 422 MB → 0
git stash save -u "WIP-pre-stt-integration-<ts>"
git checkout -b dev/stt-integration       # branched off master HEAD
git stash apply                           # keep stash, do not pop
cp build/respeaker_core build/respeaker_core.preinteg.bak
cp config.json config.json.preinteg.bak
```

WIP stash preserves 5-file refactor (LED `HW_LED_SPEC`, location
header, old IP override).

### Phase 2 — Stop pm2 ✅

```
pm2 stop asr        # status=stopped, watching=disabled
```

### Phase 3 — Edits (next)

**`config.json`:**
```
"webSocketAddress": "ws://192.168.0.95:8766/stt/vosk"
```

**`src/main.cpp`** — drop wake-word gating in the audio loop. Current:
```cpp
if (isWakeWordDetected && wakeWordIndex < 1 && wsClient->isConnected())
{
  wsClient->send(audioChunk);
}
```
Becomes always-stream:
```cpp
if (wsClient->isConnected())
{
  wsClient->send(audioChunk);
}
```
Keep wake-word detection block (logs + pixel ring) but as
informational only, not as a send gate. Also remove the listening-
timeout `isWakeWordDetected = false` branch since we no longer
window utterances on board.

Leave `ws_transport.cpp` `{"location": "livingRoom"}` header as-is —
server ignores unknown text frames; harmless.

### Phase 4 — STT server on dev box

```
STT_HOST=0.0.0.0 STT_PORT=8766 uv run python -m stt
```

Verify from board:
```
curl -s http://192.168.0.95:8766/healthz
```

### Phase 5 — Build

```
cd ~/projects/respeaker-websockets/build
cmake ..                                  # picks up new config.json copy
make -j2
```

CMakeLists copies `config.json` to `build/` automatically.

### Phase 6 — Foreground test

Run binary directly, watch both sides:
```
# board
~/projects/respeaker-websockets/build/respeaker_core

# dev box
tail -f <stt logs>
```

Expected: PCM frames flow board→dev, Vosk JSON responses board←dev.
Speak Ukrainian → final transcripts logged.

### Phase 7 — Restore pm2

```
pm2 start asr
pm2 logs asr --lines 50
```

Confirm stable for 60s. If `watch: true` causes regressions, set
`pm2 set asr:watch false` in ecosystem or delete + re-add without
watch flag.

## Rollback

| Failure | Recovery |
|---|---|
| Bad source | `git checkout -- <file>` |
| Bad branch state | `git checkout master` (master intact) |
| Bad binary | `cp build/respeaker_core.preinteg.bak build/respeaker_core` |
| Lost WIP | `git stash list` → `git stash apply stash@{0}` |
| pm2 wedged | `pm2 delete asr && pm2 start <orig cmd>` from dump |

`config.json.preinteg.bak` also kept alongside `config.json`.

## Open items

- pm2 `watch: true` — keep or drop? Currently triggers restart on
  any file change including builds. Recommend dropping after Phase 7.
- 422 MB `asr-out.log` was the symptom of a runaway crash-loop.
  Consider `pm2 install pm2-logrotate` to cap log size going forward.
- pm2 dump references `versioning.unstaged: true` — pm2 records git
  state in its dump. Will refresh on `pm2 save`.
