#ifndef MAIN_HPP
#define MAIN_HPP

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

#include "log.hpp"
#include "config.hpp"
#include "ws_transport.hpp"
#include "pixel_ring.hpp"
#include "respeaker_core.hpp"
#include <alsa/asoundlib.h>

using namespace std;
// using namespace respeaker;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

// Common definitions
#define CONFIG_FILE "config.json"

// Key entities
WsTransport *wsClient;
Config *config;
RespeakerCore* respeakerCore;

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

void setAlsaMasterVolume(long volume);

string runUnixCommandAndCaptureOutput(string cmd);

void handleQuit(int signal);

void configureSignalHandler();

void cleanup(int status);

void enablePixelRing(Config* config);

bool trackPixelRingState();

#endif
