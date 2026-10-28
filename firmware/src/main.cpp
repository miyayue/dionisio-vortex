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

// flags
bool door_open;
bool door_warn = false;
bool auth_access = false;
bool dsp_access_prio = false;

// momento da atualização
unsigned long update_time = 0;
// momento de destravamento
unsigned long unlock_time = 0;

// handle das tasks
TaskHandle_t rfid_auth_handle = NULL;
TaskHandle_t access_granted_handle = NULL;
TaskHandle_t access_denied_handle = NULL;
TaskHandle_t door_warning_handle = NULL;
TaskHandle_t wifi_connect_handle = NULL;
TaskHandle_t send_payload_handle = NULL;
TaskHandle_t temp_warning_handle = NULL;

// funcoes das tasks
void door_status();
void update_display();
void warnings();
void rfid_auth(void *parameter);
void access_granted();
void access_denied();
void send_payload(void *parameter);
void wifi_connect();
void door_warning(void *parameter);

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
  pinMode(BUZZ_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

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

  wifi_connect();

  // task para montar e enviar payload ao dashboard
  xTaskCreateUniversal(send_payload, "send_payload", 10000, NULL, 1, &send_payload_handle, 0);
  // task para autorizar rfid
  xTaskCreateUniversal(rfid_auth, "rfid_auth", 10000, NULL, 1, &rfid_auth_handle, 1);
}

void loop()
{
  // estado da porta
  door_status();
  // display
  update_display();
  // checar alertas
  warnings();

  // conecta o wifi caso ainda nao esteja conectado ou caso perca a conexao
  if (WiFi.status() != WL_CONNECTED)
  {
    wifi_connect();
  }

  if (door_warn)
  {
    xTaskCreateUniversal(door_warning, "door_warning", 10000, NULL, 2, &door_warning_handle, 0);
  }
}

void door_status()
{
  // porta aberta
  if (digitalRead(RS_PIN) == HIGH)
  {
    digitalWrite(DOOR_RED_LED_PIN, HIGH);
    digitalWrite(DOOR_GREEN_LED_PIN, LOW);
    door_open = true;
  }
  // porta fechada
  else
  {
    digitalWrite(DOOR_GREEN_LED_PIN, HIGH);
    digitalWrite(DOOR_RED_LED_PIN, LOW);

    // flags
    door_open = false;
    door_warn = false;

    if (millis() - unlock_time > 3000 && auth_access)
    {
      auth_access = false;
    }
  }
}

void update_display()
{
  if (!door_warn)
  { // posiciona o cursor
    lcd.setCursor(0, 0);
    // escreve a temperatura e umidade
    lcd.printf("T: %.1f  U: %.1f", dht.readTemperature(), dht.readHumidity());

    // estado da porta
    if (!dsp_access_prio)
    {
      if (door_open)
      {
        lcd.setCursor(0, 1);
        lcd.print("  Porta aberta  ");
      }
      else
      {
        lcd.setCursor(0, 1);
        lcd.print("  Porta fechada ");
      }
    }
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("  Favor fechar  ");
    lcd.setCursor(0, 1);
    lcd.print("    a porta!    ");
  }
}

void rfid_auth(void *parameter)
{
  for (;;)
  {
    if (rfid.available() > 0 && !door_open)
    {
      // armazena os bytes da tag
      uint8_t tag[14];
      int byte_index = 0;

      vTaskDelay(100 / portTICK_PERIOD_MS);
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
      vTaskDelay(500 / portTICK_PERIOD_MS);

      // compara a tag lida com a tag autorizada para permitir ou negar o acesso
      if (memcmp(tag, card_tag, sizeof(tag)) == 0)
      {
        Serial.println("acesso liberado");
        access_granted();
      }
      else
      {
        Serial.println("acesso negado");
        access_denied();
      }

      // descartar leituras adicionais
      while (rfid.available())
      {
        rfid.read();
      }
    }
  }
}

void access_granted()
{
  // salva o momento em que a porta foi destrancada
  unlock_time = millis();
  // indica acesso autorizado
  auth_access = true;
  // adquire prioridade no display
  dsp_access_prio = true;

  // liga o relé e o led
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(ACCESS_GREEN_LED_PIN, HIGH);

  // feedback no display
  lcd.setCursor(0, 1);
  lcd.print(" Acesso liberado");

  // desliga o relé e o led apos 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(ACCESS_GREEN_LED_PIN, LOW);

  // remove prioridade no display
  dsp_access_prio = false;
}

void access_denied()
{
  // adquire prioridade no display
  dsp_access_prio = true;

  // liga o led
  digitalWrite(ACCESS_RED_LED_PIN, HIGH);

  // feedback no display
  lcd.setCursor(0, 1);
  lcd.print(" Acesso negado  ");

  // desliga o led após 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  digitalWrite(ACCESS_RED_LED_PIN, LOW);

  // remove prioridade no display
  dsp_access_prio = false;
}

void send_payload(void *parameter)
{
  for (;;)
  {
    // lê os valores medidos pelo dht
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    String timestamp = DateTime.toString();
    //  TODO: tratar caso de leitura inválida para enviar ao dashboard

    String payload;
    JsonDocument doc;

    // campos do payload
    doc["device_id"] = DEVICE_ID;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["door_open"] = door_open;
    doc["timestamp"] = DateTime.toString();
    doc["connection_status"] = CONNECTION_STATUS;
    // TODO: adicionar campos do payload em falta

    Serial.print("hora de envio: ");
    Serial.println(DateTime.toString());

    serializeJson(doc, payload);

    // envia a cada 20 segundos
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

void wifi_connect()
{
  // começa a comunicação wifi
  WiFi.begin(ssid, pswd);

  unsigned long start_time = millis();

  Serial.println("Conectando com a rede WiFi");
  while ((millis() - start_time) < 100)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nConectado à rede Wifi.");
      break;
    }
  }

  // timezone GMT-3
  DateTime.setTimeZone("<-03>3");
  // inicia o tempo e sincroniza com a timezone definida
  DateTime.begin();
}

void warnings()
{
  // porta aberta
  if (door_open && (millis() - unlock_time > 30000 || !auth_access))
  {
    Serial.println("alerta: porta aberta");
    door_warn = true;
  }
}

void door_warning(void *parameter)
{
  while (door_warn)
  {
    // ativa o led e o buzzer
    digitalWrite(DOOR_RED_LED_PIN, HIGH);
    digitalWrite(BUZZ_PIN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // desativa o led e o buzzer
    digitalWrite(DOOR_RED_LED_PIN, LOW);
    digitalWrite(BUZZ_PIN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  vTaskDelete(NULL);
}