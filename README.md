## Respeaker WebSocket Client

This project is a quick start guide for revealing [Alango](http://www.alango.com/) DSP algorithms bundled into Respeaker library. Basically, it allows sending pre-processed audio stream to custom ASR engine via WebSockets.

### Requirements

Make sure you installed all the required dependencies on your Respeaker Core V2 board (assuming you already have their [official debian distribution](http://respeaker.seeed.io/images/respeakerv2/debian/20180801/respeaker-debian-9-lxqt-sd-20180801-4gb.img.xz)):

```shell script
sudo apt-get update && sudo apt-get -y upgrade
sudo apt-get install -y \
  cmake \
  mraa-tools \
  zlib1g-dev \
  librespeaker-dev \
  libsndfile1-dev \
  libasound2-dev
```

Don't forget to reboot your board after installation!

This project also depends on [IXWebSocket library](https://machinezone.github.io/IXWebSocket/). You can build it the following way:

```shell script
git clone https://github.com/machinezone/IXWebSocket.git
cd IXWebSocket && mkdir build && cd build
cmake ..
make -j
sudo make install
```

Setup [Vosk ASR server](https://github.com/sskorol/asr-server). We'll use this server later for sending audio chunks from ReSpeaker board.

![image](https://user-images.githubusercontent.com/6638780/102908650-6ec77480-4480-11eb-8bfd-b8f3c65efd79.png)

### Installation

Pull source code:

```shell script
git clone https://github.com/sskorol/respeaker-websockets.git
cd respeaker-websockets && mkdir build
```

Adjust **config.json** with required values. Note that it'll be automatically copied to the build folder.
```json
{
  "webSocketAddress": "ws://127.0.0.1:2700",
  "respeaker": {
    "kwsModelName": "snowboy.umdl",
    "kwsSensitivity": "0.6",
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

You can use any of the hotwords located in **models** folder.

Build source code:

```shell script
cd build
cmake ..
make -j
```

This script will produce **respeaker_core** executable in the build folder.

### Running

Make sure you have VOSK or other ASR server running. By default **respeaker_core** uses localhost address trying to establish connection with WS server. You may want to change it to the actual server's address.

Use the following commands to start a speech streaming process:
```shell script
./respeaker_core
```

You should see a configuration log and a message about successfull connectivity to WS server and Pixel Ring (implemented based on [snips-respeaker-skill](https://github.com/snipsco/snips-skill-respeaker) sources).

Current app's logic assumes the following chain:

- Apply rate conversion, beamforming, acoustic echo cancellation, noise suppression and automatic gain control to the input audio stream.
- Wake word detection ("snowboy" is a default one). You can change it in **config.json**.
- When wake word is detected, you will see it in log, as well as the direction which is tracked by DOA (direction of arrival) algorithm. Moreover, a Pixel Ring color state is changed to notify user so that they can start dictating.
- Then we check if the pre-processed chunk is not a hotword to prevent sending it to the WS server. It's required for the hotword's filtering which we don't wanna get a transcribe for.
- Send audio chunks to WS server until we receive a final transcribe or reach a 8s timeout. Transcibe or timeout event also changes Pixel Ring state, which becomes idle.

It's recommended you'll check [main.cpp](https://github.com/sskorol/respeaker-websockets/blob/master/src/main.cpp) source code and comments to understand what's going on there, and customize it for your own needs.

### Running as a Service

Install nodejs:
```shell script
curl -sL https://deb.nodesource.com/setup_14.x | sudo bash -
sudo apt-get install -y nodejs
```

Install [pm2](https://pm2.keymetrics.io/docs/usage/quick-start/):
```shell script
sudo npm install pm2@latest -g
```

Create pm2 startup script:
```schell script
pm2 startup -u respeaker --hp /home/respeaker
sudo env PATH=$PATH:/usr/bin pm2 startup systemd -u respeaker --hp /home/respeaker
```

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

Restart pm2 service:
```shell script
sudo systemctl daemon-reload
sudo systemctl restart pm2-respeaker
```

Add **respeaker_core** binary to pm2:
```shell script
pm2 start respeaker_core --watch --name asr --time
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
