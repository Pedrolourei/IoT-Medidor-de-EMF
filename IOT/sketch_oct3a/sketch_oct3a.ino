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

// =============== TÓPICOS MQTT ===============
const char* TOPIC_PUB_TEMP = "sdap/sensor/temperatura";
const char* TOPIC_PUB_SOUND = "sdap/sensor/som";
const char* TOPIC_PUB_STATUS = "sdap/dispositivo/status";
const char* TOPIC_SUB_ALARM = "sdap/atuador/alarme/comando"; // Este tópico agora controla o LED vermelho

// =============== PINOS DOS SENSORES E LEDS ===============
const int TEMP_SENSOR_PIN = 35;
const int SOUND_SENSOR_PIN = 34;

const int ledPins[] = {18, 19, 21, 22, 23}; // O último pino (índice 4) é o LED vermelho
const int numLeds = 5;

#define READ_INTERVAL_MS 1000

// =============== OBJETOS E VARIÁVEIS GLOBAIS ===============
WiFiClientSecure net;
PubSubClient mqtt(net);

unsigned long last_read_ms = 0;
float previousTemp = -100;
const float TEMP_DROP_THRESHOLD = 1.0;
bool manualAlarmOn = false; // Flag para controlar o LED vermelho manualmente

// Protótipos das Funções
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateLeds(int level);
void publishTemperature(float temp);
void publishSound(int soundLevel);
void publishStatus(int activityLevel);
static void connectWiFi();
static void setupTLS();
static bool mqttConnect();


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
  mqtt.setCallback(mqttCallback);

  previousTemp = (analogRead(TEMP_SENSOR_PIN) / 4095.0) * 330.0;
}


void loop() {
  if (!mqtt.connected()) {
    if (!mqttConnect()) {
      delay(2000);
      return;
    }
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - last_read_ms >= READ_INTERVAL_MS) {
    last_read_ms = now;

    float currentTemp = (analogRead(TEMP_SENSOR_PIN) / 4095.0) * 330.0;
    int soundLevel = analogRead(SOUND_SENSOR_PIN);

    publishTemperature(currentTemp);
    publishSound(soundLevel);

    int activityLevel = map(soundLevel, 700, 4000, 0, 5);
    if (previousTemp > -50 && (previousTemp - currentTemp > TEMP_DROP_THRESHOLD)) {
      activityLevel += 2;
    }
    activityLevel = constrain(activityLevel, 0, 5);
    previousTemp = currentTemp;

    updateLeds(activityLevel);
    
    publishStatus(activityLevel);
  }
}

// ================== DEFINIÇÃO DAS FUNÇÕES ==================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == TOPIC_SUB_ALARM) {
    if (message == "LIGAR") {
      Serial.println("Comando para LIGAR o LED de alerta recebido!");
      manualAlarmOn = true;
    } else if (message == "DESLIGAR") {
      Serial.println("Comando para DESLIGAR o LED de alerta recebido!");
      manualAlarmOn = false;
    }
  }
}

void updateLeds(int level) {
  // Atualiza os 4 primeiros LEDs (verdes e amarelos) normalmente
  for (int i = 0; i < 4; i++) {
    if (i < level) {
      digitalWrite(ledPins[i], HIGH);
    } else {
      digitalWrite(ledPins[i], LOW);
    }
  }

  // Lógica separada para o LED vermelho (atuador)
  if (level == 5 || manualAlarmOn) {
    digitalWrite(ledPins[4], HIGH); // Liga o LED vermelho se a atividade for máxima OU se o comando manual estiver ativo
  } else {
    digitalWrite(ledPins[4], LOW);
  }
}

void publishTemperature(float temp) {
  StaticJsonDocument<100> doc;
  doc["unidade"] = "C";
  doc["valor"] = temp;
  char payload[128];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_PUB_TEMP, payload);
}

void publishSound(int soundLevel) {
  StaticJsonDocument<100> doc;
  doc["valor_raw"] = soundLevel;
  char payload[128];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_PUB_SOUND, payload);
}

void publishStatus(int activityLevel) {
  StaticJsonDocument<100> doc;
  doc["atividade_paranormal"] = activityLevel;
  if (activityLevel == 5 || manualAlarmOn) {
    doc["estado"] = "ALERTA";
  } else {
    doc["estado"] = "Normal";
  }
  char payload[128];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_PUB_STATUS, payload);
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
    mqtt.subscribe(TOPIC_SUB_ALARM);
    Serial.printf("Assinando o tópico: %s\n", TOPIC_SUB_ALARM);
  } else {
    Serial.printf("falhou, rc=%d\n", mqtt.state());
  }
  return ok;
}