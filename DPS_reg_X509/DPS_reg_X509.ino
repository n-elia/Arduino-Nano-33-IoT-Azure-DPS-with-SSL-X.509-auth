/*
   Azure IoT Device Provisioning Service registration tool
   This sketch securely connects to an Azure IoT DPS using MQTT over WiFi,
   secured by SSl.
   It uses a private key stored in the built-in crypto chip and a CA signed
   public certificate for SSL/TLS authetication.

   It subscribes to a DPS topic to receive the response, and publishes a message
   to stard the device enrollment.

   Boards:
   - Arduino Nano 33 IoT

   Author: Nicola Elia
   GNU General Public License v3.0
*/

#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <utility/ECCX08SelfSignedCert.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>

#include "secrets.h"

// ================================== SETTINGS ===================================
// Set self_signed_cert to true for using the crypto chip stored certificate
// Set self_signed_cert to false to use an hardcoded certificate
bool self_signed_cert = false;
const int keySlot     = 0;  // Crypto chip slot to pick the key from
const int certSlot    = 8;  // Crypto chip slot to pick the certificate from
// ===============================================================================

const char ssid[]     = SECRET_SSID;
const char pass[]     = SECRET_PASS;
const char broker[]   = SECRET_BROKER;
String idScope        = SECRET_ID_SCOPE;
String registrationId = SECRET_REGISTRATION_ID;

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
  // Set a callback to get the current time
  // (used to validate the servers certificate)
  ArduinoBearSSL.onGetTime(getTime);

  if (self_signed_cert) {
    Serial.println("Loading the self-signed certificate from ECCX08...");
    // Reconstruct the self signed cert
    ECCX08SelfSignedCert.beginReconstruction(keySlot, certSlot);
    // In case of self-signed certificate with ECCX08SelfSignedCert.ino, the ECCX08
    // crypto chip serial number is used as CN by default. Otherwise, change it here.
    ECCX08SelfSignedCert.setCommonName(ECCX08.serialNumber());
    ECCX08SelfSignedCert.endReconstruction();

    // Instruct the SSL client to use the chosen ECCX08 slot for picking the private key
    // and set the reconstructed certificate as accompanying public certificate.
    sslClient.setEccSlot(
      keySlot,
      ECCX08SelfSignedCert.bytes(),
      ECCX08SelfSignedCert.length()
    );
  } else if (!self_signed_cert) {
    Serial.println("Using the certificate from secrets.h...");

    // Instruct the SSL client to use the chosen ECCX08 slot for picking the private key
    // and set the hardcoded certificate as accompanying public certificate.
    sslClient.setEccSlot(
      keySlot,
      CLIENT_CERT);
  }
  
  /*
   * Note: I prefer to use BearSSLClient::setEccSlot because it contains a function to decode
   * the .pem certificate (the hardcoded certificate can be stored in base64, instead of
   * converting it to binary), and it automatically computes the certificate length.
   */
   
  // ================ MQTT Client SETUP ================
  // Set the client id used for MQTT as the registrationId
  mqttClient.setId(registrationId);

  // Set the username to "<idScope>/registrations/<registrationId>/api-version=2019-03-31"
  String username = idScope + "/registrations/" + registrationId + "/api-version=2019-03-31";

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
  String pub_msg = "{\"registrationId\":\"}" + registrationId + "\"}";

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

// Sample output on Serial monitor:
//10:40:12.491 -> Using the certificate from secrets.h...
//10:40:13.378 -> Attempting to connect to SSID: Home&Life SuperWiFi-2FD1 .
//10:40:20.951 -> You're connected to the network
//10:40:20.951 ->
//10:40:20.951 -> Attempting to connect to MQTT broker: global.azure-devices-provisioning.net
//10:40:26.413 -> connectError: -2
//10:40:31.702 -> connectError: -2
//10:40:35.783 ->
//10:40:35.783 -> You're connected to the MQTT broker
//10:40:35.783 ->
//10:40:35.830 -> Publishing message
//10:40:36.298 -> Received a message with topic '$dps/registrations/res/202/?$rid=1&retry-after=3'{"operationId":"***","status":"assigning"}
