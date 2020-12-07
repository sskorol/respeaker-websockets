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
    uint8_t step;
    int curr_bri = 0;
    verbose(VVV_DEBUG, stdout, PURPLE "[%s]" NONE " animation started", __FUNCTION__);
    RUNTIME.if_update = 0;
    cAPA102_Clear_All();

    step = RUNTIME.max_brightness / STEP_COUNT;
    while (RUNTIME.curr_state == ON_SPEAK)
    {
        for (curr_bri = 0; curr_bri < RUNTIME.max_brightness &&
                           RUNTIME.curr_state == ON_SPEAK;
             curr_bri += step)
        {
            for (j = 0; j < RUNTIME.LEDs.number && RUNTIME.curr_state == ON_SPEAK; j++)
                cAPA102_Set_Pixel_4byte(j, remap_4byte(RUNTIME.animation_color.speak, curr_bri));
            cAPA102_Refresh();
            delay_on_state(20, ON_SPEAK);
        }
        curr_bri = RUNTIME.max_brightness;
        for (curr_bri = RUNTIME.max_brightness; curr_bri > 0 &&
                                                 RUNTIME.curr_state == ON_SPEAK;
             curr_bri -= step)
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
