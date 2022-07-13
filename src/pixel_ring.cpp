#include <chrono>

#define GLOBAL_BRIGHTNESS 31

extern "C" {
#include "cAPA102.h"
#include "gpio_rw.h"
}

#include "pixel_ring.hpp"

PixelRing::PixelRing(const Config& config)
{
    RUNTIME.hardware_model = config.hardwareModelName();
    RUNTIME.max_brightness = config.brightness();
    RUNTIME.animation_color.idle        = textToColour(config.idleColor().c_str());
    RUNTIME.animation_color.listen      = textToColour(config.listenColor().c_str());
    RUNTIME.animation_color.speak       = textToColour(config.speakColor().c_str());
    RUNTIME.animation_color.mute        = textToColour(config.muteColor().c_str());
    RUNTIME.animation_color.unmute      = textToColour(config.unmuteColor().c_str());
    RUNTIME.animation_enable[ON_IDLE]   = config.isIdleAnimationEnabled();
    RUNTIME.animation_enable[ON_LISTEN] = config.isListenAnimationEnabled();
    RUNTIME.animation_enable[ON_SPEAK]  = config.isSpeakAnimationEnabled();
    RUNTIME.animation_enable[TO_MUTE]   = config.isMuteAnimationEnabled();
    RUNTIME.animation_enable[TO_UNMUTE] = config.isUnmuteAnimationEnabled();
    RUNTIME.if_mute      = config.shouldMute();
    RUNTIME.LEDs.number  = config.ledsAmount();
    RUNTIME.LEDs.spi_bus = config.spiBusNumber();
    RUNTIME.LEDs.spi_dev = config.spiDevNumber();
    RUNTIME.power.pin    = config.powerPin();
    RUNTIME.power.val    = config.powerPinValue();

    RUNTIME.curr_state = RUNTIME.if_mute ? TO_MUTE : TO_UNMUTE;

#ifndef EMULATION
    if (setPowerPin() == -1) {
        throw std::runtime_error("Can't enable APA102 power");
    }
#endif
    if (cAPA102_Init(RUNTIME.LEDs.number, RUNTIME.LEDs.spi_bus,
                     RUNTIME.LEDs.spi_dev, GLOBAL_BRIGHTNESS) == -1) {
        resetPowerPin();
        throw std::runtime_error("APA102 initialization failed");
    }

    threadAnimation = std::thread([this]{ worker(); });
}

PixelRing::~PixelRing()
{
    std::unique_lock<std::mutex> lock(mutex);
    terminate = true;
    cond.notify_one();
    lock.unlock();

    threadAnimation.join();

    cAPA102_Close();
#ifndef EMULATION
    resetPowerPin();
#endif
}

/**
 * @brief: set power pin
 *
 * @returns: -1\ On Error
 *            0\ On Success
 */
int PixelRing::setPowerPin()
{
  if (-1 == RUNTIME.power.val || -1 == RUNTIME.power.val)
  {
    verbose(VV_INFO, stdout, BLUE "[%s]" NONE " Mode has no power pin", __PRETTY_FUNCTION__);
    return 0;
  }

  if (-1 == cGPIO_export(RUNTIME.power.pin))
    return -1;

  sleep(1);

  if (-1 == cGPIO_direction(RUNTIME.power.pin, GPIO_OUT))
    return -1;

  if (-1 == cGPIO_write(RUNTIME.power.pin, RUNTIME.power.val))
    return -1;

  verbose(VV_INFO, stdout, BLUE "[%s]" NONE " Set power pin %d to " LIGHT_GREEN "<%s>" NONE, __PRETTY_FUNCTION__, RUNTIME.power.pin, (RUNTIME.power.val) ? "HIGH" : "LOW");
  return 1;
}

/**
 * @brief: release power pin
 *
 * @returns: -1\ On Error
 *            0\ On Success
 */
int PixelRing::resetPowerPin()
{
  if (-1 == RUNTIME.power.val || -1 == RUNTIME.power.val)
  {
    verbose(VV_INFO, stdout, BLUE "[%s]" NONE " Mode has no power pin", __PRETTY_FUNCTION__);
    return 0;
  }

  if (-1 == cGPIO_unexport(RUNTIME.power.pin))
    return -1;

  verbose(VV_INFO, stdout, BLUE "[%s]" NONE " Released power pin", __PRETTY_FUNCTION__);
  return 1;
}

/* Animation state machine */

void PixelRing::setState(State state)
{
    std::unique_lock<std::mutex> lock(mutex);
    RUNTIME.curr_state = state;
    cond.notify_one();
    lock.unlock();
}

/* Animation thread functions */

void PixelRing::worker()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (terminate) {
            break;
        }
        State state = RUNTIME.curr_state;
        lock.unlock();

        if (!RUNTIME.animation_enable[state]) {
            on_disabled();
            continue;
        }

        switch (state) {
            case ON_IDLE:     on_idle();     break;
            case ON_LISTEN:   on_listen();   break;
            case ON_SPEAK:    on_speak();    break;
            case TO_MUTE:     to_mute();     break;
            case TO_UNMUTE:   to_unmute();   break;
            case ON_DISABLED:
            default: on_disabled();
                break;
        }
    }
    cAPA102_Clear_All();
}

