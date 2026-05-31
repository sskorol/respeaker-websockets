#include "config.hpp"

Config::Config(const char* name)
{
  ifstream jStream(name);
  data = json::parse(jStream, nullptr, false);
  jStream.close();
}

bool Config::isRead()
{
  return !data.is_discarded();
}

// All getters use nlohmann `.value(key, default)` rather than `operator[]`. operator[] on a
// missing key INSERTS null and the subsequent `.get<T>()` THROWS — and only the parse is
// guarded (isRead). A malformed/partial config would crash mid-run. `.value()` degrades to a
// sane default instead. Nested access goes through `.value(section, object())` so a missing
// top-level section can't throw either.

// Respeaker Config
string Config::kwsModelName()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_KWS_MODEL_STR, string(""));
}

string Config::kwsSensitivityLevel()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_KWS_SENSITIVITY_STR, string(""));
}

int Config::listeningTimeout()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_LISTENING_TIMEOUT_STR, 0);
}

int Config::gainLevel()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_GAIN_LEVEL_STR, 0);
}

bool Config::doAGC()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_AGC_STR, false);
}

bool Config::doWaveLog()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_WAV_LOG_STR, false);
}

bool Config::isSingleBeamOutput()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_SINGLE_BEAM_OUTPUT_STR, false);
}

// Optional (default if absent): .value() avoids the throw that data[][] does on a missing key.
int Config::mic0Angle()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_MIC0_ANGLE_STR, 0);
}

int Config::triggerConfirmMs()
{
  return data.value(C_RESPEAKER_STR, json::object()).value(RSP_TRIGGER_CONFIRM_STR, 0);
}

// Pixel Ring Config
string Config::hardwareModelName()
{
  return data.value(C_HARDWARE_STR, json::object()).value(HW_MODEL_STR, string(""));
}

string Config::idleColor()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_IDLE_COLOR_STR, string(""));
}

string Config::listenColor()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_LISTEN_COLOR_STR, string(""));
}

string Config::speakColor()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_SPEAK_COLOR_STR, string(""));
}

string Config::muteColor()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_MUTE_COLOR_STR, string(""));
}

string Config::unmuteColor()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_UNMUTE_COLOR_STR, string(""));
}

bool Config::isIdleAnimationEnabled()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_ON_IDLE_STR, false);
}

bool Config::isListenAnimationEnabled()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_ON_LISTEN_STR, false);
}

bool Config::isSpeakAnimationEnabled()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_ON_SPEAK_STR, false);
}

bool Config::isMuteAnimationEnabled()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_TO_MUTE_STR, false);
}

bool Config::isUnmuteAnimationEnabled()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_TO_UNMUTE_STR, false);
}

bool Config::shouldMute()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_MUTE_STR, false);
}

int Config::brightness()
{
  return data.value(C_PIXEL_RING_STR, json::object()).value(PR_LED_BRI_STR, 31);
}

int Config::ledsAmount()
{
  return data.value(C_HARDWARE_STR, json::object()).value(HW_LED_NUM, 12);
}

int Config::spiBusNumber()
{
  return data.value(C_HARDWARE_STR, json::object()).value(HW_LED_SPI_BUS, 0);
}

int Config::spiDevNumber()
{
  return data.value(C_HARDWARE_STR, json::object()).value(HW_LED_SPI_DEV, 0);
}

int Config::powerPin()
{
  return data.value(C_HARDWARE_STR, json::object())
      .value(HW_POWER_STR, json::object())
      .value(HW_GPIO_PIN, -1);
}

int Config::powerPinValue()
{
  return data.value(C_HARDWARE_STR, json::object())
      .value(HW_POWER_STR, json::object())
      .value(HW_GPIO_VAL, -1);
}

// WebSocket Config
string Config::webSocketAddress()
{
  return data.value(C_WS_ADDRESS_STR, string(""));
}
