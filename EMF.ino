#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =============== CONFIG WI-FI E MQTT ===============
const char* ssid = "AMF-CORP";
const char* password = "@MF$4515";

const char* mqtt_server = "5e872d89b8ad4f9f8b879af366b22c4a.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Avila";
const char* mqtt_password = "Adimin123";

// Tópico para publicação
const char* topic_publish = "meu-emf-meter/sala/atividade";

// =============== PINOS DOS SENSORES E LEDS ===============
const int TEMP_SENSOR_PIN = 35;
const int SOUND_SENSOR_PIN = 34;

const int ledPins[] = {18, 19, 21, 22, 23};
const int numLeds = 5;

// Intervalos de tempo
#define SENSOR_READ_INTERVAL_MS 250   // Lê sensores e atualiza LEDs 4x por segundo (rápido)
#define MQTT_PUBLISH_INTERVAL_MS 120000 // Publica no MQTT a cada 2 minutos (lento)

// =============== OBJETOS ===============
WiFiClientSecure net;
PubSubClient mqtt(net);

// =============== VARIÁVEIS DE LÓGICA ===============
unsigned long last_sensor_read_ms = 0;
unsigned long last_mqtt_publish_ms = 0;

float previousTemp = -100;
const float TEMP_DROP_THRESHOLD = 1.0;

// Variáveis para guardar os últimos dados
float last_temp = 0.0;
int last_sound = 0;
int last_activity = 0;

// Protótipos das funções
void updateLeds(int level);
static void connectWiFi();
static void setupTLS();
static bool mqttConnect();
static void publishData(float temp, int sound, int activity);

void setup() {
  Serial.begin(115200);
  delay(100);
  analogReadResolution(12);
  pinMode(TEMP_SENSOR_PIN, INPUT);
  pinMode(SOUND_SENSOR_PIN, INPUT);
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
  connectWiFi();
  setupTLS();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(512);
  previousTemp = (analogRead(TEMP_SENSOR_PIN) / 4095.0) * 330.0;
}

void loop() {
  if (!mqtt.connected()) {
    if (!mqttConnect()) {
      delay(5000);
      return;
    }
  }
  mqtt.loop();

  unsigned long now = millis();

  // BLOCO 1: LEITURA RÁPIDA DE SENSORES E ATUALIZAÇÃO DOS LEDS
  if (now - last_sensor_read_ms >= SENSOR_READ_INTERVAL_MS) {
    last_sensor_read_ms = now;

    float currentTemp = (analogRead(TEMP_SENSOR_PIN) / 4095.0) * 330.0;
    int soundLevel = analogRead(SOUND_SENSOR_PIN);
    int activityLevel = 0;
    
    activityLevel = map(soundLevel, 700, 4000, 0, 5);
    
    if (previousTemp > -50 && (previousTemp - currentTemp > TEMP_DROP_THRESHOLD)) {
      Serial.println("!!! QUEDA SÚBITA DE TEMPERATURA DETECTADA !!!");
      activityLevel += 2;
    }

    activityLevel = constrain(activityLevel, 0, 5);
    previousTemp = currentTemp;

    updateLeds(activityLevel);

    last_temp = currentTemp;
    last_sound = soundLevel;
    last_activity = activityLevel;
  }

  // BLOCO 2: PUBLICAÇÃO LENTA NO MQTT
  if (now - last_mqtt_publish_ms >= MQTT_PUBLISH_INTERVAL_MS) {
    last_mqtt_publish_ms = now;
    
    Serial.println("\n--- Enviando relatório para o MQTT ---");
    publishData(last_temp, last_sound, last_activity);
  }
}

// ================== DEFINIÇÃO DAS FUNÇÕES ==================
void updateLeds(int level) {
  for (int i = 0; i < numLeds; i++) {
    if (i < level) {
      digitalWrite(ledPins[i], HIGH);
    } else {
      digitalWrite(ledPins[i], LOW);
    }
  }
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando ao Wi-Fi '%s'...\n", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 Serial.printf("\nWi-Fi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
}

static void setupTLS() {
  net.setInsecure();
  net.setTimeout(15000);
}

static bool mqttConnect() {
  if (mqtt.connected()) return true;
  Serial.printf("Conectando ao MQTT TLS em %s:%d ... ", mqtt_server, mqtt_port);
  bool ok = mqtt.connect(
    (String("ESP32Client-") + String((uint32_t)ESP.getEfuseMac(), HEX)).c_str(),
    mqtt_user, mqtt_password
  );
  if (ok) {
    Serial.println("conectado!");
    return true;
  } else {
    Serial.printf("falhou, rc=%d\n", mqtt.state());
    return false;
  }
}

static void publishData(float temp, int sound, int activity) {
  StaticJsonDocument<200> doc;
  doc["temperatura"] = temp;
  doc["nivel_ruido"] = sound;
  doc["atividade_paranormal"] = activity;
  doc["rssi"] = WiFi.RSSI();
  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0) {
    Serial.println("⚠️ Erro ao montar JSON");
    return;
  }
  if (!mqtt.publish(topic_publish, payload)) {
    Serial.println("⚠️ Falha ao publicar");
  } else {
    Serial.print("PUB -> ");
    Serial.println(payload);
  }
}
