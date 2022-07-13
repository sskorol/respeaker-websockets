#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include "config.hpp"
#include "verbose.h"

#define RED_C 0xFF0000
#define GREEN_C 0x00FF00
#define BLUE_C 0x0000FF
#define YELLOW_C 0xFFFF00
#define PURPLE_C 0xFF00FF
#define TEAL_C 0x00FFFF
#define ORANGE_C 0xFF8000

class PixelRing
{
public:
    PixelRing(const Config& config);
    ~PixelRing();

    uint32_t textToColour(const std::string& cTxt)
    {
        return colorNames.at(cTxt);
    }

    enum State
    {
        ON_IDLE = 0,
        ON_LISTEN,
        ON_SPEAK,
        TO_MUTE,
        TO_UNMUTE,
        ON_DISABLED,
        STATE_NUM
    };

    void setState(State state);

private:
    static const unsigned int STEP_COUNT { 20 };

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

    const std::map<const std::string, uint32_t> colorNames {
        { "red",    RED_C    },
        { "green",  GREEN_C  },
        { "blue",   BLUE_C   },
        { "yellow", YELLOW_C },
        { "purple", PURPLE_C },
        { "teal",   TEAL_C   },
        { "orange", ORANGE_C }
    };

    // Default pixel ring config
    struct {
        std::string hardware_model;
        HW_LED_SPEC LEDs        = { -1, -1, -1 };
        HW_GPIO_SPEC power      = { -1, -1 };
        uint8_t max_brightness  = { 31 };
        State curr_state        = { ON_IDLE };
        COLOURS animation_color = { GREEN_C, BLUE_C, PURPLE_C, YELLOW_C, GREEN_C };
        bool animation_enable[STATE_NUM] = { 1, 1, 1, 1, 1, 1 };
        uint8_t if_mute = { 0 };
    } RUNTIME;

    bool terminate = { false };

    std::thread threadAnimation;
    std::mutex mutex;
    std::condition_variable cond;
    void worker();
    bool ack;

    int setPowerPin();
    int resetPowerPin();

    bool delay_on_state(int ms, State state);

    void on_idle();
    void on_listen();
    void on_speak();
    void to_mute();
    void to_unmute();
    void on_disabled();

    uint32_t remap_4byte(uint32_t color, uint8_t brightness);

    void delay_on_state(int ms, int state);
};
