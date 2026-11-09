#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// pinos
#define DHT_PIN 4
#define RS_PIN 26
#define RELAY_PIN 14
#define BUZZ_PIN 27

// pinos do led
#define GREEN_LED_PIN 33
#define RED_LED_PIN 32
#define BLUE_LED_PIN 25

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

// frequencia de notas musicais
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_E5 659
#define NOTE_G5 784

// nome da rede wifi
String ssid = "whimsygarden";
// senha da rede wifi
String pswd = "wifi6@alares";

// tag do cartão da adega
uint8_t card_tag[] = {0x02, 0x36, 0x31, 0x30, 0x30, 0x35, 0x32, 0x30, 0x39, 0x36, 0x34, 0x35, 0x45, 0x03};

// client wifi
WiFiClient esp_client;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

DHT dht(DHT_PIN, DHT_TYPE);

// configura o serial para o rfid
HardwareSerial rfid(1);

float temperature;
float humidity;

// flags porta
bool door_open;
bool locked = true;
bool door_warn = false;

// flags acesso
bool access_grnt = false;
bool access_dnd = false;

// momento da atualização
unsigned long update_time = 0;
// momento de destravamento
unsigned long unlock_time = 0;

// handle das tasks
TaskHandle_t wifi_connect_handle = NULL;
TaskHandle_t rfid_auth_handle = NULL;
TaskHandle_t door_warning_handle = NULL;
TaskHandle_t send_payload_handle = NULL;
TaskHandle_t remote_handle = NULL;

// funcoes das tasks
void door_status();
void update_display();
void rfid_auth(void *parameter);
void access_granted();
void access_denied();
void send_payload(void *parameter);
void wifi_connect(void *parameter);
void door_warning(void *parameter);
void remote_access(void *parameter);

void setup()
{
  Serial.begin(115200);

  // configuração dos pinos
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RS_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  // desabilita os pinos do led
  digitalWrite(BLUE_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);

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

  // task para conectar ao wifi
  // começa a comunicação wifi
  WiFi.begin(ssid, pswd);
  Serial.print("Conectando com a rede WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  Serial.println("\nConectado à rede Wifi.");

  // task para montar e enviar payload ao dashboard
  xTaskCreateUniversal(send_payload, "send_payload", 10000, NULL, 1, &send_payload_handle, 1);
  // task para checar acesso remoto
  xTaskCreateUniversal(remote_access, "remote_access", 10000, NULL, 1, &remote_handle, 0);
  // task para autorizar rfid
  xTaskCreateUniversal(rfid_auth, "rfid_auth", 5000, NULL, 1, &rfid_auth_handle, 1);
  // task para alertar porta aberta
  xTaskCreateUniversal(door_warning, "door_warning", 5000, NULL, 2, &door_warning_handle, 0);
}

void loop()
{
  // estado da porta
  door_status();
  // display
  update_display();

  if (WiFi.status() == WL_DISCONNECTED)
  {
    WiFi.begin(ssid, pswd);
    Serial.print("Conectando com a rede WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    Serial.println("\nConectado à rede Wifi.");
  }
}

void door_status()
{
  // porta aberta -> indica com led vermelho
  if (digitalRead(RS_PIN) == HIGH)
  {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    door_open = true;
  }
  // porta fechada ->  indica com led verde
  else
  {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    // flags
    door_open = false;

    if (!access_grnt)
    {
      locked = true;
    }

    // se a porta foi fechada, desativa o alerta
    if (door_warn)
    {
      door_warn = false;
    }
  }
}

void update_display()
{
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  lcd.setCursor(0, 0);
  // escreve a temperatura e umidade
  lcd.printf("T: %.1f  U: %.1f", temperature, humidity);

  if (door_warn)
  {
    // avisa no display
    lcd.setCursor(0, 1);
    lcd.print(" Feche a porta! ");
  }
  else if (access_grnt)
  {
    // feedback no display
    lcd.setCursor(0, 1);
    lcd.print("Acesso liberado!");
  }
  else if (access_dnd)
  {
    // feedback no display
    lcd.setCursor(0, 1);
    lcd.print(" Acesso negado! ");
  }
  else
  {
    // limpa segunda linha do display
    lcd.setCursor(0, 1);
    lcd.print("                ");
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
        access_granted();
      }
      else
      {
        access_denied();
      }

      // descartar leituras adicionais
      while (rfid.available())
      {
        rfid.read();
      }
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

void access_granted()
{
  // salva o momento em que a porta foi destrancada
  unlock_time = millis();
  // indica acesso autorizado
  access_grnt = true;
  locked = false;

  // feedback sonoro
  tone(BUZZ_PIN, NOTE_C5, 200);
  tone(BUZZ_PIN, NOTE_E5, 200);
  tone(BUZZ_PIN, NOTE_G5, 200);

  // liga o relé
  digitalWrite(RELAY_PIN, LOW);

  // espera 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY_PIN, HIGH);

  access_grnt = false;
}

void access_denied()
{
  access_dnd = true;

  // feedback sonoro
  tone(BUZZ_PIN, NOTE_CS5, 200);
  tone(BUZZ_PIN, NOTE_C5, 200);

  // espera 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);

  access_dnd = false;
}

void send_payload(void *parameter)
{
  for (;;)
  {
    String payload;
    JsonDocument doc;
    JsonDocument arr;

    char temp[10];
    sprintf(temp, "%.1f", temperature);

    char hum[10];
    sprintf(hum, "%.1f", humidity);

    char door[10];
    sprintf(door, "%.1f", door_open);

    String keys[] = {"dispositivo", "temperatura", "umidade", "porta-aberta", "status-conexao"};
    String values[] = {DEVICE_ID, temp, hum, door, CONNECTION_STATUS};

    for (int i = 0; i < 5; i++)
    {
      arr["key"] = keys[i];
      arr["value"] = values[i];

      doc["feeds"].add(arr);
    }

    serializeJson(doc, payload);

    // criando um client http
    HTTPClient http;
    // conexão http
    http.begin("https://io.adafruit.com/api/v2/letholanda/groups/dionisio/data");
    // enviando via http
    http.addHeader("Content-Type", "application/json");
    int http_response_code = http.POST(payload);
    http.end();

    Serial.print("Response code: ");
    Serial.println(http_response_code);

    // envia a cada 20 segundos
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

void remote_access(void *parameter)
{
  for (;;)
  {
    // criando um client http
    HTTPClient http;

    // conexão http
    http.begin("https://io.adafruit.com/api/v2/letholanda/feeds/liberar-porta/data/retain");
    // enviando via http
    http.addHeader("Content-Type", "application/json");
    int http_response_code = http.GET();
    String val = http.getString();
    val.trim();
    if (val == "1,,,")
    {
      access_granted();
    }

    Serial.println(val);

    http.end();

    Serial.print("Response code: ");
    Serial.println(http_response_code);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void door_warning(void *parameter)
{
  for (;;)
  {
    if (door_open && (millis() - unlock_time > 30000 || locked))
    {
      door_warn = true;

      // apaga o led verde se estiver aceso
      digitalWrite(GREEN_LED_PIN, HIGH);

      // liga o led vermelho e toca o buzzer
      digitalWrite(RED_LED_PIN, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);

      // desliga o led vermelho
      digitalWrite(RED_LED_PIN, HIGH);
      tone(BUZZ_PIN, NOTE_G5, 500);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}
