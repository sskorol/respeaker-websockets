#ifndef __COMMON_H__
#define __COMMON_H__

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STATE_NUM 6
#define ON_IDLE_STR "on_idle"
#define ON_LISTEN_STR "on_listen"
#define ON_SPEAK_STR "on_speak"
#define TO_MUTE_STR "to_mute"
#define TO_UNMUTE_STR "to_unmute"
#define ON_DISABLED_STR "on_disabled"

typedef enum
{
    ON_IDLE = 0,
    ON_LISTEN,
    ON_SPEAK,
    TO_MUTE,
    TO_UNMUTE,
    ON_DISABLED
} STATE;

#define C_WS_ADDRESS_STR "webSocketAddress"
#define C_RESPEAKER_STR "respeaker"
#define C_HARDWARE_STR "hardware"
#define C_PIXEL_RING_STR "pixelRing"

#define RSP_ROOM_STR "room"
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

#define RED_C 0xFF0000
#define GREEN_C 0x00FF00
#define BLUE_C 0x0000FF
#define YELLOW_C 0xFFFF00
#define PURPLE_C 0xFF00FF
#define TEAL_C 0x00FFFF
#define ORANGE_C 0xFF8000

#define GLOBAL_BRIGHTNESS 31

typedef struct
{
    uint32_t idle;
    uint32_t listen;
    uint32_t speak;
    uint32_t mute;
    uint32_t unmute;
} COLOURS;

typedef struct
{
    int number;
    int spi_bus;
    int spi_dev;
} HW_LED_SPEC;

typedef struct
{
    int pin;
    int val;
} HW_GPIO_SPEC;

typedef struct
{
    /* Hardware */
    char hardware_model[50];
    HW_LED_SPEC LEDs;
    HW_GPIO_SPEC power;

    /* Brightness */
    uint8_t max_brightness;

    /* Animation thread */
    pthread_t curr_thread;
    STATE curr_state;

    /* Colour */
    COLOURS animation_color;

    /* Flags */
    volatile sig_atomic_t if_terminate;
    uint8_t if_update;
    uint8_t if_disable;

    /* Animation Enable */
    uint8_t animation_enable[STATE_NUM];

    /* Feedback sound */
    uint8_t if_mute;
} RUNTIME_OPTIONS;

#endif
