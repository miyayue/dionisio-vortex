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
#define BUZZ_PIN 32
#define LED_PIN 33
#define RS_PIN 14
#define RELAY_PIN 26

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

// tag do cartão da adega
uint8_t card_tag[] = {0x02, 0x36, 0x31, 0x30, 0x30, 0x35, 0x32, 0x30, 0x39, 0x36, 0x34, 0x35, 0x45, 0x03};

// client wifi
WiFiClient esp_client;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

DHT dht(DHT_PIN, DHT_TYPE);

// configura o serial para o rfid
HardwareSerial rfid(1);

// funções
void wifiConnect();
void setupDateTime();
void update_display();
void relay_safety();
void door_status();
void rfid_auth();
void unlock();
void access_denied();
void warning();
void update();

void testing();

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(120000);

  // Serial.println("Insira o nome da rede Wifi: ");
  // ssid = Serial.readStringUntil('\n');
  // ssid.trim();

  // Serial.println("Insira a senha da rede Wifi: ");
  // pswd = Serial.readStringUntil('\n');
  // pswd.trim();

  // configuração dos pinos
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RS_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  wifiConnect();

  setupDateTime();

  // inicializa e liga a luz do display
  lcd.init();
  lcd.backlight();
  lcd.clear();
  // inicializa o sensor dht
  dht.begin();
  // inicializa o rfid
  rfid.begin(9600, SERIAL_8N1, TX_PIN, RX_PIN);

  // inicializa o estado da porta
  door_status();
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

  // segurança do relé
  relay_safety();
  // checa o status da porta
  door_status();
  // emite alertas se necessário
  warning();

  // checa se o rfid detectou um cartao com a porta fechada
  if (rfid.available() > 0 && !door_open)
  {
    rfid_auth();
  }

  unsigned long time_now = millis();
  // atualiza dados a cada 20 segundos
  if (update_time == 0 || time_now - update_time >= 20000)
  {
    update_time = time_now;
    // atualiza o display
    update_display();
    // update();
    testing();
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

    if ((millis() - start_time) > 100000)
    {
      Serial.println("A conexão falhou. Verifique as credenciais de WiFi e reinicie o programa.");
      return;
    }
  }
  Serial.println("\nConectado à rede Wifi.");
}

void setupDateTime()
{
  // timezone GMT-3
  DateTime.setTimeZone("<-03>3");
  DateTime.begin();
}

void update_display()
{
  // posiciona o cursor para escrever a temperatura
  lcd.setCursor(4, 0);
  lcd.printf("T: %.2f", dht.readTemperature());
  // posiciona o cursor para escrever a umidade
  lcd.setCursor(4, 1);
  lcd.printf("U: %.2f", dht.readHumidity());
}

void relay_safety()
{
  // desliga o gpio do relé apos 1,5 segundo ou mais (idealmente logo apos ultrapassar
  // 1,5 s e antes de atingir 3 s)
  if (millis() - unlock_time > 1500 && digitalRead(RELAY_PIN) == LOW)
  {
    digitalWrite(RELAY_PIN, HIGH);
  }
}

void door_status()
{
  if (digitalRead(RS_PIN) == HIGH)
  {
    door_open = true;
  }
  else
  {
    door_open = false;
  }
}

void rfid_auth()
{
  // armazena os bytes da tag
  uint8_t tag[14];
  int byte_index = 0;

  delay(100);
  while (rfid.available() && byte_index < 14)
  {
    uint8_t byte = rfid.read();

    // inicia a tag no byte 2 (head da tag)
    if (byte == 2)
    {
      byte_index = 0;
    }

    tag[byte_index] = byte;
    byte_index++;
  }
  delay(500);

  // compara a tag lida com a tag autorizada para permitir ou negar o acesso
  if (memcmp(tag, card_tag, sizeof(tag)) == 0)
  {
    Serial.println("acesso liberado");
    unlock();
    return;
  }
  Serial.println("acesso negado");
  // access_denied();
  return;
}

void unlock()
{
  unlock_time = millis();
  digitalWrite(RELAY_PIN, LOW);
}

void warning()
{
  if (door_open && millis() - unlock_time > 60000)
  {
    Serial.println("alerta: porta aberta +1min");
    // TODO: comportamento do buzzer para avisar da porta aberta
  }
}

void update()
{
  // lê os valores medidos pelo dht
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  String timestamp = DateTime.toString();
  //  TODO: tratar caso de leitura inválida para enviar ao dashboard?

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

void testing()
{
  Serial.println("Lendo valores:");

  Serial.print("temp: ");
  Serial.println(dht.readTemperature());

  Serial.print("umid: ");
  Serial.println(dht.readHumidity());

  Serial.print("timestamp: ");
  Serial.println(DateTime.toString());

  Serial.print("porta aberta: ");
  Serial.println(door_open);

  digitalWrite(LED_PIN, HIGH);
}