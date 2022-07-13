#include "respeaker_core.hpp"

RespeakerCore::RespeakerCore(const Config& config)
{
  interrupt = false;
  string inputSource = "default";
  string kwsPath = string(getenv("PWD")) + "/models/";
  string kwsResourcesPath = kwsPath + "common.res";
  string kwsModelPath = kwsPath + config.kwsModelName();
#ifndef EMULATION
  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, config.isSingleBeamOutput(), 6, config.doWaveLog()));
  hotwordNode.reset(SnowboyMbDoaKwsNode::Create(kwsResourcesPath, kwsModelPath, config.kwsSensitivityLevel(), 10, config.doAGC()));
  
  if (config->doAGC()) {
    hotwordNode->SetAgcTargetLevelDbfs(config.gainLevel());
  }
  hotwordNode->DisableAutoStateTransfer();

  // Create audio DSP chain: convert Pulse audio to 16k -> do beamforming, AEC, NR -> detect hotword.
  beamformingNode->Uplink(collectorNode.get());
  hotwordNode->Uplink(beamformingNode.get());

  respeaker.reset(ReSpeaker::Create(INFO_LOG_LEVEL));
  respeaker->RegisterChainByHead(collectorNode.get());
  respeaker->RegisterOutputNode(hotwordNode.get());
  respeaker->RegisterDirectionManagerNode(hotwordNode.get());
  respeaker->RegisterHotwordDetectionNode(hotwordNode.get());
#endif
}

bool RespeakerCore::startListening()
{
#ifndef EMULATION
  return respeaker->Start(&interrupt);
#else
  return true;
#endif
}

int RespeakerCore::channels()
{
#ifndef EMULATION
  return respeaker->GetNumOutputChannels();
#else
  return 5;
#endif
}

int RespeakerCore::rate()
{
#ifndef EMULATION
  return respeaker->GetNumOutputRate();
#else
  return 42;
#endif
}

string RespeakerCore::processAudio(int& detected)
{
#ifndef EMULATION
  return respeaker->DetectHotword(detected);
#else
  detected = 0;
  return "";
#endif
}

int RespeakerCore::soundDirection()
{
#ifndef EMULATION
  return respeaker->GetDirection();
#else
  return 0;
#endif
}

void RespeakerCore::stopAudioProcessing()
{
  interrupt = true;
#ifndef EMULATION
  respeaker->Stop();
#endif
}
