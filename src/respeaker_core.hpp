#ifndef RESPEAKER_CORE_HPP
#define RESPEAKER_CORE_HPP

#define BLOCK_SIZE_MS 8

// Audio DSP provided by Alango: https://wiki.seeedstudio.com/ReSpeaker_Core_v2.0/#closed-source-solution
#ifndef EMULATION
#include <respeaker.h>
#include <chain_nodes/pulse_collector_node.h>
#include <chain_nodes/vep_aec_beamforming_node.h>
#include <chain_nodes/snowboy_mb_doa_kws_node.h>
#endif
#include <memory>

#include "config.hpp"

#ifndef EMULATION
using namespace respeaker;
#endif

class RespeakerCore
{
private:
#ifndef EMULATION
  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
  unique_ptr<PulseCollectorNode> collectorNode;
  unique_ptr<VepAecBeamformingNode> beamformingNode;
  unique_ptr<SnowboyMbDoaKwsNode> hotwordNode;
  unique_ptr<ReSpeaker> respeaker;
#endif
  bool interrupt;
public:
  RespeakerCore(const Config& config);
  bool startListening();
  int channels();
  int rate();
  int soundDirection();
  void stopAudioProcessing();
  string processAudio(int& detected);
};

#endif
