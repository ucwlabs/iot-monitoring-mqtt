/*
 * Environment monitoring with MQTT, InfluxDB, and Grafana
 * MQTT Bridge
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

const mqtt = require('mqtt');
const Influx = require("influx");

const mqttUsername = "admin";
const mqttPassword = "admin";

const influx = new Influx.InfluxDB({
  host: 'influxdb',
  port: 8086,
  database: 'iot-monitoring',
  username: 'root',
  password: 'root'
});

influx.getDatabaseNames()
  .then(names => {
    if (!names.includes('iot-monitoring')) {
      return influx.createDatabase('iot-monitoring');
    }
  });

let writeDataToInflux = (data) => {
  influx
      .writePoints(
          [
              {
                measurement: 'temperature',
                tags: {
                  deviceId: data.deviceId
                },
                fields: {
                  value: data.payload.temperature
                }
              },
              {
                measurement: 'humidity',
                tags: {
                  deviceId: data.deviceId
                },
                fields: {
                  value: data.payload.humidity
                }
              }
           ]
      )
      .catch(error => {
        console.error("Error writing data to Influx - " + error);
      });
};

const main = async function () {
  console.log("MQTT Bridge started.");

  const topicSensorDhtData = "/sensor/dht/data";
  const topicSensorDhtStatus = "/sensor/dht/status";

  let mqttOptions = {
    clientId: "dht-sensor01",
    username: mqttUsername,
    password: mqttPassword,
    clean:true
  };

  const mqttClient = mqtt.connect("mqtt://01.mqtt.services.unitycloudware.net:1883", mqttOptions);
  console.log("connected flag = " + mqttClient.connected);

  mqttClient.subscribe(topicSensorDhtData, {qos:1});
  mqttClient.subscribe(topicSensorDhtStatus, {qos:1});

  // Handle incoming messages
  mqttClient.on('message', function(topic, message, packet) {
    console.log("Message arrived [" + topic + "] " + message);

    if (topic.toString().trim() === topicSensorDhtData) {
      let data = {
        deviceId: "dht-sensor01",
        payload: JSON.parse(message.toString())
      };
      console.log(data);
      writeDataToInflux(data);
    }
  });

  mqttClient.on("connect", function() {
    console.log("connected = "+ mqttClient.connected);
  })
};

main().catch(function (error) {
  console.error("Error", error);
  process.exit(1);
});
