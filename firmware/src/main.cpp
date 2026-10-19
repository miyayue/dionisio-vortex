#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ezTime.h>

// pinos
#define DHT_PIN 27
#define BUZZ_PIN 32
#define LED_PIN 33
#define RS_PIN 14
#define RELAY_PIN 26

// pinos do rfid
#define SDA_PIN 5
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define RST_PIN 22

// TODO: atualizar para os pinos definitivos

// tipo de dht
#define DHT_TYPE DHT22
// id do dispositivo
#define DEVICE_ID "dionisio-01"
#define CONNECTION_STATUS "conectado"

// momento da atualização
unsigned long update_time = 0;
// momento de destravamento
unsigned long unlock_time = 0;

// flags
bool door_open;

// nome da rede wifi
String ssid = "";
// senha da rede wifi
String pswd = "";

// timestamp e timezone
String timestamp;
Timezone brasil;

// client wifi
WiFiClient esp_client;

DHT dht(DHT_PIN, DHT_TYPE);

// funções
void wifiConnect();
void update();
void rfid_access();
void unlock();

void debug();

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

  // configuração dos pinos
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RS_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  // TODO: configurar pinos do rfid

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
    // update();
    debug();
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

void rfid_access()
{
  // temporario
  bool access_granted = true;
  // TODO: verificação do acesso

  if (access_granted)
  {
    unlock();
  }
  else
  {
    tone(BUZZ_PIN, 3000, 1500);
  }
  noTone(BUZZ_PIN);
  // TODO: colocar valores definitivos para o buzzer
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
  doc["timestamp"] = timestamp;
  doc["connection_status"] = CONNECTION_STATUS;
  // TODO: adicionar campos do payload em falta

  serializeJson(doc, payload);
}

void unlock()
{
  // TODO: adicionar comportamento específico do relé para abrir a trava
}

void debug()
{
  Serial.println("Lendo valores:");

  Serial.println("temp: ");
  Serial.println(dht.readTemperature());

  Serial.println("umid: ");
  Serial.println(dht.readHumidity());

  digitalWrite(LED_PIN, HIGH);
}