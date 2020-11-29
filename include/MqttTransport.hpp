#ifndef MQTT_TRANSPORT_HPP
#define MQTT_TRANSPORT_HPP

#define MQTT_CLIENT_ID "Respeaker Client"
#define MQTT_QOS 1
#define MQTT_KEEP_ALIVE_INTERVAL 20
#define MQTT_CONNECTION_TIMEOUT 5000
#define MQTT_WAKE_TOPIC "respeaker/led/wake"
#define MQTT_SLEEP_TOPIC "respeaker/led/sleep"
#define MICRO_TIMEOUT 1

// Required for sending events to control LEDs via https://github.com/respeaker/pixel_ring. See docs: https://github.com/eclipse/paho.mqtt.c
#include "MQTTAsync.h"

MQTTAsync client;
MQTTAsync_message message = MQTTAsync_message_initializer;
static bool isMqttConnected = false;

void onDisconnect(void *context, MQTTAsync_successData *response)
{
  cout << "\nDisconnected from MQTT broker" << endl;
  isMqttConnected = false;
}

void onConnectionFailure(void *context, MQTTAsync_failureData *response)
{
  cout << "\nMQTT test connection failed" << endl;
  isMqttConnected = false;
}

void onConnectionLost(void *context, char *cause)
{
  cout << "\nMQTT connection lost. Reconnecting..." << endl;
  isMqttConnected = false;
}

void onConnect(void *context, char *cause)
{
  cout << "\nConnected to MQTT broker" << endl;
  isMqttConnected = true;
}

void sendMqttMessage(const char *topic, int angle = -1)
{
  auto payload = (angle >= 0 ? to_string(angle) : "").c_str();
  message.payload = const_cast<char *>(payload);
  message.payloadlen = strlen(payload);
  message.qos = MQTT_QOS;
  message.retained = 0;
  MQTTAsync_sendMessage(client, topic, &message, NULL);
}

bool connectToMqtt(json config)
{
  string mqttAddress = config["mqttAddress"];
  string mqttUser = config["mqttUser"];
  string mqttPassword = config["mqttPassword"];

  MQTTAsync_create(&client, mqttAddress.c_str(), MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  MQTTAsync_setCallbacks(client, NULL, onConnectionLost, NULL, NULL);
  MQTTAsync_setConnected(client, NULL, onConnect);

  MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
  opts.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
  opts.cleansession = 1;
  opts.automaticReconnect = 1;
  opts.context = client;
  opts.username = mqttUser.c_str();
  opts.password = mqttPassword.c_str();
  opts.onFailure = onConnectionFailure;
  MQTTAsync_connect(client, &opts);

  TimePoint connectTime = SteadyClock::now();
  while (!isMqttConnected && (SteadyClock::now() - connectTime < chrono::milliseconds(MQTT_CONNECTION_TIMEOUT)))
  {
    this_thread::sleep_for(chrono::seconds(MICRO_TIMEOUT));
  }

  return isMqttConnected;
}

void disconnectFromMqtt()
{
  int rc;
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.onSuccess = onDisconnect;

  TimePoint disconnectTime = SteadyClock::now();
  if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
  {
    cout << "\nFailed to disconnect" << endl;
    isMqttConnected = false;
  }

  // Mqtt might not be disconnect at this point yet, so we have to wait until a corresponding flag is set in disconnect handler or timeout occured.
  while (isMqttConnected || (SteadyClock::now() - disconnectTime > chrono::milliseconds(MQTT_CONNECTION_TIMEOUT)))
  {
    this_thread::sleep_for(chrono::seconds(MICRO_TIMEOUT));
  }

  MQTTAsync_destroy(&client);
}

#endif
