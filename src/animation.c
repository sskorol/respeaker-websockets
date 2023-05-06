#include "animation.h"
#include "cAPA102.h"
#include "verbose.h"

extern RUNTIME_OPTIONS RUNTIME;

#define ANIMATION_DELAY_MS 1000
#define BRIGHTNESS_STEP (RUNTIME.max_brightness / STEP_COUNT)

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
    {
        usleep(ANIMATION_DELAY_MS);
        if (RUNTIME.curr_state != state)
        {
            break;
        }
    }
}

// 0
void *on_idle()
{
    int curr_bri = 0;
    uint8_t led;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();
    srand((unsigned int)time(NULL));

    while (RUNTIME.curr_state == ON_IDLE)
    {
        delay_on_state(2000, ON_IDLE);
        cAPA102_Clear_All();
        led = rand() % RUNTIME.LEDs.number;

        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness &&
                           RUNTIME.curr_state == ON_IDLE;
             curr_bri += BRIGHTNESS_STEP)
        {
            cAPA102_Set_Pixel_4byte(led, remap_4byte(RUNTIME.animation_color.idle, curr_bri));
            cAPA102_Refresh();
            delay_on_state(100, ON_IDLE);
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 &&
                                                 RUNTIME.curr_state == ON_IDLE;
             curr_bri -= BRIGHTNESS_STEP)
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

// 1
void *on_listen()
{
    uint8_t i, g, group;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();
    group = RUNTIME.LEDs.number / 3;
    while (RUNTIME.curr_state == ON_LISTEN)
    {
        for (i = 0; i < 3 && RUNTIME.curr_state == ON_LISTEN; i++)
        {
            for (g = 0; g < group && RUNTIME.curr_state == ON_LISTEN; g++)
                cAPA102_Set_Pixel_4byte(g * 3 + i, remap_4byte(RUNTIME.animation_color.listen, RUNTIME.max_brightness));
            cAPA102_Refresh();
            delay_on_state(80, ON_LISTEN);
            cAPA102_Clear_All();
            delay_on_state(80, ON_LISTEN);
        }
    }
    cAPA102_Clear_All();
    return ((void *)"ON_LISTEN");
}

// 2
void *on_speak()
{
    uint8_t j;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    while (RUNTIME.curr_state == ON_SPEAK)
    {
        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness &&
                           RUNTIME.curr_state == ON_SPEAK;
             curr_bri += BRIGHTNESS_STEP)
        {
            for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_SPEAK; j++)
                cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.speak, curr_bri));
            cAPA102_Refresh();
            delay_on_state(20, ON_SPEAK);
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 &&
                                                 RUNTIME.curr_state == ON_SPEAK;
             curr_bri -= BRIGHTNESS_STEP)
        {
            for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_SPEAK; j++)
                cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.speak, curr_bri));
            cAPA102_Refresh();
            delay_on_state(20, ON_SPEAK);
        }
        cAPA102_Clear_All();
        cAPA102_Refresh();
        delay_on_state(200, ON_SPEAK);
    }
    cAPA102_Clear_All();
    return ((void *)"ON_SPEAK");
}

// 3
void *on_mute()
{
    uint8_t j;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness && RUNTIME.curr_state == ON_MUTE; curr_bri += BRIGHTNESS_STEP)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_MUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, ON_MUTE);
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 && RUNTIME.curr_state == ON_MUTE; curr_bri -= BRIGHTNESS_STEP)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_MUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.mute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, ON_MUTE);
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    if (ON_MUTE == RUNTIME.curr_state)
    {
        RUNTIME.curr_state = ON_IDLE;
        RUNTIME.if_update = 1;
    }
    cAPA102_Clear_All();
    return ((void *)"ON_MUTE");
}

// 4
void *on_unmute()
{
    uint8_t j;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    for (curr_bri = 0; curr_bri < RUNTIME.max_brightness && RUNTIME.curr_state == ON_UNMUTE; curr_bri += BRIGHTNESS_STEP)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_UNMUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, ON_UNMUTE);
    }
    curr_bri = RUNTIME.max_brightness;
    for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 && RUNTIME.curr_state == ON_UNMUTE; curr_bri -= BRIGHTNESS_STEP)
    {
        for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_UNMUTE; j++)
            cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.unmute, curr_bri));
        cAPA102_Refresh();
        delay_on_state(50, ON_UNMUTE);
    }
    cAPA102_Clear_All();
    cAPA102_Refresh();
    if (ON_UNMUTE == RUNTIME.curr_state)
    {
        RUNTIME.curr_state = ON_IDLE;
        RUNTIME.if_update = 1;
    }
    cAPA102_Clear_All();
    return ((void *)"ON_UNMUTE");
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
