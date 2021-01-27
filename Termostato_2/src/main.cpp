#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define redLed D9
#define greenLed D10
#define blueLed D11

#define rotation A0
#define air A1
#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define onSwitch D2
#define offSwitch D3

const int fireTemperature = 50;
const int airQualityLimit = 50;

const char *ssid = "MiFibra-36E9-plus";
const char *password = "XMe3cpbk";

#define TOPIC "1111111A/1/1/1"
#define BROKER_IP "192.168.1.13"
#define BROKER_PORT 2883

WiFiClient espClient;
PubSubClient client(espClient);

void wifiConnect()
{
   WiFi.begin(ssid, password);

   while (WiFi.status() != WL_CONNECTED)
   {
      delay(1000);
      Serial.println("Connecting to WiFi..");
   }

   Serial.println("Connected to the WiFi network");
   Serial.print("IP Address: ");
   Serial.println(WiFi.localIP());
}

void mqttConnect()
{
   client.setServer(BROKER_IP, BROKER_PORT);
   while (!client.connected())
   {
      Serial.print("MQTT connecting ...");

      if (client.connect("ESP32Client1"))
      {
         Serial.println("connected");
      }
      else
      {
         Serial.print("failed, status code =");
         Serial.print(client.state());
         Serial.println("try again in 5 seconds");

         delay(5000); //* Wait 5 seconds before retrying
      }
   }
}

static enum { ST_RED,
              ST_GREEN,
              ST_BLUE,
              ST_NONE } next_state;

TaskHandle_t t2;
TaskHandle_t t3;
TaskHandle_t t5;
TaskHandle_t t6;
TaskHandle_t t7;

QueueHandle_t q1_humidity;
QueueHandle_t q2_temperature;
QueueHandle_t q3_air;

void IRAM_ATTR on_handleInterrupt()
{
   vTaskResume(t2);
   vTaskResume(t3);
   vTaskResume(t5);
   vTaskResume(t6);
   vTaskResume(t7);
}

void IRAM_ATTR off_handleInterrupt()
{
   next_state = ST_NONE;
   vTaskSuspend(t2);
   vTaskSuspend(t3);
   vTaskSuspend(t5);
   vTaskSuspend(t6);
   vTaskSuspend(t7);
}

void control_RGB_leds(unsigned char red, unsigned char green, unsigned char blue)
{
   digitalWrite(redLed, red);
   digitalWrite(greenLed, green);
   digitalWrite(blueLed, blue);
}

void vTaskRGB(void *pvParam)
{
   for (;;)
   {
      switch (next_state)
      {
      case ST_RED:
         control_RGB_leds(1, 0, 0);
         break;

      case ST_GREEN:
         control_RGB_leds(0, 0, 1);
         break;

      case ST_BLUE:
         control_RGB_leds(0, 1, 0);
         break;
      default:
         control_RGB_leds(0, 0, 0);
         break;
      }
      delay(500);
   }
   vTaskDelete(NULL);
}

void vTask2(void *pvParam)
{
   const portTickType xticks = 250 / portTICK_RATE_MS;
   float temperature_val = 0.0;

   for (;;)
   {
      temperature_val = dht.readTemperature();
      //Serial.printf("Temp. actual = %f\n",temperature_val);

      portBASE_TYPE st = xQueueSendToBack(q2_temperature, &temperature_val, xticks);
      if (st != pdPASS)
         Serial.printf("No puedo enviar Temp Actual\r\n");

      //delay(500);
      vTaskDelay(250 / portTICK_RATE_MS);
   }
   vTaskDelete(NULL);
}

void vTask3(void *pvParam)
{
   float temp_val_rec;
   const portTickType xticks = 250 / portTICK_RATE_MS;
   for (;;)
   {
      portBASE_TYPE st_temp;
      st_temp = xQueueReceive(q2_temperature, &temp_val_rec, xticks);
      if (st_temp == pdPASS)
      {
         if (temp_val_rec > fireTemperature)
         {
            Serial.println("Se enciende el aspersor");
         }
         else
         {
            ;
         }
      }
      vTaskDelay(250 / portTICK_RATE_MS);
   }
   vTaskDelete(NULL);
}

