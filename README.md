## Respeaker WebSocket Client

This project is a quick start guide for revealing Alango DSP algorithms bundled into Respeaker library. Basically, it allows sending pre-processed audio stream to custom ASR engine via WebSockets.

### Requirements

Make sure you've already installed **librespeaker** on your Respeaker Core V2 board. You can find required dependencies in the official [respeakerd installation script](https://github.com/respeaker/respeakerd/blob/master/scripts/install_all.sh#L37-L43). Or just run the entire script until you reach the Alexa auth step. Note that the above script was created for AVS integration only. So we don't need to run all the instructions listed in the provided script.

This project also depends on [IXWebSocket library](https://machinezone.github.io/IXWebSocket/) which was manually built and added as a static lib. However, if for some reason further compilation will fail for you (or you need an advanced socket configuration), try to rebuild IXWebSocket manually following the official guide.

LED animation is implemented based on [snips-respeaker-skill](https://github.com/snipsco/snips-skill-respeaker) sources.

Setup [VOSK ASR server](https://github.com/alphacep/vosk-server/blob/master/websocket/asr_server.py), which supports different languages. Check the official guide on their webpage. We'll use this server later for sending audio chunks from Respeaker board.

### Configuration

Adjust **config.json** with required values. Note that it'll be automatically copied to the build folder.
```json
{
  "webSocketAddress": "ws://127.0.0.1:2700",
  "respeaker": {
    "kwsModelName": "snowboy.umdl",
    "kwsSensitivity": 0.6,
    "listeningTimeout": 8000,
    "wakeWordDetectionOffset": 300,
    "gainLevel": 7
  },
  "pixelRing": {
    "ledBrightness": 31,
    "onIdle": true,
    "onListen": true,
    "onSpeak": true,
    "toMute": true,
    "toUnmute": true,
    "idleColor": "teal",
    "listenColor": "blue",
    "speakColor": "purple",
    "muteColor": "yellow",
    "unmuteColor": "green",
    "mute": false
  },
  "hardware": {
    "model": "Respeaker Core V2",
    "ledsAmount": 12,
    "spiBus": 0,
    "spiDev": 0,
    "power": {
      "gpioPin": 66,
      "gpioVal": 0
    }
  }
}
```

### Installation

```shell script
git clone https://github.com/sskorol/respeaker-websockets.git
cd respeaker-websockets && mkdir build && cd build
cmake ..
make -j
```

This script will produce **respeaker-core** executable in the build folder.

### Running

Make sure you have VOSK or other ASR server running. By default **RespeakerCore** uses localhost address trying to establish connection with WS server. You may want to change it to the actual server's address and rebuild.

User the following commands to start a speech streaming process:
```shell script
cd build
./respeaker-core
```

You should see a configuration log and a message about successfull connectivity to WS server and Pixel Ring setup.

Current app's logic assumes the following chain:

- Apply rate conversion, beamforming, acoustic echo cancellation and noise suppression to the input audio stream.
- Wake word detection ("snowboy" is a default one). You can change it in **config.json**. Note that the default location is hardcoded, assuming you've executed respeaker setup script.
- When wake word is detected, you will see it in log, as well as an angle which was tracked by DOA (direction of arrival) algorithm. Moreover, a Pixel Ring color state is changed to notify user so that they can start dictating.
- We give a 300ms delay to prevent sending audio chunks to the WS server. It's required for the hotword's filtering which we don't wanna get a transcribe for.
- Send audio chunks to WS server until we receive a final transcribe or reach a 8s timeout. Transcibe or timeout event also changes Pixel Ring state, which becomes idle.

It's recommended you'll check **respeaker-core.cpp** source code and comments to understand what's going on there, and customize it for your own needs.

### ToDo

- [ ] Refactor code in an object-oriented manner.
- [ ] Implement Google / Echo [patterns](https://github.com/respeaker/pixel_ring/blob/master/pixel_ring/pattern.py) to control Pixel Ring.
