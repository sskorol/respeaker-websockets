## Respeaker WebSocket Client

This project is a quick start guide for revealing [Alango](http://www.alango.com/) DSP algorithms bundled into Respeaker library. Basically, it allows sending pre-processed audio stream to custom ASR engine via WebSockets.

### Requirements

Make sure you've already installed **librespeaker** on your Respeaker Core V2 board. You can find required dependencies in the official [respeakerd installation script](https://github.com/respeaker/respeakerd/blob/master/scripts/install_all.sh#L37-L43). Or just run the entire script until you reach the Alexa auth step. Note that the above script was created for AVS integration only. So we don't need to run all the instructions listed in the provided script.

This project also depends on [IXWebSocket library](https://machinezone.github.io/IXWebSocket/) which was manually built and added as a static lib. However, if for some reason you need an advanced socket configuration, try to rebuild **IXWebSocket** manually following the official guide.

Setup [Vosk ASR server](https://github.com/sskorol/asr-server). We'll use this server later for sending audio chunks from ReSpeaker board.

![image](https://user-images.githubusercontent.com/6638780/102908650-6ec77480-4480-11eb-8bfd-b8f3c65efd79.png)

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
    "gainLevel": 10,
    "singleBeamOutput": false,
    "enableWavLog": false,
    "agc": true
  },
  "pixelRing": {
    "ledBrightness": 20,
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
    "isMutedOnStart": false
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

Note that you can use any of the hotwords located in **models** folder.

### Installation

```shell script
git clone https://github.com/sskorol/respeaker-websockets.git
cd respeaker-websockets && mkdir build && cd build
cmake ..
make -j
```

This script will produce **respeaker_core** executable in the build folder.

### Running

Make sure you have VOSK or other ASR server running. By default **respeaker_core** uses localhost address trying to establish connection with WS server. You may want to change it to the actual server's address.

User the following commands to start a speech streaming process:
```shell script
cd build && ./respeaker_core
```

You should see a configuration log and a message about successfull connectivity to WS server and Pixel Ring (implemented based on [snips-respeaker-skill](https://github.com/snipsco/snips-skill-respeaker) sources).

Current app's logic assumes the following chain:

- Apply rate conversion, beamforming, acoustic echo cancellation and noise suppression to the input audio stream.
- Wake word detection ("snowboy" is a default one). You can change it in **config.json**.
- When wake word is detected, you will see it in log, as well as an angle which was tracked by DOA (direction of arrival) algorithm. Moreover, a Pixel Ring color state is changed to notify user so that they can start dictating.
- We give a 300ms delay to prevent sending audio chunks to the WS server. It's required for the hotword's filtering which we don't wanna get a transcribe for.
- Send audio chunks to WS server until we receive a final transcribe or reach a 8s timeout. Transcibe or timeout event also changes Pixel Ring state, which becomes idle.

It's recommended you'll check [respeaker_core](https://github.com/sskorol/respeaker-websockets/blob/master/src/respeaker_core.cpp) source code and comments to understand what's going on there, and customize it for your own needs.

### Running as a Service

Install nodejs:
```shell script
curl -sL https://deb.nodesource.com/setup_14.x | bash -
apt-get install -y nodejs
```

Install [pm2](https://pm2.keymetrics.io/docs/usage/quick-start/):
```shell script
npm install pm2@latest -g
```

Create pm2 startup script:
```schell script
pm2 startup -u respeaker --hp /home/respeaker
```

This command will produce further instructions you need to follow to complete pm2 startup script setup.

Open pm2 service for editing:
```shell script
sudo nano /etc/systemd/system/pm2-respeaker.service
```

Adjust **Unit** block with the following options:
```shell script
Wants=network-online.target
After=network.target network-online.target
```

Adjust **Service** block with the following option:
```shell script
LimitRTPRIO=99
```

It's very important to set this limit (also known as **ulimit -r**). Otherwise, you won't be able to start this service on boot.

Adjust **Install** block with the following option:
```shell script
WantedBy=multi-user.target network-online.target
```

Add **respeaker-core** binary to pm2:
```shell script
pm2 start /home/respeaker/path/to/respeaker-core --watch --name asr --time
```

Save current process list:
```shell script
pm2 save
```

### Demo

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/IAASoRu2ANU/0.jpg)](https://www.youtube.com/watch?v=IAASoRu2ANU)

### ToDo

- [ ] Refactor code in an object-oriented manner.
- [ ] Implement Google / Echo [patterns](https://github.com/respeaker/pixel_ring/blob/master/pixel_ring/pattern.py) to control Pixel Ring.
