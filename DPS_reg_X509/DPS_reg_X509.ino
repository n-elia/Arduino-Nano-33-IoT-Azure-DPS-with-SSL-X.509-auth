#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <utility/ECCX08SelfSignedCert.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>

#include "secrets.h"

// ================================== SETTINGS ===================================
// Set self_signed_cert to true for using the crypto chip stored certificate
// Set self_signed_cert to false to use the hardcoded certificate
bool self_signed_cert = false;
const int keySlot     = 0;  // Crypto chip slot to pick the key from
const int certSlot    = 8;  // Crypto chip slot to pick the certificate from
// ===============================================================================

const char ssid[]           = SECRET_SSID;
const char pass[]           = SECRET_PASS;
const char broker[]         = SECRET_BROKER;
const char idScope[]        = SECRET_ID_SCOPE;
const char registrationId[] = SECRET_REGISTRATION_ID;

WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECCX08
MqttClient    mqttClient(sslClient); // Used for MQTT protocol usage

unsigned long lastMillis = 0;

void setup() {
  // Wait for serial
  Serial.begin(9600);
  while (!Serial);

  // Check the crypto chip module presence (needed for BearSSL)
  if (!ECCX08.begin()) {
    Serial.println("No ECCX08 present!");
    while (1);
  }

  // ================ SSL SETUP ================
  if (self_signed_cert) {
    // Reconstruct the self signed cert
    ECCX08SelfSignedCert.beginReconstruction(keySlot, certSlot);
    // In case of self-signed certificate with ECCX08SelfSignedCert.ino, the ECCX08
    // crypto chip serial number is used as CN. Otherwise, change it here.
    ECCX08SelfSignedCert.setCommonName(ECCX08.serialNumber());
    ECCX08SelfSignedCert.endReconstruction();
  } else if (!self_signed_cert) {
    // Load the CA signed cert from secrets.h
    // const char client_cert[] = CLIENT_CERT;
  }

  // Set a callback to get the current time
  // (used to validate the servers certificate)
  ArduinoBearSSL.onGetTime(getTime);

  // Set the ECCX08 slot to use for the private key
  // and the accompanying public certificate for it
  if (self_signed_cert) {
    sslClient.setEccSlot(
      keySlot,
      ECCX08SelfSignedCert.bytes(),
      ECCX08SelfSignedCert.length()
    );
  } else if (!self_signed_cert) {
    sslClient.setEccSlot(
      keySlot,
      // client_cert);
      CLIENT_CERT);
  }

  // ================ MQTT Client SETUP ================
  // Set the client id used for MQTT as the registrationId
  mqttClient.setId(registrationId);

  // Set the username to "<idScope>/registrations/<registrationId>/api-version=2019-03-31"
  String username;
  username += idScope;
  username += "/registrations/";
  username += registrationId;
  username += "/api-version=2019-03-31";

  // Set an empty password (because of X.509 authentication instead of SAS token)
  String password = "";

  // Authenticate the MQTT Client
  mqttClient.setUsernamePassword(username, password);

  // Set the on message callback, called when the MQTT Client receives a message
  mqttClient.onMessage(onMessageReceived);
}

void loop() {
  // ================ LOOP FUNCTION ================
  // Select the MQTT topic to subscribe to. It is a default value for DPS.
  String sub_topic = "$dps/registrations/res/#";

  // Connect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Establish MQTT connection
  if (!mqttClient.connected()) {
    connectMQTT(sub_topic);
  }

  // Select the MQTT topic to publish to. It is a default value for DPS.
  String pub_topic = "$dps/registrations/PUT/iotdps-register/?$rid=1";

  // Select the MQTT payload to be published. It must be JSON formatted.
  // Example:
  //  {
  //    "registrationId": "mydevice",
  //    "tpm":
  //      {
  //        "endorsementKey": "stuff",
  //        "storageRootKey": "things"
  //      },
  //    "payload": "your additional data goes here. It can be nested JSON."
  //  }
  // (This is a rudimental JSON, you can use ArduinoJSON)
  String pub_msg = "";
  pub_msg  = "{\"registrationId\":\"}";
  pub_msg += registrationId;
  pub_msg += "\"}";

  // Publish the message for requesting the device registration
  publishMessage(pub_topic, pub_msg);

  // Hang on after the device registration
  hangHere();
}

unsigned long getTime() {
  // Get the current time from the WiFi module
  return WiFi.getTime();
}

void connectWiFi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  Serial.print(" ");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the network");
  Serial.println();
}

void connectMQTT(String topic) {
  Serial.print("Attempting to connect to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    delay(5000);
    // Failed, retry
    Serial.print("connectError: ");
    Serial.println(mqttClient.connectError());
  }
  Serial.println();

  Serial.println("You're connected to the MQTT broker");
  Serial.println();

  // Subscribe to the given topic
  mqttClient.subscribe(topic);
}

void publishMessage(String topic, String payload) {
  Serial.println("Publishing message");
  // Use the Print interface to send the message contents
  mqttClient.beginMessage(topic);
  mqttClient.print(payload);
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  // Message received, print the topic and the message
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("'");

  // Use the stream interface to print the message contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }
  Serial.println();
}

void hangHere() {
  while (true) {
    // Poll for new MQTT messages and send keep alives
    mqttClient.poll();
  }
}
