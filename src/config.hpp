#pragma once

#include <fstream>

// See JSON lib docs: https://github.com/nlohmann/json (included as a prebuilt single header)
#include "json.hpp"

#define ON_IDLE_STR "on_idle"
#define ON_LISTEN_STR "on_listen"
#define ON_SPEAK_STR "on_speak"
#define TO_MUTE_STR "to_mute"
#define TO_UNMUTE_STR "to_unmute"
#define ON_DISABLED_STR "on_disabled"

#define C_WS_ADDRESS_STR "webSocketAddress"
#define C_RESPEAKER_STR "respeaker"
#define C_HARDWARE_STR "hardware"
#define C_PIXEL_RING_STR "pixelRing"

#define RSP_KWS_MODEL_STR "kwsModelName"
#define RSP_KWS_SENSITIVITY_STR "kwsSensitivity"
#define RSP_LISTENING_TIMEOUT_STR "listeningTimeout"
#define RSP_WAKEWORD_DETECTION_OFFSET_STR "wakeWordDetectionOffset"
#define RSP_GAIN_LEVEL_STR "gainLevel"
#define RSP_SINGLE_BEAM_OUTPUT_STR "singleBeamOutput"
#define RSP_WAV_LOG_STR "enableWavLog"
#define RSP_AGC_STR "agc"

#define HW_POWER_STR "power"
#define HW_LED_NUM "ledsAmount"
#define HW_LED_SPI_BUS "spiBus"
#define HW_LED_SPI_DEV "spiDev"
#define HW_GPIO_PIN "gpioPin"
#define HW_GPIO_VAL "gpioVal"
#define HW_MODEL_STR "model"

#define PR_LED_BRI_STR "ledBrightness"
#define PR_ON_IDLE_STR "onIdle"
#define PR_ON_LISTEN_STR "onListen"
#define PR_ON_SPEAK_STR "onSpeak"
#define PR_TO_MUTE_STR "toMute"
#define PR_TO_UNMUTE_STR "toUnmute"
#define PR_IDLE_COLOR_STR "idleColor"
#define PR_LISTEN_COLOR_STR "listenColor"
#define PR_SPEAK_COLOR_STR "speakColor"
#define PR_MUTE_COLOR_STR "muteColor"
#define PR_UNMUTE_COLOR_STR "unmuteColor"
#define PR_MUTE_STR "isMutedOnStart"

using namespace std;
using json = nlohmann::json;

class Config
{
private:
    json data;

public:
    Config(const char* name)
    {
        ifstream jStream(name);
        data = json::parse(jStream, nullptr, false);
        jStream.close();
        if (data.is_discarded()) {
            throw std::runtime_error("Read config failed");
        }
    }

    // Respeaker Config
    string kwsModelName()        const { return data[C_RESPEAKER_STR][RSP_KWS_MODEL_STR]; }
    string kwsSensitivityLevel() const { return data[C_RESPEAKER_STR][RSP_KWS_SENSITIVITY_STR]; }
    int    listeningTimeout()    const { return data[C_RESPEAKER_STR][RSP_LISTENING_TIMEOUT_STR]; }
    int    gainLevel()           const { return data[C_RESPEAKER_STR][RSP_GAIN_LEVEL_STR]; }
    bool   doAGC()               const { return data[C_RESPEAKER_STR][RSP_AGC_STR]; }
    bool   doWaveLog()           const { return data[C_RESPEAKER_STR][RSP_WAV_LOG_STR]; }
    bool   isSingleBeamOutput()  const { return data[C_RESPEAKER_STR][RSP_SINGLE_BEAM_OUTPUT_STR]; }
    // Pixel Ring Config
    string hardwareModelName()   const { return data[C_HARDWARE_STR][HW_MODEL_STR]; }
    string idleColor()           const { return data[C_PIXEL_RING_STR][PR_IDLE_COLOR_STR]; }
    string listenColor()         const { return data[C_PIXEL_RING_STR][PR_LISTEN_COLOR_STR]; }
    string speakColor()          const { return data[C_PIXEL_RING_STR][PR_SPEAK_COLOR_STR]; }
    string muteColor()           const { return data[C_PIXEL_RING_STR][PR_MUTE_COLOR_STR]; }
    string unmuteColor()         const { return data[C_PIXEL_RING_STR][PR_UNMUTE_COLOR_STR]; }
    bool   isIdleAnimationEnabled()   const { return data[C_PIXEL_RING_STR][PR_ON_IDLE_STR]; }
    bool   isListenAnimationEnabled() const { return data[C_PIXEL_RING_STR][PR_ON_LISTEN_STR]; }
    bool   isSpeakAnimationEnabled()  const { return data[C_PIXEL_RING_STR][PR_ON_SPEAK_STR]; }
    bool   isMuteAnimationEnabled()   const { return data[C_PIXEL_RING_STR][PR_TO_MUTE_STR]; }
    bool   isUnmuteAnimationEnabled() const { return data[C_PIXEL_RING_STR][PR_TO_UNMUTE_STR]; }
    bool   shouldMute()       const { return data[C_PIXEL_RING_STR][PR_MUTE_STR]; }
    int    brightness()       const { return data[C_PIXEL_RING_STR][PR_LED_BRI_STR]; }
    int    ledsAmount()       const { return data[C_HARDWARE_STR][HW_LED_NUM]; }
    int    spiBusNumber()     const { return data[C_HARDWARE_STR][HW_LED_SPI_BUS]; }
    int    spiDevNumber()     const { return data[C_HARDWARE_STR][HW_LED_SPI_DEV]; }
    int    powerPin()         const { return data[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_PIN]; }
    int    powerPinValue()    const { return data[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_VAL]; }
    // WebSocket Config
    string webSocketAddress() const { return data[C_WS_ADDRESS_STR]; }
};
