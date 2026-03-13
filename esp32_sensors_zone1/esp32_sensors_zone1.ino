#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

/* ---------- Wi‑Fi ---------- */
const char* ssid     = "Wifi-Campus1";
const char* password = "GSBP-WIFI";

/* ---------- HiveMQ Cloud ---------- */
const char* mqtt_broker = "a94d3f7ed3bf4467ab3b338393b611c4.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "Admin";
const char* mqtt_pass   = "Admin123";
const char* topic       = "GreenSmartEnergyPark/environment";

/* ---------- GPIO / Capteurs ---------- */
#define DHTPIN     4
#define DHTTYPE    DHT22
#define MQ135_PIN  34
#define SOUND_PIN  35

BH1750 lightMeter;
DHT     dht(DHTPIN, DHTTYPE);

WiFiClientSecure espClient;
PubSubClient     client(espClient);

/* ---------- MQTT reconnexion ---------- */
void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) break;
    delay(5000);
  }
}

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(115200);

  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);

  Wire.begin(21, 22);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  espClient.setInsecure();              // TLS sans certificat
  client.setServer(mqtt_broker, mqtt_port);
  client.setBufferSize(256);

  reconnect();
}

/* ---------- LOOP ---------- */
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  /* --- Lecture capteurs --- */
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  int   co2         = analogRead(MQ135_PIN);
  int   sound       = analogRead(SOUND_PIN);
  float lux         = lightMeter.readLightLevel();

  if (isnan(temperature) || isnan(humidity)) {
    delay(2000);
    return;
  }

  /* === FORMAT LINE PROTOCOL POUR INFLUXDB ===
   *  - Mesure   : Living_Room
   *  - Tag      : sensor=sensor_1
   *  - Champs   : temperature, humidity, co2, sound, lux
   *  --> Telegraf (inputs.mqtt_consumer data_format="influx")
   *      enverra ces points tels quels dans le bucket 'Data'
   */
  String payload = "Living_Room,sensor=sensor_1 ";
  payload += "temperature="  + String(temperature, 2) + ",";
  payload += "humidity="     + String(humidity,    2) + ",";
  payload += "co2="          + String(co2)            + ",";
  payload += "sound="        + String(sound)          + ",";
  payload += "lux="          + String(lux,         2);

  /* --- Publication MQTT --- */
  client.publish(topic, payload.c_str());   // true = QoS 0, retain false par défaut
  delay(60000);                             // 60 s
}