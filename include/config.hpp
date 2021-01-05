#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstring>
#include <fstream>
#include <cstdlib>

// See JSON lib docs: https://github.com/nlohmann/json (included as a prebuilt single header)
#include "json.hpp"
#include "common.h"

using namespace std;
using json = nlohmann::json;

class Config
{
private:
  json data;
public:
  Config(const char* name);
  bool isRead();

  // Respeaker
  string kwsModelName();
  string kwsSensitivityLevel();
  int listeningTimeout();
  int gainLevel();
  bool doAGC();
  bool doWaveLog();
  bool isSingleBeamOutput();

  // Pixel Ring
  string hardwareModelName();
  string idleColor();
  string listenColor();
  string speakColor();
  string muteColor();
  string unmuteColor();
  int brightness();
  int ledsAmount();
  int spiBusNumber();
  int spiDevNumber();
  int powerPin();
  int powerPinValue();
  bool isIdleAnimationEnabled();
  bool isListenAnimationEnabled();
  bool isSpeakAnimationEnabled();
  bool isMuteAnimationEnabled();
  bool isUnmuteAnimationEnabled();
  bool shouldMute();

  // WebSocket
  string webSocketAddress();
};

#endif
