#ifndef SOUND_CONTROLLER_C
#define SOUND_COMNTROLLER_C

#include <alsa/asoundlib.h>
#include "log.hpp"

#define PCM_DEVICE "default"

class SoundController
{
private:
  unsigned int tmp, dir, rate;
	int pcm, channels, seconds;
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	char *buff;
	int buff_size, loops;
public:
  SoundController(unsigned int rate, int channels, int seconds);
  void play();
};

#endif
