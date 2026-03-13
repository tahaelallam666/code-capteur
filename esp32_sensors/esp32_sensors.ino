#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

// WiFi credentials
const char* ssid = "Wifi-Campus1";
const char* password = "GSBP-WIFI";

// InfluxDB Cloud configuration
const char* influxHost = "https://us-east-1-1.aws.cloud2.influxdata.com";
const char* org = "9702ba02b9224902";
const char* bucket = "Data";
const char* token = "7iUgzrI9F7IRHfS2uzdeR9nS6gRuFVChXdc3dM6QdFsHmnmSG-SU2wqFn0pt5blL4R44U1tq_7FrOXzZ68m07Q==";

// Sensor pins
#define DHTPIN 4
#define DHTTYPE DHT22
#define MQ135_PIN 34   // CO2 analog
#define SOUND_PIN 35   // Sound sensor analog

// Sensor setup
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
WiFiClientSecure secureClient;

void setup() {
  Serial.begin(115200);

  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);

  Wire.begin(21,22); // SDA, SCL for BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
    Serial.println("BH1750 initialisé correctement !");
  } else {
    Serial.println("Erreur d'initialisation du BH1750 !");
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Accept insecure cert (for testing)
  secureClient.setInsecure();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Read sensor values
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int co2 = analogRead(MQ135_PIN);     // MQ135 CO2 raw
    int soundValue = analogRead(SOUND_PIN); // KY-038 Sound raw
    float lux = lightMeter.readLightLevel(); // BH1750

    // Validate DHT
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Erreur DHT22 !");
      delay(5000);
      return;
    }

    // Print to Serial
    Serial.println("---------------");
    Serial.print("Température : "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("Humidité : "); Serial.print(humidity); Serial.println(" %");
    Serial.print("MQ135 (CO2 - brut) : "); Serial.println(co2);
    Serial.print("Son (KY-038 - brut) : "); Serial.println(soundValue);
    Serial.print("Lumière (BH1750) : "); Serial.print(lux); Serial.println(" lx");
    Serial.println("---------------\n");

    // Format Line Protocol for InfluxDB
    String lineProtocol =
      "Bed_Room,sensor=sensor_1 " +
      String("temperature=") + String(temperature, 2) + "," +
      "humidity=" + String(humidity, 2) + "," +
      "co2=" + String(co2) + "," +
      "sound=" + String(soundValue) + "," +
      "lux=" + String(lux, 2);

    String url = String(influxHost) + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";

    // Send to InfluxDB
    HTTPClient http;
    http.begin(secureClient, url);
    http.addHeader("Authorization", "Token " + String(token));
    http.addHeader("Content-Type", "text/plain");

    int httpResponseCode = http.POST(lineProtocol);

    Serial.print("InfluxDB HTTP Response: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode != 204) {
      Serial.println("Erreur InfluxDB:");
      Serial.println(http.getString());
    }

    http.end();
  } else {
    Serial.println("WiFi non connecté !");
  }

  delay(60000);
}