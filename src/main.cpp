#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// --- Broches ---
#define DHTPIN 4
#define LED_PIN 26
#define SERVO_PIN 2
#define LDR_PIN 34
#define TRIG_PIN 5
#define ECHO_PIN 18

// --- Capteur DHT ---
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Servo servo;

// --- Réseau ---
const char* ssid = ".";
const char* password = "1-1-1-1-";
// const char* ssid = "Wokwi-GUEST";
// const char* password = "";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --- Topics ---
const char* topic_dht = "tp/dht11";
const char* topic_ldr = "tp/ldr";
const char* topic_distance = "tp/distance";
const char* topic_led = "tp/led";
const char* topic_servo = "tp/servo";

// --- Variables ---
unsigned long previousMillis = 0;
const long interval = 2000;

// --- WiFi ---
void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connecté");
  Serial.println("Adresse IP : " + WiFi.localIP().toString());
}

// --- Callback MQTT ---
void callback(char* topic, byte* payload, unsigned int length) {
  String data = "";
  for (int i = 0; i < length; i++) data += (char)payload[i];
  data.trim();
  data.toLowerCase();
  Serial.print("Message reçu sur le topic : ");
  Serial.println(topic);
  Serial.println("Contenu : " + data);

  // --- LED ---
  if (String(topic) == topic_led) {
    if (data == "on" || data == "true" || data == "1") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED → ON");
    } else {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED → OFF");
    }
  }

  // --- Servo ---
  if (String(topic) == topic_servo) {
    int degree = constrain(data.toInt(), 0, 180);
    servo.write(degree);
    Serial.print("Servo déplacé à : ");
    Serial.println(degree);
  }
}

// --- Reconnexion MQTT non bloquante ---
void reconnect() {
  if (!client.connected()) {
    Serial.print("Connexion MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connecté");
      client.subscribe(topic_led);
      client.subscribe(topic_servo);
      Serial.println("Topics abonnés");
    } else {
      Serial.print("Échec, code erreur = ");
      Serial.println(client.state());
      Serial.println("Nouvelle tentative dans 5 secondes");
    }
  }
}

// --- HC-SR04 ---
long readDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

// --- Test DHT ---
void testDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    Serial.printf("✅ Test DHT OK → %.2f°C, %.2f%%\n", t, h);
  } else {
    Serial.println("⚠️ Erreur lecture DHT11 ! Vérifiez le câblage et la résistance de pull-up.");
  }
}

// --- Setup ---
void setup() {
  Serial.begin(9600);

  // --- PinMode ---
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // --- Servo ---
  servo.attach(SERVO_PIN);
  servo.write(0);

  // --- DHT ---
  dht.begin();
  delay(2000); // ✅ Attente stabilisation
  testDHT();   // ✅ Test immédiat du capteur

  // --- WiFi et MQTT ---
  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

// --- Loop ---
void loop() {
  // --- MQTT ---
  client.loop();
  reconnect();  // Non bloquant

  // --- Lecture capteurs toutes les 2 secondes ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // --- DHT ---
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      String dhtMsg = String(temp) + "," + String(hum);
      client.publish(topic_dht, dhtMsg.c_str());
      Serial.printf("DHT → %.2f°C, %.2f%%\n", temp, hum);
    } else {
      Serial.println("⚠️ Erreur lecture DHT22 !");
    }

    // --- LDR ---
    int ldrValue = analogRead(LDR_PIN);
    client.publish(topic_ldr, String(ldrValue).c_str());
    Serial.println("LDR → " + String(ldrValue));

    // --- Ultrason ---
    long distance = readDistance();
    client.publish(topic_distance, String(distance).c_str());
    Serial.println("Distance → " + String(distance));
  }
}
