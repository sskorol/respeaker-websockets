#ifndef RESPEAKER_CORE_HPP
#define RESPEAKER_CORE_HPP

#define BLOCK_SIZE_MS 8

// Audio DSP provided by Alango: https://wiki.seeedstudio.com/ReSpeaker_Core_v2.0/#closed-source-solution
#include <respeaker.h>
#include <chain_nodes/pulse_collector_node.h>
#include <chain_nodes/vep_aec_beamforming_node.h>
#include <chain_nodes/snowboy_mb_doa_kws_node.h>
#include <memory>

#include "config.hpp"

using namespace respeaker;

class RespeakerCore
{
private:
  // Respeaker config: http://respeaker.io/librespeaker_doc/index.html
  unique_ptr<PulseCollectorNode> collectorNode;
  unique_ptr<VepAecBeamformingNode> beamformingNode;
  unique_ptr<SnowboyMbDoaKwsNode> hotwordNode;
  unique_ptr<ReSpeaker> respeaker;
public:
  RespeakerCore(Config* config);
  bool startListening(bool* interrupt);
  int channels();
  int rate();
  int soundDirection();
  void stopAudioProcessing();
  string processAudio(int& detected);
};

#endif
