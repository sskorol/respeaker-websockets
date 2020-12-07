#ifndef RESPEAKER_CORE_HPP
#define RESPEAKER_CORE_HPP

extern "C"
{
#include "common.h"
#include "cAPA102.h"
#include "gpio_rw.h"
#include "state_handler.h"
#include "verbose.h"
}

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
#include <chain_nodes/snowboy_1b_doa_kws_node.h>

// See JSON lib docs: https://github.com/nlohmann/json (included as a prebuilt single header)
#include "json.hpp"
#include "ws_transport.hpp"
#include "pixel_ring.hpp"

using namespace std;
using namespace respeaker;
using json = nlohmann::json;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

// Common definitions
#define CONFIG_FILE "config.json"
#define BLOCK_SIZE_MS 8

// WebSockets
WsTransport *wsClient;

// Respeaker settings
string kwsPath("/usr/share/respeaker/snowboy/resources/");
string kwsResourcesPath = kwsPath + "common.res";
string inputSource("default");

// Common flow flags
static bool isWakeWordDetected = false;
static bool shouldStopListening = false;

// Default pixel ring config
RUNTIME_OPTIONS RUNTIME = {
    /* Hardware */
    "",
    {-1, -1, -1},
    {-1, -1},
    /* Brightness */
    31,
    /* Animation thread */
    0, // NULL
    ON_IDLE,
    /* Colour */
    {GREEN_C, BLUE_C, PURPLE_C, YELLOW_C, GREEN_C},
    /* Flags */
    0,
    1,
    0,
    /* Animation Enable */
    {1, 1, 1, 1, 1, 1},
    /* Mute*/
    0};

json readConfig();

void handleQuit(int signal);

void configureSignalHandler();

void cleanup(int status);

void setup(json config);

bool trackPixelRingState();

#endif