void vTaskAirQuality(void *pvParam)
{
   const portTickType xticks = 5000 / portTICK_RATE_MS;
   float air_val = 0.0;

   for (;;)
   {
      air_val = analogRead(air);
      /* Transformamos valor de 0-100 */
      air_val /= 1023;
      air_val *= 100;

      portBASE_TYPE st = xQueueSendToBack(q3_air, &air_val, xticks);
      if (st != pdPASS)
      {
         Serial.printf("No puedo enviar Luz\r\n");
      }
      Serial.printf("Valor de luz: %d\r\n", (int)air_val);

      if ((int)air_val > airQualityLimit)
      {
         next_state = ST_RED;
      }
      else
      {
         next_state = ST_GREEN;
      }

      vTaskDelay(5000 / portTICK_RATE_MS);
   }
   vTaskDelete(NULL);
}

void vTaskSendMQTT(void *pvParam)
{
   float temp_val_rec;
   float air_val_rec;
   float hum_val_rec;
   const portTickType xticks = 1000 / portTICK_RATE_MS;

   for (;;)
   {
      portBASE_TYPE st_hum;
      st_hum = xQueueReceive(q1_humidity, &hum_val_rec, xticks);

      portBASE_TYPE st_temp;
      st_temp = xQueueReceive(q2_temperature, &temp_val_rec, xticks);

      portBASE_TYPE st_air;
      st_air = xQueueReceive(q3_air, &air_val_rec, xticks);

      if (st_temp == pdPASS && st_air == pdPASS && st_hum == pdPASS)
      {
         String jsonData = "{\"temperatura\":" + (String)temp_val_rec + ",\"calidadAire\":" 
               + (String)air_val_rec + + ",\"humedad\":" + (String)hum_val_rec + "}";
         client.publish(TOPIC, jsonData.c_str());
      }
      vTaskDelay(1000 / portTICK_RATE_MS);
   }
   vTaskDelete(NULL);
}

void vTaskHumidity(void *pvParam)
{
   const portTickType xticks = 250 / portTICK_RATE_MS;
   float humidity_val = 0.0;

   for (;;)
   {
      humidity_val = dht.readHumidity();
      Serial.printf("Humedad: %f", humidity_val);

      /*portBASE_TYPE st_hum = xQueueSendToBack(q1_humidity, &humidity_val, xticks);
      if (st_hum != pdPASS)
      {
         Serial.printf("No puedo enviar humedad Actual\r\n");
      }*/
      
      vTaskDelay(250 / portTICK_RATE_MS);
   }
   vTaskDelete(NULL);
}

void setup()
{
   q1_humidity = xQueueCreate(5, sizeof(float));
   q2_temperature = xQueueCreate(5, sizeof(float));
   q3_air = xQueueCreate(5, sizeof(float));
   
   Serial.begin(9600);
   pinMode(A0, INPUT);
   pinMode(A1, INPUT);
   analogReadResolution(10);

   dht.begin();

   pinMode(redLed, OUTPUT);
   pinMode(greenLed, OUTPUT);
   pinMode(blueLed, OUTPUT);

   pinMode(onSwitch, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(onSwitch), on_handleInterrupt, FALLING);
   pinMode(offSwitch, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(offSwitch), off_handleInterrupt, FALLING);

   /*Anadimos las tareas de medicion de temperatura y rotacion*/
   xTaskCreatePinnedToCore(vTaskRGB, "Task RGB", 4000, NULL, 1, NULL, 1);
   xTaskCreatePinnedToCore(vTask2, "Task 2", 4000, NULL, 3, &t2, 1);
   xTaskCreatePinnedToCore(vTask3, "Task 3", 4000, NULL, 3, &t3, 1);
   xTaskCreatePinnedToCore(vTaskAirQuality, "Task Air", 4000, NULL, 3, &t5, 1);
   xTaskCreatePinnedToCore(vTaskSendMQTT, "Task MQTT", 4000, NULL, 3, &t6, 1);
   xTaskCreatePinnedToCore(vTaskHumidity, "Task Humidity", 4000, NULL, 3, &t7, 1);

   vTaskSuspend(t2);
   vTaskSuspend(t3);
   vTaskSuspend(t5);
   vTaskSuspend(t6);

   next_state = ST_NONE;

   wifiConnect();
   mqttConnect();
}

void loop()
{
}
