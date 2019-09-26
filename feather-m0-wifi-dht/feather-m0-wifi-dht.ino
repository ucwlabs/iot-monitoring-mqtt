/*
 * Environment monitoring with MQTT, InfluxDB, and Grafana
 * Adafruit Feather M0 Wifi with DHT sensor
 * Copyright 2019 HackTheBase - UCW Labs Ltd. All rights reserved.
 *
 * The example was created for the HackTheBase IoT Hub Lab.
 *
 * The HackTheBase IoT Hub Lab is a creative space dedicated to prototyping and inventing, 
 * all sorts of microcontrollers (Adafruit Feather, Particle Boron, ESP8266, ESP32 and more) 
 * with various types of connectivity (WiFi, LoRaWAN, GSM, LTE-M), and tools such as screwdriver,
 * voltmeter, wirings, as well as an excellent program filled with meetups, workshops, 
 * and hackathons to the community members. 
 *
 * https://hackthebase.com/iot-hub-lab
 * 
 * The HackTheBase IoT Hub will give you access to the infrastructure dedicated to your project,
 * and you will get access to our Slack community.
 *
 * https://hackthebase.com/iot-hub
 *
 * As a member, you will get access to the hardware and software resources 
 * that can help you to work on your IoT project.
 *
 * https://hackthebase.com/register
 *
 */

#include <SPI.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include <DHT.h>

#define VBATPIN A7
#define LED  6
#define DHTPIN  9
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// WiFi connection
const char WIFI_SSID[] = "your_ssid";     // your network SSID (name)
const char WIFI_PASS[] = "your_password"; // your network password (use for WPA, or use as key for WEP)
byte mac[6];
int status = WL_IDLE_STATUS;

// MQTT connection 
#define MQTT_BROKER   "01.mqtt.services.unitycloudware.net"
#define MQTT_PORT     1883
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "admin"

// MQTT topics
#define TOPIC_SENSOR_DHT_DATA   "/sensor/dht/data"
#define TOPIC_SENSOR_DHT_STATUS "/sensor/dht/status"
#define TOPIC_SENSOR_DHT_LED    "/sensor/dht/led"

WiFiClient httpClient;
PubSubClient mqttClient(httpClient);

unsigned long lastConnectionTime = 0;               // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 15L * 1000L;  // delay between updates, in milliseconds

void setup() {
  setupSerialPorts();
  setupWifi();
  
  // set server
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  // callback
  mqttClient.setCallback(callback);

  // initialise DHT
  dht.begin();

  pinMode(LED, OUTPUT);

  /*
   * Feather M0 Analog Input Resolution
   * https://forums.adafruit.com/viewtopic.php?f=57&t=103719&p=519042
   */
  analogReadResolution(12);
}

void setupSerialPorts() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  
  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}
}

void setupWifi() {
  /*
   * Adafruit Feather M0 WiFi with ATWINC1500
   * https://learn.adafruit.com/adafruit-feather-m0-wifi-atwinc1500/downloads?view=all
   */
   
  WiFi.setPins(8, 7, 4, 2); // Configure pins for Adafruit ATWINC1500 Feather
  
  // Check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // Attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait 10 seconds for connection:
    delay(10000);
  }
  
  Serial.println("Connected to WiFi!");
  
  printWifiStatus();
}

void printWifiStatus() {
  Serial.println();
  
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  WiFi.macAddress(mac);
  Serial.print("MAC Address: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
  
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  Serial.print("WiFi firmware version: ");
  Serial.println(WiFi.firmwareVersion());

  Serial.println();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
    
  } else {
    mqttClient.loop();
  }

  collectData();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "DHTSensor-";  
    clientId += getMacAddress(mac);
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      mqttClient.subscribe(TOPIC_SENSOR_DHT_LED);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

String getMacAddress(byte mac[]) {
  String s;

  for (int i = 5; i >=0; i--) {
    char buf[3];
    sprintf(buf, "%2X", mac[i]);
    s += buf;
  }
  
  return s;
}

void collectData() {
  if ((millis() - lastConnectionTime) > postingInterval) {
    String data = "";
    String deviceStatus = "";

    Serial.print("Data = ");
    data = readData();
    Serial.print(data);
    Serial.println("");

    if (data.length() > 0) {
      sendData(TOPIC_SENSOR_DHT_DATA, data);
    }

    Serial.print("Status = ");
    deviceStatus = readStatus();
    Serial.print(deviceStatus);
    Serial.println("");

    if (deviceStatus.length() > 0) {
      sendData(TOPIC_SENSOR_DHT_STATUS, deviceStatus);
    }
  }
}

String readData() {
  String data = "";
  
  // Read humidity
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return data;
  }
  
  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
    
  data = "{\"humidity\": %humidity,\"temperature\": %temperatureC,\"temperatureF\": %temperatureF,\"heat_index\": %heat_indexC,\"heat_indexF\": %heat_indexF}";
  data.replace("%humidity", String(h));
  data.replace("%temperatureC", String(t));
  data.replace("%temperatureF", String(f));
  data.replace("%heat_indexC", String(hic));
  data.replace("%heat_indexF", String(hif));
    
  return data;
}

String readStatus() {
  String data = "";

  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.30; // Multiply by 3.3V, our reference voltage
  measuredvbat /= 4096; // convert to voltage

  float batteryV = measuredvbat;
  float batteryPcnt = ((batteryV - 3.20) / 3.30) * 100;

  data = "{\"batteryV\": %batteryV,\"batteryP\": %batteryP}";
  data.replace("%batteryV", String(batteryV));
  data.replace("%batteryP", String(batteryPcnt));

  return data;
}

void sendData(String topic, String payload) {
  if (payload.length() < 1) {
    Serial.println("No data to send!");
    return;
  }
    
  mqttClient.publish(topic.c_str(), payload.c_str());

  // Note the time that the connection was made
  lastConnectionTime = millis();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  
  Serial.println();

  if (strcmp(topic, TOPIC_SENSOR_DHT_LED) == 0) {
    int value = atoi((char*) payload);
    Serial.print("Value: ");
    Serial.println(value);

    if (value > 0) {
      digitalWrite(LED, HIGH);

    } else {
      digitalWrite(LED, LOW);
    }
  }
}
