## Respeaker WebSocket Client

This project is a quick start guide for revealing [Alango](http://www.alango.com/) DSP algorithms bundled into Respeaker library. Basically, it allows sending pre-processed audio stream to custom ASR engine via WebSockets.

### Requirements

Make sure you installed all the required dependencies on your Respeaker Core V2 board (assuming you already have their [official debian distribution](http://respeaker.seeed.io/images/respeakerv2/debian/20180801/respeaker-debian-9-lxqt-sd-20180801-4gb.img.xz)).

After flashing the image to SD card, you may found that your root partition size is ~4GB. To expand its size to the real SD card size, do the following:
```shell script
fdisk /dev/mmcblk0      # change it to your device name
Command (m for help): p # check available partitions
Command (m for help): d # delete partition
Selected partition 2    # you'll likely have a boot with idx == 1, so select the other one
Command (m for help): p # ensure there's only one (if you had boot) or nothing left
Command (m for help): n # create new partition
Select (default p): p   # primary
Partition number: 2     # the same idx as was deleted
# Leave defaults for first and last sectors
Command (m for help): w # alter partition
sudo reboot
resize2fs /dev/mmcblk1p2 # or whatever idx you've altered
df -h                    # should now show a full size
```

You may also want to add swap:
```shell script
sudo swapon --show
free -h
df -h
sudo fallocate -l 2G /swapfile
ls -lh /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
sudo swapon --show
free -h
sudo cp /etc/fstab /etc/fstab.bak
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
sudo nano /etc/sysctl.conf
 # add -> vm.swappiness=10
 # add -> vm.vfs_cache_pressure=50
```

Edit source lists due to Debian Stretch EOL:
```shell script
sudo nano /etc/apt/sources.list
```

Ensure the following lines:
```shell script
deb http://archive.debian.org/debian stretch main contrib non-free
deb [arch=armhf] http://respeaker.seeed.io/deb stretch main
```

Install required dependencies:
```shell script
sudo apt-get update && sudo apt-get -y upgrade
sudo apt-get install -y \
  cmake \
  mraa-tools \
  zlib1g-dev \
  librespeaker-dev \
  libsndfile1-dev \
  libasound2-dev \
  zip
```

Install CMake 3.10.3:
```shell script
wget https://github.com/Kitware/CMake/releases/download/v3.10.3/cmake-3.10.3.zip
unzip cmake-3.10.3.zip && cd cmake-3.10.3
./bootstrap && make -j$(nproc) && sudo make install
```

Don't forget to reboot your board after installation!

This project also depends on several libraries:

[IXWebSocket](https://machinezone.github.io/IXWebSocket):

```shell script
git clone https://github.com/machinezone/IXWebSocket.git
cd IXWebSocket && mkdir build && cd build
cmake .. -DUSE_ZLIB=1
make -j$(nproc)
sudo make install
```

[FMT](https://github.com/fmtlib/fmt):

```shell script
git clone https://github.com/fmtlib/fmt.git
cd fmt && git checkout tags/8.1.1
mkdir build && cd build
cmake .. -DFMT_TEST=OFF
make -j$(nproc)
sudo make install
```

[SPDLOG](https://github.com/gabime/spdlog):

```shell script
git clone https://github.com/gabime/spdlog.git
cd spdlog && git checkout tags/v1.10.0
mkdir build && cd build
cmake .. -DSPDLOG_FMT_EXTERNAL=ON -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_BUILD_TESTS=OFF -DSPDLOG_BUILD_BENCH=OFF
make -j$(nproc)
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
  "webSocketAddress": "ws://127.0.0.1/api/v1/speech/",
  "respeaker": {
    "kwsModelName": "snowboy.umdl",
    "kwsSensitivity": "0.6",
    "room": "lobby",
    "listeningTimeout": 8000,
    "gainLevel": -7,
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
make -j$(nproc)
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
pm2 start ecosystem.config.js
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
