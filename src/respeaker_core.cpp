#include "respeaker_core.hpp"

RespeakerCore::RespeakerCore(Config* config)
{
  string inputSource = "default";
  string kwsPath = string(getenv("PWD")) + "/models/";
  string kwsResourcesPath = kwsPath + "common.res";
  string kwsModelPath = kwsPath + config->kwsModelName();

  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, config->isSingleBeamOutput(), 6, config->doWaveLog()));
  hotwordNode.reset(SnowboyMbDoaKwsNode::Create(kwsResourcesPath, kwsModelPath, config->kwsSensitivityLevel(), 10, config->doAGC()));
  
  if (config->doAGC()) {
    hotwordNode->SetAgcTargetLevelDbfs(config->gainLevel());
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
}

bool RespeakerCore::startListening(bool* interrupt)
{
  return respeaker->Start(interrupt);
}

int RespeakerCore::channels()
{
  return respeaker->GetNumOutputChannels();
}

int RespeakerCore::rate()
{
  return respeaker->GetNumOutputRate();
}

string RespeakerCore::processAudio(int& detected)
{
  return respeaker->DetectHotword(detected);
}

int RespeakerCore::soundDirection()
{
  return respeaker->GetDirection();
}

void RespeakerCore::stopAudioProcessing()
{
  respeaker->Stop();
}
