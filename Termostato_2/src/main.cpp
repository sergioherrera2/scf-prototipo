#include <Arduino.h>
#include "DHT.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire);

#define redLed   D9
#define greenLed D10
#define blueLed  D11

#define rotation A0
#define light A1
#define DHTPIN D4 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define onSwitch D2
#define offSwitch D3

static enum {ST_RED,ST_GREEN,ST_BLUE,ST_NONE} next_state;

TaskHandle_t t1;
TaskHandle_t t2;
TaskHandle_t t3;
TaskHandle_t t4;
TaskHandle_t t5;

QueueHandle_t q1_rotation;
QueueHandle_t q2_temperature;
QueueHandle_t q3_light;

void IRAM_ATTR on_handleInterrupt(){
  vTaskResume(t1);
  vTaskResume(t2);
  vTaskResume(t3);
  vTaskResume(t4);
  //vTaskResume(t5);
}

void IRAM_ATTR off_handleInterrupt(){
  next_state = ST_NONE;
  vTaskSuspend(t1);
  vTaskSuspend(t2);
  vTaskSuspend(t3);
  vTaskSuspend(t4);
  //vTaskSuspend(t5);
  display.clearDisplay();
  display.setCursor(1,0);
  display.printf("");
  display.display();
}

void control_RGB_leds(unsigned char red, unsigned char green, unsigned char blue) {
    digitalWrite(redLed, red);
    digitalWrite(greenLed, green);
    digitalWrite(blueLed, blue);
}

void vTaskRGB(void* pvParam)
{
  for(;;) {
    switch (next_state)
    {
    case ST_RED:
      control_RGB_leds(1,0,0);
      break;
    
    case ST_GREEN:
      control_RGB_leds(0,0,1);
      break;

    case ST_BLUE:
      control_RGB_leds(0,1,0);
      break;
    default:
      control_RGB_leds(0,0,0);
      break;
    }
    delay(500);
  }
  vTaskDelete(NULL);
}

void vTask1(void* pvParam)
{
  const portTickType xticks = 250/portTICK_RATE_MS;
  float rotation_val = 0.0;

  for(;;) {
    rotation_val = analogRead(rotation);
    /* Transformamos valor de 0-X ºC*/
    rotation_val /= 1023;
    rotation_val *= 40;
    //Serial.printf("Temp. deseada = %d\n",(int)rotation_val);

    portBASE_TYPE st = xQueueSendToBack(q1_rotation, &rotation_val, xticks);
    if (st != pdPASS)
      Serial.printf("No puedo enviar Temp Deseada\r\n");

    //delay(500);
    vTaskDelay(250/portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void vTask2(void* pvParam)
{
  const portTickType xticks = 250/portTICK_RATE_MS;
  float temperature_val = 0.0;

  for(;;) {
    temperature_val = dht.readTemperature();
    //Serial.printf("Temp. actual = %f\n",temperature_val);

    portBASE_TYPE st = xQueueSendToBack(q2_temperature, &temperature_val, xticks);
    if (st != pdPASS)
      Serial.printf("No puedo enviar Temp Actual\r\n");

    //delay(500);
    vTaskDelay(250/portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void vTask3(void* pvParam)
{
  float rot_val_rec;
  float temp_val_rec;
  const portTickType xticks = 250/portTICK_RATE_MS;
  for(;;) {
    portBASE_TYPE st_rot;
    portBASE_TYPE st_temp;
    st_rot = xQueueReceive(q1_rotation, &rot_val_rec, xticks);
    st_temp = xQueueReceive(q2_temperature, &temp_val_rec, xticks);
    if(st_rot == pdPASS && st_temp == pdPASS){
      if ((int)rot_val_rec > temp_val_rec){
        next_state = ST_RED;
      } else{
        next_state = ST_GREEN;
      }
    }
    //delay(500);
    vTaskDelay(250/portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void vTaskLight(void* pvParam)
{
  const portTickType xticks = 5000/portTICK_RATE_MS;
  float light_val = 0.0;

  for(;;) {
    light_val = analogRead(light);
    /* Transformamos valor de 0-100 */
    light_val /= 1023;
    light_val *= 100;

    //portBASE_TYPE st = xQueueSendToBack(q3_light, &light_val, xticks);
    //if (st != pdPASS)
    //  Serial.printf("No puedo enviar Luz\r\n");
    Serial.printf("Valor de luz: %d\r\n", (int)light_val);

    if ((int)light_val > 50){
      on_handleInterrupt();
    }else{
      off_handleInterrupt();
    }

    //delay(500);
    vTaskDelay(5000/portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void vTaskLCD(void* pvParam)
{
  float rot_val_rec;
  float temp_val_rec;
  const portTickType xticks = 250/portTICK_RATE_MS;
  for(;;) {
    portBASE_TYPE st_rot;
    portBASE_TYPE st_temp;
    st_rot = xQueueReceive(q1_rotation, &rot_val_rec, xticks);
    st_temp = xQueueReceive(q2_temperature, &temp_val_rec, xticks);

    if(st_temp == pdPASS){
      Serial.printf("Recibido %d Temp Actual\r\n", (int)temp_val_rec);
    }
    else
    {
      Serial.printf("No recibo Temp Actual\r\n");
    }
    if(st_rot == pdPASS){
      Serial.printf("Recibido %d Temp Deseada\r\n", (int)temp_val_rec);
    }
    else
    {
      Serial.printf("No recibo Temp Deseada\r\n");
    }

    display.clearDisplay();
    display.setCursor(1,0);
    display.printf("T.Deseada: %d\n",(int)rot_val_rec);
    display.printf("T.Actual: %f\n",temp_val_rec);
    display.display();
    //delay(250);
    vTaskDelay(250/portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void setup() {
    q1_rotation = xQueueCreate(5, sizeof(float));
    q2_temperature = xQueueCreate(5, sizeof(float));
    q3_light = xQueueCreate(5, sizeof(float));

    Serial.begin(9600);
    pinMode(A0,INPUT);
    pinMode(A1,INPUT);
    analogReadResolution(10);

    dht.begin();

    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
    pinMode(blueLed, OUTPUT);

    pinMode(onSwitch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(onSwitch), on_handleInterrupt, FALLING);
    pinMode(offSwitch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(offSwitch), off_handleInterrupt, FALLING);

    display.begin(SSD1306_SWITCHCAPVCC,0x3C);

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(1,0);

    /*Anadimos las tareas de medicion de temperatura y rotacion*/
    xTaskCreatePinnedToCore(vTaskRGB, "Task RGB", 4000, NULL, 1, NULL, 1);

    xTaskCreatePinnedToCore(vTask1, "Task 1", 4000, NULL, 3, &t1, 1);
    xTaskCreatePinnedToCore(vTask2, "Task 2", 4000, NULL, 3, &t2, 1);
    xTaskCreatePinnedToCore(vTask3, "Task 3", 4000, NULL, 3, &t3, 1);
    xTaskCreatePinnedToCore(vTaskLCD, "Task LCD", 4000, NULL, 3, &t4, 1);
    xTaskCreatePinnedToCore(vTaskLight, "Task Light", 4000, NULL, 3, &t5, 1);

    vTaskSuspend(t1);
    vTaskSuspend(t2);
    vTaskSuspend(t3);
    vTaskSuspend(t4);
    //vTaskSuspend(t5);

    display.clearDisplay();
    next_state = ST_NONE;

}


void loop() {

}
