#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ezTime.h>

// pino provisório
#define DHTPIN 2
// tipo de dht
#define DHTTYPE DHT22

// nome da rede wifi
String ssid = "";
// senha da rede wifi
String pswd = "";

// timestamp e timezone
String timestamp;
Timezone brasil;

// client wifi
WiFiClient esp_client;

DHT dht(DHTPIN, DHTTYPE);

// funções
void wifiConnect() {};
void update() {};

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(120000);

  Serial.println("Insira o nome da rede Wifi: ");
  ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Insira a senha da rede Wifi: ");
  pswd = Serial.readStringUntil('\n');
  pswd.trim();

  wifiConnect();

  // sincroniza o tempo
  waitForSync();
  // define a timezone
  brasil.setLocation("America/Fortaleza");

  // inicializa o sensor dht
  dht.begin();
}

void loop()
{
  // conexão perdida -> reconexão
  if (WiFi.status() == WL_CONNECTION_LOST)
  {
    Serial.println("Conexão com rede WiFi perdida. Tentando reconectar.");
    wifiConnect();
  }
}

void wifiConnect()
{
  // começa a comunicação wifi
  WiFi.begin(ssid, pswd);

  unsigned long start_time = millis();

  Serial.print("Conectando com a rede WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    if ((millis() - start_time) > 10000)
    {
      Serial.println("A conexão falhou. Verifique as credenciais de WiFi e reinicie o programa.");
      return;
    }
  }
  Serial.println("\nConectado à rede Wifi.");
}

void update()
{
  // lê os valores medidos pelo dht
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  timestamp = brasil.dateTime(ISO8601);
  // TODO: tratar caso de leitura inválida para enviar ao dashboard
}
