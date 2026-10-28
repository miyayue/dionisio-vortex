#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESPDateTime.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>

// pinos
#define DHT_PIN 27
#define RS_PIN 14
#define RELAY_PIN 26
#define BUZZ_PIN 18

// pinos dos leds
#define ACCESS_GREEN_LED_PIN 32
#define ACCESS_RED_LED_PIN 13
#define DOOR_GREEN_LED_PIN 23
#define DOOR_RED_LED_PIN 33

// pinos do rfid (rx não é utilizado)
#define RX_PIN 17
#define TX_PIN 16

// dados do display
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define LCD_ADDRESS 0x27

// tipo de dht
#define DHT_TYPE DHT22
// id do dispositivo
#define DEVICE_ID "dionisio-01"
#define CONNECTION_STATUS "conectado"

// nome da rede wifi
String ssid = "";
// senha da rede wifi
String pswd = "";

// tag do cartão da adega
uint8_t card_tag[] = {0x02, 0x36, 0x31, 0x30, 0x30, 0x35, 0x32, 0x30, 0x39, 0x36, 0x34, 0x35, 0x45, 0x03};

// client wifi
WiFiClient esp_client;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

DHT dht(DHT_PIN, DHT_TYPE);

// configura o serial para o rfid
HardwareSerial rfid(1);

float temperature = 15.0;
float humidity = 50.0;

void setup()
{
  Serial.begin(115200);

  // configuração dos pinos
  pinMode(ACCESS_GREEN_LED_PIN, OUTPUT);
  pinMode(ACCESS_RED_LED_PIN, OUTPUT);
  pinMode(DOOR_GREEN_LED_PIN, OUTPUT);
  pinMode(DOOR_RED_LED_PIN, OUTPUT);
  pinMode(RS_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
}

void loop()
{
}
