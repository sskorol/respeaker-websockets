#include "respeaker_core.hpp"

RespeakerCore::RespeakerCore(Config* config)
{
  string inputSource = "default";
  string kwsPath = string(getenv("PWD")) + "/models/";
  string kwsResourcesPath = kwsPath + "common.res";
  string kwsModelPath = kwsPath + config->kwsModelName();

  collectorNode.reset(PulseCollectorNode::Create_48Kto16K(inputSource, BLOCK_SIZE_MS));
  // ref_channel_index=7. librespeaker header doc says "starts from 0; specify 6 for v2"
  // but empirical test (loopback/pulse_snowboy_mb_test_ref7 vs the ref=6 default) shows
  // index 7 is the only value that puts the codec's playback-reference channel into VEP's
  // ref_in. With ref=6 the speaker leak survives in out_6 at ~24% energy → STT self-loop.
  // With ref=7 the post-AEC beamformed output drops to ~0.2% energy → AEC actually suppresses.
  beamformingNode.reset(VepAecBeamformingNode::Create(CIRCULAR_6MIC_7BEAM, config->isSingleBeamOutput(), 7, config->doWaveLog()));
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
