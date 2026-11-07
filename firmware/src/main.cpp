#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>

// pinos
#define DHT_PIN 4
#define RS_PIN 26
#define RELAY_PIN 14
#define BUZZ_PIN 27

// pinos do led
#define GREEN_LED_PIN 25
#define RED_LED_PIN 32
#define BLUE_LED_PIN 33

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

float temperature = 15.0;
float humidity = 50.0;

// flags
bool door_open;
bool auth_access = false;

// momento da atualização
unsigned long update_time = 0;
// momento de destravamento
unsigned long unlock_time = 0;

// handle das tasks
TaskHandle_t wifi_connect_handle = NULL;
TaskHandle_t rfid_auth_handle = NULL;
TaskHandle_t door_warning_handle = NULL;
TaskHandle_t send_payload_handle = NULL;
TaskHandle_t temp_warning_handle = NULL;

// funcoes das tasks
void door_status();
void update_display();
void rfid_auth(void *parameter);
void access_granted();
void access_denied();
void send_payload(void *parameter);
void wifi_connect(void *parameter);
void door_warning(void *parameter);

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
  xTaskCreateUniversal(wifi_connect, "wifi_connect", 5000, NULL, 1, &wifi_connect_handle, 0);
  // task para montar e enviar payload ao dashboard
  // xTaskCreateUniversal(send_payload, "send_payload", 10000, NULL, 1, &send_payload_handle, 0);
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

    // remove a autorização de acesso caso esteja autorizado e faça mais de 3 segundos desde o destravamento da porta
    if (millis() - unlock_time > 3000 && auth_access)
    {
      auth_access = false;
    }
  }
}

void update_display()
{
  // posiciona o cursor
  lcd.setCursor(0, 0);
  // escreve a temperatura e umidade
  lcd.printf("T: %.1f  U: %.1f", dht.readTemperature(), dht.readHumidity());
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

      // limpa o aviso no display
      lcd.setCursor(0, 1);
      lcd.print("               ");

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
  auth_access = true;

  // liga o relé
  digitalWrite(RELAY_PIN, LOW);

  // feedback no display
  lcd.setCursor(0, 1);
  lcd.print(" Acesso liberado");

  // espera 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY_PIN, HIGH);
}

void access_denied()
{
  // feedback no display
  lcd.setCursor(0, 1);
  lcd.print(" Acesso negado  ");

  // espera 3 segundos
  vTaskDelay(3000 / portTICK_PERIOD_MS);
}

// TODO: checar se contribui pro delay
void send_payload(void *parameter)
{
  for (;;)
  {
    // lê os valores medidos pelo dht
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    // TODO: tratar caso de leitura inválida para enviar ao dashboard

    String payload;
    JsonDocument doc;

    // campos do payload
    doc["device_id"] = DEVICE_ID;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["door_open"] = door_open;
    doc["connection_status"] = CONNECTION_STATUS;
    // TODO: adicionar campos do payload em falta

    serializeJson(doc, payload);

    // NOTE: se contribuir para o delay, modificar para uma chamada com millis a cada 20 segundos
    //       no loop principal
    // envia a cada 20 segundos
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

void wifi_connect(void *parameter)
{
  for (;;)
  {
    // começa a comunicação wifi
    WiFi.begin(ssid, pswd);
    Serial.print("Conectando com a rede WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    Serial.println("\nConectado à rede Wifi.");
  }
}

void door_warning(void *parameter)
{
  for (;;)
  {
    if (door_open && (millis() - unlock_time > 30000 || !auth_access))
    {
      // apaga o led verde se estiver aceso
      digitalWrite(GREEN_LED_PIN, HIGH);

      // avisa no display
      lcd.setCursor(0, 0);
      lcd.print("  Favor fechar  ");
      lcd.setCursor(0, 1);
      lcd.print("    a porta!    ");

      // liga o led vermelho
      digitalWrite(RED_LED_PIN, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);

      // desliga o led vermelho
      digitalWrite(RED_LED_PIN, HIGH);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

// TODO: buzzer para acesso
// TODO: led para status da porta | ok!
// TODO: mudanca no display (apenas acesso e mensagem de alerta) | ok!
// TODO: integração com dashboard
// TODO: identificar e resolver problema de delay (provavel problema com o rtos,
//       gerenciar melhor os nucleos e os tempos de delay nas tasks)