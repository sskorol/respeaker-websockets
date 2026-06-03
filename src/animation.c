#include "animation.h"
#include "cAPA102.h"
#include "verbose.h"

extern RUNTIME_OPTIONS RUNTIME;

/* @brief: Consider that each color has 255 level brightness,
 *         this function remap the origin rgb value to a certain
 *         level of brightness.
 */
static uint32_t remap_4byte(uint32_t color, uint8_t brightness)
{
    uint8_t r, g, b;

    r = (uint8_t)((color >> 16) * brightness / 255);
    g = (uint8_t)((color >> 8) * brightness / 255);
    b = (uint8_t)(color * brightness / 255);

    return (r << 16) | (g << 8) | b;
}

static void delay_on_state(int ms, int state)
{
    for (int j = 0; j < ms && RUNTIME.curr_state == state; j++)
        usleep(1000);
}

// Quiet hours: keep the idle ring fully dark so it doesn't glow in a dark bedroom.
// Board local clock (Europe/Kyiv ≈ Cluj offset). Only gates ON_IDLE — wake, listen and
// speak still light up, so a kid who says "Alexa" at night still gets visual feedback.
#define NIGHT_START_HOUR 21
#define NIGHT_END_HOUR 7
static int is_night(void)
{
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);
    if (NIGHT_START_HOUR > NIGHT_END_HOUR) // window spans midnight
        return lt.tm_hour >= NIGHT_START_HOUR || lt.tm_hour < NIGHT_END_HOUR;
    return lt.tm_hour >= NIGHT_START_HOUR && lt.tm_hour < NIGHT_END_HOUR;
}

// 0
void *on_idle()
{
    int curr_bri = 0;
    uint8_t led, step;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();
    srand((unsigned int)time(NULL));

    step = RUNTIME.max_brightness / STEP_COUNT;
    while (RUNTIME.curr_state == ON_IDLE)
    {
        if (is_night())
        {
            cAPA102_Clear_All();
            cAPA102_Refresh();
            delay_on_state(5000, ON_IDLE); // poll for state change / morning every 5 s
            continue;
        }
        delay_on_state(2000, ON_IDLE);
        cAPA102_Clear_All();
        led = rand() % RUNTIME.LEDs.number;

        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness &&
                           RUNTIME.curr_state == ON_IDLE;
             curr_bri += step)
        {
            cAPA102_Set_Pixel_4byte(led, remap_4byte(RUNTIME.animation_color.idle, curr_bri));
            cAPA102_Refresh();
            delay_on_state(100, ON_IDLE);
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 &&
                                                 RUNTIME.curr_state == ON_IDLE;
             curr_bri -= step)
        {
            cAPA102_Set_Pixel_4byte(led, remap_4byte(RUNTIME.animation_color.idle, curr_bri));
            cAPA102_Refresh();
            delay_on_state(100, ON_IDLE);
        }
        cAPA102_Set_Pixel_4byte(led, 0);
        cAPA102_Refresh();
        delay_on_state(3000, ON_IDLE);
    }
    cAPA102_Clear_All();
    return ((void *)"ON_IDLE");
}

// 1 — "Thinking" comet. Single bright head with 3-LED fading trail rotates around the
// ring. Reads as a spinner so the kid sees the assistant is busy thinking and the
// pause is intentional.
void *on_listen()
{
    uint8_t head = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();
    uint8_t leds = RUNTIME.LEDs.number;
    uint8_t bri = RUNTIME.max_brightness;
    uint8_t trail[4] = {bri, (uint8_t)(bri / 3), (uint8_t)(bri / 8), (uint8_t)(bri / 24)};
    while (RUNTIME.curr_state == ON_LISTEN)
    {
        cAPA102_Clear_All();
        for (uint8_t k = 0; k < 4; k++)
        {
            if (trail[k] == 0) break;
            uint8_t idx = (head + leds - k) % leds;
            cAPA102_Set_Pixel_4byte(idx, remap_4byte(RUNTIME.animation_color.listen, trail[k]));
        }
        cAPA102_Refresh();
        delay_on_state(70, ON_LISTEN);
        head = (head + 1) % leds;
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    return ((void *)"ON_LISTEN");
}

// HSV → 24-bit RGB. h in [0,360), s/v in [0,255]. Integer math so it runs cheap on
// the board's ARM core without pulling in libm.
static uint32_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint8_t r, g, b;
    uint16_t region = (h / 60) % 6;
    uint16_t rem = (h - region * 60) * 255 / 60;
    uint8_t p = (uint8_t)(((uint16_t)v * (255 - s)) / 255);
    uint8_t q = (uint8_t)(((uint16_t)v * (255 - ((uint32_t)s * rem) / 255)) / 255);
    uint8_t t = (uint8_t)(((uint16_t)v * (255 - ((uint32_t)s * (255 - rem)) / 255)) / 255);
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// 2 — Rainbow chase. Each LED shows a different hue; the whole ring rotates one hue
// step per frame so the kid sees a colorful spinning ring while the assistant speaks.
void *on_speak()
{
    uint16_t phase = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    uint8_t leds = RUNTIME.LEDs.number;
    uint16_t hue_step = leds > 0 ? (360 / leds) : 30;

    while (RUNTIME.curr_state == ON_SPEAK)
    {
        for (uint8_t i = 0; i < leds; i++)
        {
            uint16_t hue = ((uint16_t)(i * hue_step) + phase) % 360;
            cAPA102_Set_Pixel_4byte(i, hsv_to_rgb(hue, 255, RUNTIME.max_brightness));
        }
        cAPA102_Refresh();
        delay_on_state(40, ON_SPEAK);
        phase = (phase + 6) % 360;  // ~one full rotation every 2.4 s
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    return ((void *)"ON_SPEAK");
}

// 3
void *to_mute()
{
    uint8_t j;
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    // Fade the whole ring UP to the mute colour, then HOLD it solid. Unlike the other
    // states this is a persistent indicator, not a one-shot transition: the ring stays lit
    // the entire time the mic is muted so the kid can always see it at a glance. main.cpp
    // keeps curr_state == TO_MUTE (skips steady-LED logic) until the button unmutes, which
    // flips curr_state and lets this thread fall through and clear.
    step = RUNTIME.max_brightness / STEP_COUNT;
    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness && RUNTIME.curr_state == TO_MUTE; curr_bri += step)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == TO_MUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, TO_MUTE);
    }
    // Pin to full mute colour and hold until the state leaves TO_MUTE.
    for (j = 0; j < RUNTIME.LEDs.number; j++)
        cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, RUNTIME.max_brightness));
    cAPA102_Refresh();
    while (RUNTIME.curr_state == TO_MUTE)
        delay_on_state(100, TO_MUTE);
    cAPA102_Clear_All();
    cAPA102_Refresh();
    return ((void *)"TO_MUTE");
}

