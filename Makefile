# ReSpeaker Core v2 — respeaker_core (ASR/wake) + respeaker_speaker (audio playback).
# Run on the board (or `ssh respeaker make -C ~/projects/respeaker-websockets <target>`).
#
# Common recipes:
#   make build      # cmake --build (uses make -j4 since on-board cmake is old)
#   make deploy     # build + restart
#   make restart    # pm2 restart asr + speaker
#   make logs       # tail pm2 logs (both procs, raw)
#   make status     # pm2 list + tmpfs flag files
#   make clean      # wipe build/, reconfigure cmake
#
# Hard rules:
#   - Board's cmake is too old for `cmake --build .`; use `make -j4` directly.
#   - tmpfs flags (/tmp/respeaker_speaking_until_ms, /tmp/respeaker_thinking_clear_ms)
#     are the half-duplex contract; status target prints them.

SHELL := /usr/bin/env bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := help

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR := $(ROOT)/build

# pm2 service names — match ecosystem.config.* / first-time pm2 start.
ASR_SVC     ?= asr
SPEAKER_SVC ?= speaker

TMPFS_SPEAK := /tmp/respeaker_speaking_until_ms
TMPFS_THINK := /tmp/respeaker_thinking_clear_ms

.PHONY: help
help:
	@awk 'BEGIN{FS=":.*##"} /^[a-zA-Z0-9_.-]+:.*##/ {printf "  %-14s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

.PHONY: configure
configure:  ## (Re)run cmake in build/
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. 2>&1 | tail -10

.PHONY: build
build:  ## Compile respeaker_core + respeaker_speaker (make -j4)
	@test -d $(BUILD_DIR) || $(MAKE) -s configure
	@cd $(BUILD_DIR) && make -j4 2>&1 | tail -15

.PHONY: restart
restart:  ## pm2 restart asr + speaker
	@pm2 restart $(ASR_SVC) $(SPEAKER_SVC) 2>&1 | tail -5

.PHONY: deploy
deploy: build restart  ## Build + restart pm2 services
	@echo "[board] deployed"

.PHONY: start
start:  ## pm2 start both services (after pm2 delete or first run)
	@pm2 start $(ASR_SVC) $(SPEAKER_SVC) 2>&1 | tail -5

.PHONY: stop
stop:  ## pm2 stop both services
	@pm2 stop $(ASR_SVC) $(SPEAKER_SVC) 2>&1 | tail -5

.PHONY: logs
logs:  ## Tail pm2 logs (asr + speaker, raw)
	@pm2 logs $(ASR_SVC) $(SPEAKER_SVC) --raw

.PHONY: logs-asr
logs-asr:  ## Tail asr only
	@pm2 logs $(ASR_SVC) --raw

.PHONY: logs-speaker
logs-speaker:  ## Tail speaker only
	@pm2 logs $(SPEAKER_SVC) --raw

.PHONY: status
status:  ## pm2 list + half-duplex tmpfs flag files
	@pm2 list 2>&1 | grep -E '$(ASR_SVC)|$(SPEAKER_SVC)' || echo "(services not in pm2 list)"
	@echo "--- tmpfs (epoch ms; 0 = idle) ---"
	@for f in $(TMPFS_SPEAK) $(TMPFS_THINK); do \
	  printf '%s = ' "$$f"; cat "$$f" 2>/dev/null || echo MISSING; \
	done

.PHONY: clean
clean:  ## Wipe build/ and reconfigure
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -s configure

.PHONY: tmpfs-reset
tmpfs-reset:  ## Manually clear half-duplex tmpfs flags (debug)
	@echo 0 > $(TMPFS_SPEAK); echo 0 > $(TMPFS_THINK); echo "[board] tmpfs cleared"
