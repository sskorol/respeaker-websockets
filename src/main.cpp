#include <iostream>
#include <csignal>
#include <chrono>

#include "config.hpp"
#include "ws_transport.hpp"
#include "pixel_ring.hpp"
#include "respeaker_core.hpp"

#include "verbose.h"

// using namespace respeaker;
using SteadyClock = chrono::steady_clock;
using TimePoint = chrono::time_point<SteadyClock>;

// Common definitions
#define CONFIG_FILE "config.json"

// Common flow flags
static sig_atomic_t shouldStopListening = false;

/**
 * Make sure we correctly handle exit signals to close resouces.
 */
static void handleQuit(int)
{
    shouldStopListening = true;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handleQuit);
    signal(SIGTERM, handleQuit);

    setVerbose(VVV_DEBUG);

    Config config(CONFIG_FILE);
    RespeakerCore respeakerCore(config);
    WsTransport wsClient;

    if (!wsClient.connect(config.webSocketAddress()))
    {
        verbose(VV_INFO, stdout, "Unable to connect to WS server. Quitting...");
        return EXIT_FAILURE;
    }

    PixelRing pixelRing(config);

    if (!respeakerCore.startListening())
    {
        verbose(VV_INFO, stdout, "Unable to start the respeaker node chain. Quitting...");
        return EXIT_FAILURE;
    }

    verbose(VV_INFO, stdout, "Press CTRL-C to exit");

    int wakeWordIndex = 0, direction = 0;
    bool isWakeWordDetected = false;
    TimePoint detectTime;
    std::string audioChunk;

    while (!shouldStopListening)
    {
        audioChunk = respeakerCore.processAudio(wakeWordIndex);
//        usleep(1000);
//        wakeWordIndex = rand() % 10000 == 0;

        if (wakeWordIndex >= 1)
        {
            isWakeWordDetected = true;
            wsClient.isTranscribed(false);
            detectTime = SteadyClock::now();
            direction = respeakerCore.soundDirection();
            verbose(VV_INFO, stdout, "Wake word is detected, direction = %d.", direction);
            pixelRing.setState(PixelRing::TO_UNMUTE);
        }

        // Skip the chunk with a hotword to avoid sending it for transciption.
        if (isWakeWordDetected && wakeWordIndex < 1 && wsClient.isConnected())
        {
            wsClient.send(audioChunk);
        }

        // Reset wake word detection flag when wait timeout occurs or if we received a final transcribe from WS server.
        if (isWakeWordDetected && ((SteadyClock::now() - detectTime) > chrono::milliseconds(config.listeningTimeout()) || wsClient.isTranscribeReceived()))
        {
            isWakeWordDetected = false;
            wsClient.isTranscribed(false);
            pixelRing.setState(PixelRing::TO_MUTE);
        }
    }

    respeakerCore.stopAudioProcessing();
    wsClient.disconnect();

    return 0;
}
