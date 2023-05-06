#ifndef __PIXEL_RING__HPP__
#define __PIXEL_RING_HPP__

#include "config.hpp"
#include <unordered_map>
#include <memory>
extern "C"
{
#include "common.h"
#include "verbose.h"
#include "gpio_rw.h"
#include "state_handler.h"
}

extern RUNTIME_OPTIONS RUNTIME;

using namespace std;

unordered_map<std::string, uint32_t> colorMap = {
    {"red", RED_C},
    {"green", GREEN_C},
    {"blue", BLUE_C},
    {"yellow", YELLOW_C},
    {"purple", PURPLE_C},
    {"teal", TEAL_C},
    {"orange", ORANGE_C},
};

/**
 * Basic color conversion API. 
 */
uint32_t textToColour(const std::string& cTxt)
{
    auto it = colorMap.find(cTxt);
    return (it != colorMap.end()) ? it->second : 0;
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
 * Populate pixel ring runtime options.
 */
void setupPixelRing(shared_ptr<Config> config)
{
  strcpy(RUNTIME.hardware_model, config->hardwareModelName().c_str());
  RUNTIME.max_brightness = config->brightness();
  RUNTIME.animation_color.idle = textToColour(config->idleColor().c_str());
  RUNTIME.animation_color.listen = textToColour(config->listenColor().c_str());
  RUNTIME.animation_color.speak = textToColour(config->speakColor().c_str());
  RUNTIME.animation_color.mute = textToColour(config->muteColor().c_str());
  RUNTIME.animation_color.unmute = textToColour(config->unmuteColor().c_str());
  RUNTIME.animation_enable[ON_IDLE] = config->isIdleAnimationEnabled();
  RUNTIME.animation_enable[ON_LISTEN] = config->isListenAnimationEnabled();
  RUNTIME.animation_enable[ON_SPEAK] = config->isSpeakAnimationEnabled();
  RUNTIME.animation_enable[ON_MUTE] = config->isMuteAnimationEnabled();
  RUNTIME.animation_enable[ON_UNMUTE] = config->isUnmuteAnimationEnabled();
  RUNTIME.if_mute = config->shouldMute();
  RUNTIME.LEDs.number = config->ledsAmount();
  RUNTIME.LEDs.spi_bus = config->spiBusNumber();
  RUNTIME.LEDs.spi_dev = config->spiDevNumber();
  RUNTIME.power.pin = config->powerPin();
  RUNTIME.power.val = config->powerPinValue();
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
