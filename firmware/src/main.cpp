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
// id do dispositivo
#define DEVICE_ID "dionisio-01"

// momento da atualização
unsigned long update_time = 0;
// momento de destravamento
unsigned long unlock_time = 0;

// flags
bool door_open;
bool lock_triggered;

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
void wifiConnect();
void update();
void unlock();

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

  // TODO: inicializar flags
}

void loop()
{
  // conexão perdida -> reconexão
  if (WiFi.status() == WL_CONNECTION_LOST)
  {
    Serial.println("Conexão com rede WiFi perdida. Tentando reconectar.");
    wifiConnect();
  }

  unsigned long time_now = millis();
  // atualiza dados para o dashboard a cada 20 segundos
  if (update_time == 0 || time_now - update_time >= 20000)
  {
    update_time = time_now;
    update();
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

  String payload;
  JsonDocument doc;

  // campos do payload
  doc["device_id"] = DEVICE_ID;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["door_open"] = door_open;
  doc["lock_triggered"] = lock_triggered;
  doc["timestamp"] = timestamp;
  // TODO: adicionar campos do payload em falta

  serializeJson(doc, payload);
}

void unlock()
{
  lock_triggered = true;
  // TODO: adicionar comportamento específico do relé para abrir a trava
}
