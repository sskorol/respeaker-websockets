### Respeaker WebSocket Client

This project is a quick start guide for revealing Alango DSP algorithms bundled into Respeaker library. Basically, it allows sending pre-processed audio stream to custom ASR engine via WebSockets.

#### Requirements

Make sure you've already installed **librespeaker** on your Respeaker Core V2 board. You can find required dependencies in the official [respeakerd installation script](https://github.com/respeaker/respeakerd/blob/master/scripts/install_all.sh#L37-L43). Or just run the entire script until you reach the Alexa auth step. Note that this project was initially created for AVS integration. But we don't need to run all the instructions listed in the provided script.

This project also depends on [IXWebSocket library](https://machinezone.github.io/IXWebSocket/) which was manually built and added as a static lib into **CMakeLists**. However, if for some reason further compilation will fail for you (or you need an advanced socket configuration), try to rebuild IXWebSocket manually following the official guide.

Setup [VOSK ASR server](https://github.com/alphacep/vosk-server/blob/master/websocket/asr_server.py), which supports different languages. Check the official guide on their webpage. We'll use this server later for sending audio chunks from Respeaker board.

#### Installation

```shell script
git clone https://github.com/sskorol/respeaker-websockets.git
cd respeaker-websockets && mkdir build && cd build
cmake ..
make -j
```

This script will produce **respeakerws** executable in the build folder.

Alternatevely, you can run **build.sh** which basically does the same thing. However, it assumes you have **IXWebSocket** lib installed globally.

```shell script
./build.sh
```

#### Running

Make sure you have VOSK or other ASR server running. By default **respeakerws** uses localhost address trying to establish connection with WS server. You may want to change it to the actual server's address and rebuild.

```shell script
cd build
./respeakerws
```

You should see a configuration log and a message about successfull connectivity to WS server.

Continuous dots (".") in log mean that no data is sent to the server yet. Current logic assumes the following chain:

- Apply rate conversion, beamforming, acoustic echo cancellation and noise suppression to the input audio stream.
- Hotword detection ("snowboy" is a default one). You can change it in code or make it configurable.
- When hotword is detected, you will see it in log, as well as an angle which was tracked by DOA (direction of arrival) algorithm.
- We give a 300ms delay to prevent sending audio chunks to the WS server. It's required for the hotword's filtering which we don't wanna get a transcribe for.
- Send audio chunks to WS server until we receive a final transcribe or reach a 8s timeout.

It's also recommended you'll check **respeakerws.cpp** source code and comments to understand what's going on there and customize it for your own needs.
