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

// Respeaker Config
string Config::kwsModelName()
{
  return data[C_RESPEAKER_STR][RSP_KWS_MODEL_STR];
}

string Config::kwsSensitivityLevel()
{
  return data[C_RESPEAKER_STR][RSP_KWS_SENSITIVITY_STR];
}

int Config::listeningTimeout()
{
  return data[C_RESPEAKER_STR][RSP_LISTENING_TIMEOUT_STR];
}

int Config::wakeWordDetectionOffset()
{
  return data[C_RESPEAKER_STR][RSP_WAKEWORD_DETECTION_OFFSET_STR];
}

int Config::gainLevel()
{
  return data[C_RESPEAKER_STR][RSP_GAIN_LEVEL_STR];
}

bool Config::doAGC()
{
  return data[C_RESPEAKER_STR][RSP_AGC_STR];
}

bool Config::doWaveLog()
{
  return data[C_RESPEAKER_STR][RSP_WAV_LOG_STR];
}

bool Config::isSingleBeamOutput()
{
  return data[C_RESPEAKER_STR][RSP_SINGLE_BEAM_OUTPUT_STR];
}

// Pixel Ring Config
string Config::hardwareModelName()
{
  return data[C_HARDWARE_STR][HW_MODEL_STR];
}

string Config::idleColor()
{
  return data[C_PIXEL_RING_STR][PR_IDLE_COLOR_STR];
}

string Config::listenColor()
{
  return data[C_PIXEL_RING_STR][PR_LISTEN_COLOR_STR];
}

string Config::speakColor()
{
  return data[C_PIXEL_RING_STR][PR_SPEAK_COLOR_STR];
}

string Config::muteColor()
{
  return data[C_PIXEL_RING_STR][PR_MUTE_COLOR_STR];
}

string Config::unmuteColor()
{
  return data[C_PIXEL_RING_STR][PR_UNMUTE_COLOR_STR];
}

bool Config::isIdleAnimationEnabled()
{
  return data[C_PIXEL_RING_STR][PR_ON_IDLE_STR];
}

bool Config::isListenAnimationEnabled()
{
  return data[C_PIXEL_RING_STR][PR_ON_LISTEN_STR];
}

bool Config::isSpeakAnimationEnabled()
{
  return data[C_PIXEL_RING_STR][PR_ON_SPEAK_STR];
}

bool Config::isMuteAnimationEnabled()
{
  return data[C_PIXEL_RING_STR][PR_TO_MUTE_STR];
}

bool Config::isUnmuteAnimationEnabled()
{
  return data[C_PIXEL_RING_STR][PR_TO_UNMUTE_STR];
}

bool Config::shouldMute()
{
  return data[C_PIXEL_RING_STR][PR_MUTE_STR];
}

int Config::brightness()
{
  return data[C_PIXEL_RING_STR][PR_LED_BRI_STR];
}

int Config::ledsAmount()
{
  return data[C_HARDWARE_STR][HW_LED_NUM];
}

int Config::spiBusNumber()
{
  return data[C_HARDWARE_STR][HW_LED_SPI_BUS];
}

int Config::spiDevNumber()
{
  return data[C_HARDWARE_STR][HW_LED_SPI_DEV];
}

int Config::powerPin()
{
  return data[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_PIN];
}

int Config::powerPinValue()
{
  return data[C_HARDWARE_STR][HW_POWER_STR][HW_GPIO_VAL];
}

// WebSocket Config
string Config::webSocketAddress()
{
  return data[C_WS_ADDRESS_STR];
}
