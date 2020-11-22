#!/bin/bash

# If you prefer building from CLI, make sure you installed librespeaker and libixwebsocket deps.
g++ ./src/respeakerws.cc -o respeakerws -lrespeaker -lixwebsocket -lz -lpthread -fPIC -std=c++14 -fpermissive -I/usr/include/respeaker/ -DWEBRTC_LINUX -DWEBRTC_POSIX -DWEBRTC_NS_FLOAT -DWEBRTC_APM_DEBUG_DUMP=0 -DWEBRTC_INTELLIGIBILITY_ENHANCER=0
