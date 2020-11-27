#ifndef RESPEAKER_WS_HPP
#define RESPEAKER_WS_HPP

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <fstream>

// Audio DSP provided by Alango: https://wiki.seeedstudio.com/ReSpeaker_Core_v2.0/#closed-source-solution
#include <respeaker.h>
#include <chain_nodes/pulse_collector_node.h>
#include <chain_nodes/vep_aec_beamforming_node.h>
#include <chain_nodes/snowboy_mb_doa_kws_node.h>

// WS client should be built locally first: https://machinezone.github.io/IXWebSocket/build/
#include <ixwebsocket/IXWebSocket.h>

// Required for parsing ASR transcribe. See JSON lib docs: https://github.com/nlohmann/json (included as a prebuilt single header)
#include "json.hpp"

// Required for sending events to control LEDs via https://github.com/respeaker/pixel_ring. See docs: https://github.com/eclipse/paho.mqtt.c
#include "MQTTAsync.h"

// Common definitions
#define CONFIG_FILE "config.json"
#define BLOCK_SIZE_MS 8
#define WS_PING_INTERVAL 45
#define MQTT_CLIENT_ID "Respeaker Client"
#define MQTT_QOS 1
#define MQTT_KEEP_ALIVE_INTERVAL 20

// Json handlers
void readConfig();

// Signal handlers
void handleQuit(int signal);
void configureSignalHandler();

// Mqtt handlers
void onMqttDisconnect(void *context, MQTTAsync_successData *response);
void onMqttConnectionFailure(void *context, MQTTAsync_failureData *response);
void onMqttConnectionLost(void *context, char *cause);
void onMqttConnect(void *context, char *cause);
void sendMqttMessage(const char *topic, int angle);
void configureMqttClient();
void startMqttClient();
void stopMqttClient();

// WS handlers
void configureWSClient();

#endif
