#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* ssid = "IoTnet";
const char* password =  "darksecret";

#define TOPIC "esi/room1/temp"
#define BROKER_IP "192.168.0.102"
#define BROKER_PORT 2883

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(D4, DHT11);


void wifiConnect()
{
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void mqttConnect() {
  client.setServer(BROKER_IP, BROKER_PORT);
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");

    if (client.connect("ESP32Client1")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");

      delay(5000);  //* Wait 5 seconds before retrying
    }
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  dht.begin();

  delay(4000);
  wifiConnect();
  mqttConnect();
}


void loop() {
  // put your main code here, to run repeatedly:
  delay(3000);
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String jsonData = "{\"temperature\":"+String(t)+",\"humidity\":"+String(h)+"}";
  client.publish(TOPIC, jsonData.c_str());
}