// 4
void *to_unmute()
{
    uint8_t j;
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    step = RUNTIME.max_brightness / STEP_COUNT;
    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness && RUNTIME.curr_state == TO_UNMUTE; curr_bri += step)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == TO_UNMUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, TO_UNMUTE);
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 && RUNTIME.curr_state == TO_UNMUTE; curr_bri -= step)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == TO_UNMUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, TO_UNMUTE);
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    if (TO_UNMUTE == RUNTIME.curr_state)
    {
        RUNTIME.curr_state = ON_IDLE;
        RUNTIME.if_update = 1;
    }
    cAPA102_Clear_All();
    return ((void *)"TO_UNMUTE");
}

// 5
void *on_disabled()
{
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    while (RUNTIME.curr_state == ON_DISABLED)
    {
        cAPA102_Clear_All();
        delay_on_state(100, ON_DISABLED);
    }
    cAPA102_Clear_All();
    return ((void *)"ON_DISABLED");
}

// 6 — Wake acknowledgment: light the WHOLE ring solid soft-cyan, hold ~2 s, then a single
// smooth fade-out. No pulsing, no loop, no second appearance. NOTE: we deliberately do NOT
// use remap_4byte here — that shared helper has a blue-channel overflow bug (it scales the
// full 0xRRGGBB int instead of the masked byte), so a "dim" cyan came out near-full-bright
// blue and the old breath-loop made it appear twice. We scale each channel by hand instead,
// so brightness is exactly what we ask for. main.cpp holds ON_WAKE long enough (WAKE_LED_-
// HOLD_MS) for the full hold+fade to finish before the steady LED logic takes over.
void *on_wake()
{
    // Soft cyan target (post-scale, correct math). Clearly visible full ring, not blinding.
    const uint8_t WR = 0x00, WG = 0x18, WB = 0x26; // (0,24,38) soft dim cyan
    const int WAKE_HOLD_MS = 1000;                 // ring solid for ~1 s
    const int WAKE_FADE_STEPS = 24;                // smooth fade
    const int WAKE_FADE_STEP_MS = 14;              // 24*14 ≈ 340 ms fade-out
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    uint8_t leds = RUNTIME.LEDs.number;
    uint32_t solid = ((uint32_t)WR << 16) | ((uint32_t)WG << 8) | WB;

    // Whole ring ON, solid.
    for (uint8_t i = 0; i < leds; i++)
        cAPA102_Set_Pixel_4byte(i, solid);
    cAPA102_Refresh();

    // Hold ~2 s (poll the state often so a state change still interrupts promptly).
    for (int t = 0; t < WAKE_HOLD_MS && RUNTIME.curr_state == ON_WAKE; t += 20)
        delay_on_state(20, ON_WAKE);

    // Single smooth fade-out to black — correct per-channel scaling, monotonic.
    for (int s = WAKE_FADE_STEPS; s >= 0 && RUNTIME.curr_state == ON_WAKE; s--)
    {
        uint8_t r = (uint8_t)(WR * s / WAKE_FADE_STEPS);
        uint8_t g = (uint8_t)(WG * s / WAKE_FADE_STEPS);
        uint8_t b = (uint8_t)(WB * s / WAKE_FADE_STEPS);
        uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        for (uint8_t i = 0; i < leds; i++)
            cAPA102_Set_Pixel_4byte(i, c);
        cAPA102_Refresh();
        delay_on_state(WAKE_FADE_STEP_MS, ON_WAKE);
    }

    // Stay dark until main.cpp leaves ON_WAKE — no re-light, no flicker.
    cAPA102_Clear_All();
    cAPA102_Refresh();
    while (RUNTIME.curr_state == ON_WAKE)
        delay_on_state(50, ON_WAKE);
    cAPA102_Clear_All();
    cAPA102_Refresh();
    return ((void *)"ON_WAKE");
}
