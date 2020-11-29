#ifndef RESPEAKER_CORE_HPP
#define RESPEAKER_CORE_HPP

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <fstream>

// Audio DSP provided by Alango: https://wiki.seeedstudio.com/ReSpeaker_Core_v2.0/#closed-source-solution
#include <respeaker.h>
#include <chain_nodes/pulse_collector_node.h>
#include <chain_nodes/vep_aec_beamforming_node.h>
#include <chain_nodes/snowboy_mb_doa_kws_node.h>

// See JSON lib docs: https://github.com/nlohmann/json (included as a prebuilt single header)
#include "json.hpp"

using namespace std;
using namespace respeaker;
using json = nlohmann::json;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

#include "MqttTransport.hpp"
#include "WSTransport.hpp"

// Common definitions
#define CONFIG_FILE "config.json"
#define BLOCK_SIZE_MS 8

json readConfig();

bool setupTransports(json config);

void cleanupTransports();

void handleQuit(int signal);

void configureSignalHandler();

#endif