bool PixelRing::delay_on_state(int ms, State state)
{
    std::unique_lock<std::mutex> lock(mutex);

    return cond.wait_for(lock, std::chrono::milliseconds(ms),
        [this, state]{ return terminate || RUNTIME.curr_state != state; });
}

uint32_t PixelRing::remap_4byte(uint32_t color, uint8_t brightness)
{
    unsigned int r, g, b;
    r = ((color >> 16) & 0xFF) * brightness / 255;
    g = ((color >> 8) & 0xFF) * brightness / 255;
    b = ((color ) & 0xFF) * brightness / 255;
    return (r << 16) | (g << 8) | b;
}

// 0
void PixelRing::on_idle()
{
    int curr_bri = 0;
    uint8_t led, step;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    srand((unsigned int)time(NULL));

    step = RUNTIME.max_brightness / STEP_COUNT;
    while (true)
    {
        if (delay_on_state(2000, ON_IDLE)) return;
        cAPA102_Clear_All();
        led = rand() % RUNTIME.LEDs.number;

        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness; curr_bri += step)
        {
            cAPA102_Set_Pixel_4byte(led, remap_4byte(RUNTIME.animation_color.idle, curr_bri));
            cAPA102_Refresh();
            if (delay_on_state(100, ON_IDLE)) return;
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0; curr_bri -= step)
        {
            cAPA102_Set_Pixel_4byte(led, remap_4byte(RUNTIME.animation_color.idle, curr_bri));
            cAPA102_Refresh();
            if (delay_on_state(100, ON_IDLE)) return;
        }
        cAPA102_Set_Pixel_4byte(led, 0);
        cAPA102_Refresh();
        if (delay_on_state(3000, ON_IDLE)) return;
    }
}

// 1
void PixelRing::on_listen()
{
    uint8_t i, g, group;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    group = RUNTIME.LEDs.number / 3;
    while (true)
    {
        for (i = 0; i < 3; i++)
        {
            for (g = 0; g < group; g++)
                cAPA102_Set_Pixel_4byte(g * 3 + i, remap_4byte(RUNTIME.animation_color.listen, RUNTIME.max_brightness));
            cAPA102_Refresh();
            if (delay_on_state(80, ON_LISTEN)) return;
            cAPA102_Clear_All();
            if (delay_on_state(80, ON_LISTEN)) return;
        }
    }
}

// 2
void PixelRing::on_speak()
{
    uint8_t j;
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);

    step = RUNTIME.max_brightness / STEP_COUNT;
    for (;;)
    {
        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness &&
                           RUNTIME.curr_state == ON_SPEAK;
             curr_bri += step)
        {
            for (j = 0; j < RUNTIME.LEDs.number; j++)
                cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.speak, curr_bri));
            cAPA102_Refresh();
            if (delay_on_state(20, ON_SPEAK)) return;
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0; curr_bri -= step)
        {
            for (j = 0; j < RUNTIME.LEDs.number; j++)
                cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.speak, curr_bri));
            cAPA102_Refresh();
            if (delay_on_state(20, ON_SPEAK)) return;
        }
        cAPA102_Clear_All();
        cAPA102_Refresh();
        if (delay_on_state(200, ON_SPEAK)) return;
    }
}

// 3
void PixelRing::to_mute()
{
    uint8_t j;
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);

    step = RUNTIME.max_brightness / STEP_COUNT;
    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness; curr_bri += step) {
        for (j = 0; j < RUNTIME.LEDs.number; j++) {
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        }
        cAPA102_Refresh();
        if (delay_on_state(50, TO_MUTE)) return;
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0; curr_bri -= step) {
        for (j = 0; j < RUNTIME.LEDs.number; j++) {
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        }
        cAPA102_Refresh();
        if (delay_on_state(50, TO_MUTE)) return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (TO_MUTE == RUNTIME.curr_state) {
        RUNTIME.curr_state = ON_IDLE;
    }
}

// 4
void PixelRing::to_unmute()
{
    uint8_t j;
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);

    step = RUNTIME.max_brightness / STEP_COUNT;
    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness; curr_bri += step)
    {
        for (j = 0; j < RUNTIME.LEDs.number; j++) {
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        }
        cAPA102_Refresh();
        if (delay_on_state(50, TO_UNMUTE)) return;
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0; curr_bri -= step)
    {
        for (j = 0; j < RUNTIME.LEDs.number; j++) {
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        }
        cAPA102_Refresh();
        if (delay_on_state(50, TO_UNMUTE)) return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (TO_UNMUTE == RUNTIME.curr_state) {
        RUNTIME.curr_state = ON_IDLE;
    }
}

// 5
void PixelRing::on_disabled()
{
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);

    cAPA102_Clear_All();
    std::unique_lock<std::mutex> lock(mutex);

    cond.wait(lock, [this]{ return terminate || RUNTIME.curr_state != ON_DISABLED; });
}
