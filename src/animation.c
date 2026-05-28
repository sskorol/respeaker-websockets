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

    step = RUNTIME.max_brightness / STEP_COUNT;
    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness && RUNTIME.curr_state == TO_MUTE; curr_bri += step)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == TO_MUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, TO_MUTE);
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 && RUNTIME.curr_state == TO_MUTE; curr_bri -= step)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == TO_MUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, TO_MUTE);
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    if (TO_MUTE == RUNTIME.curr_state)
    {
        RUNTIME.curr_state = ON_IDLE;
        RUNTIME.if_update = 1;
    }
    cAPA102_Clear_All();
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
