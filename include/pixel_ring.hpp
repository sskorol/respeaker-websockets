#ifndef __PIXEL_RING__HPP__
#define __PIXEL_RING_HPP__

#include "json.hpp"
extern "C"
{
#include "common.h"
#include "verbose.h"
#include "gpio_rw.h"
#include "state_handler.h"
}

extern RUNTIME_OPTIONS RUNTIME;

using namespace std;
using json = nlohmann::json;

/**
 * Basic color conversion API. 
 */
uint32_t textToColour(const char *cTxt)
{
  if (strlen(cTxt))
  {
    if (!strcmp(cTxt, "red"))
    {
      return RED_C;
    }
    else if (!strcmp(cTxt, "green"))
    {
      return GREEN_C;
    }
    else if (!strcmp(cTxt, "blue"))
    {
      return BLUE_C;
    }
    else if (!strcmp(cTxt, "yellow"))
    {
      return YELLOW_C;
    }
    else if (!strcmp(cTxt, "purple"))
    {
      return PURPLE_C;
    }
    else if (!strcmp(cTxt, "teal"))
    {
      return TEAL_C;
    }
    else if (!strcmp(cTxt, "orange"))
    {
      return ORANGE_C;
    }
  }
  return 0;
}

void dumpInfo()
{
  verbose(VV_INFO, stdout, "Brightness .......... %d", RUNTIME.max_brightness);
  verbose(VV_INFO, stdout, "Device .............. %s", RUNTIME.hardware_model);
  verbose(VV_INFO, stdout, "Press CTRL-C to exit");
}

/**
 * @brief: set power pin
 *
 * @returns: -1\ On Error
 *            0\ On Success
 */
int setPowerPin()
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
int resetPowerPin()
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

/**
 * Parse json config and populate pixel ring runtime options.
 */
void setupPixelRing(json config)
{
  string model = config[C_HARDWARE_STR][HW_MODEL_STR];
  strcpy(RUNTIME.hardware_model, model.c_str());

  int brightness = config[C_PIXEL_RING_STR][PR_LED_BRI_STR];
  RUNTIME.max_brightness = brightness;

  string idleColor = config[C_PIXEL_RING_STR][PR_IDLE_COLOR_STR];
  RUNTIME.animation_color.idle = textToColour(idleColor.c_str());

  string listenColor = config[C_PIXEL_RING_STR][PR_LISTEN_COLOR_STR];
  RUNTIME.animation_color.listen = textToColour(listenColor.c_str());

  string speakColor = config[C_PIXEL_RING_STR][PR_SPEAK_COLOR_STR];
  RUNTIME.animation_color.speak = textToColour(speakColor.c_str());

  string muteColor = config[C_PIXEL_RING_STR][PR_MUTE_COLOR_STR];
  RUNTIME.animation_color.mute = textToColour(muteColor.c_str());

  string unmuteColor = config[C_PIXEL_RING_STR][PR_UNMUTE_COLOR_STR];
  RUNTIME.animation_color.unmute = textToColour(unmuteColor.c_str());

  RUNTIME.animation_enable[ON_IDLE] = config[C_PIXEL_RING_STR][PR_ON_IDLE_STR];
  RUNTIME.animation_enable[ON_LISTEN] = config[C_PIXEL_RING_STR][PR_ON_LISTEN_STR];
  RUNTIME.animation_enable[ON_SPEAK] = config[C_PIXEL_RING_STR][PR_ON_SPEAK_STR];
  RUNTIME.animation_enable[TO_MUTE] = config[C_PIXEL_RING_STR][PR_TO_MUTE_STR];
  RUNTIME.animation_enable[TO_UNMUTE] = config[C_PIXEL_RING_STR][PR_TO_UNMUTE_STR];
  RUNTIME.if_mute = config[C_PIXEL_RING_STR][PR_MUTE_STR];

  int leds = config[C_HARDWARE_STR][HW_LED_NUM];
  RUNTIME.LEDs.number = leds;
  int spi_bus = config[C_HARDWARE_STR][HW_LED_SPI_BUS];
  RUNTIME.LEDs.spi_bus = spi_bus;
  int spi_dev = config[C_HARDWARE_STR][HW_LED_SPI_DEV];
  RUNTIME.LEDs.spi_dev = spi_dev;

  int power_pin = config[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_PIN];
  RUNTIME.power.pin = power_pin;
  int power_value = config[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_VAL];
  RUNTIME.power.val = power_value;
}

/**
 * Perform an infinite state polling and check if we need to exit program.
 */
bool trackPixelRingState()
{
  if (RUNTIME.if_update)
  {
    state_machine_update();
  }
  return !RUNTIME.if_terminate;
}

/**
 * Change pixel ring pattern. It's automatically handled by the state machine.
 */
void changePixelRingState(STATE state)
{
  RUNTIME.curr_state = state;
  RUNTIME.if_update = 1;
}

#endif
