#include <DHT.h>            // Bibliothèque pour lire le capteur DHT (température/humidité)
#include <WiFi.h>           // Bibliothèque pour la connexion WiFi de l’ESP32
#include <PubSubClient.h>   // Bibliothèque pour le protocole MQTT
#include <ESP32Servo.h>     // Bibliothèque pour contrôler un servo-moteur avec l’ESP32

// --- Broches ---
#define DHTPIN 4            // Broche utilisée pour connecter le DHT11
#define LED_PIN 26          // Broche de la LED
#define SERVO_PIN 2         // Broche du servo-moteur
#define LDR_PIN 34          // Broche analogique pour la photorésistance (LDR)
#define TRIG_PIN 5          // Broche TRIG du capteur ultrason (HC-SR04)
#define ECHO_PIN 18         // Broche ECHO du capteur ultrason (HC-SR04)

// --- Capteur DHT ---
#define DHTTYPE DHT11       // Type du capteur DHT utilisé (ici DHT11)
DHT dht(DHTPIN, DHTTYPE);   // Création de l’objet DHT avec la broche et le type
Servo servo;                // Création d’un objet Servo pour piloter le moteur

// --- Réseau ---
const char* ssid = ".";             // Nom du réseau WiFi
const char* password = "1-1-1-";    // Mot de passe WiFi
// ----- Réseau de test Wokwi -----
// const char* ssid = "Wokwi-GUEST";
// const char* password = "";
const char* mqttServer = "broker.hivemq.com"; // Adresse du broker MQTT
const int mqttPort = 1883;          // Port utilisé pour MQTT (1883 = standard non sécurisé)

WiFiClient espClient;       // Objet client WiFi
PubSubClient client(espClient); // Objet MQTT basé sur le client WiFi

// --- Topics MQTT ---
const char* topic_dht = "tp/dht11";       // Topic pour envoyer température/humidité
const char* topic_ldr = "tp/ldr";         // Topic pour envoyer luminosité
const char* topic_distance = "tp/distance"; // Topic pour envoyer la distance ultrason
const char* topic_led = "tp/led";         // Topic pour recevoir commandes LED
const char* topic_servo = "tp/servo";     // Topic pour recevoir commandes Servo

// --- Variables de gestion du temps ---
unsigned long previousMillis = 0;   // Stocke le dernier instant où les capteurs ont été lus
const long interval = 2000;         // Intervalle de lecture = 2 secondes

// --- Connexion WiFi ---
void setup_wifi() {
  delay(10);                        // Petite pause avant d’essayer la connexion
  WiFi.begin(ssid, password);       // Connexion au réseau WiFi
  Serial.print("Connexion WiFi");   // Message d’attente
  while (WiFi.status() != WL_CONNECTED) { // Tant que non connecté
    delay(500);                     // Attendre 500 ms
    Serial.print(".");              // Afficher un point (indicateur)
  }
  Serial.println("");               // Retour à la ligne
  Serial.println(" WiFi connecté");       // Confirmation
  Serial.println("Adresse IP : " + WiFi.localIP().toString()); // Afficher IP obtenue
}

// --- Fonction de rappel MQTT quand un message est reçu ---
void callback(char* topic, byte* payload, unsigned int length) {
  String data = "";                 // Variable pour stocker le message reçu
  for (int i = 0; i < length; i++) data += (char)payload[i]; // Conversion payload en texte
  data.trim();                      // Supprimer espaces inutiles
  data.toLowerCase();               // Passer en minuscules pour comparaison

  Serial.print(" Message reçu sur ");
  Serial.println(topic);            // Affiche le topic concerné
  Serial.println("Contenu : " + data); // Affiche le message reçu

  // --- Contrôle LED ---
  if (String(topic) == topic_led) { // Si le message vient du topic "tp/led"
    if (data == "on" || data == "true" || data == "1") {
      digitalWrite(LED_PIN, HIGH);  // Allumer la LED
      Serial.println(" LED → ON");
    } else {
      digitalWrite(LED_PIN, LOW);   // Éteindre la LED
      Serial.println(" LED → OFF");
    }
  }

  // --- Contrôle Servo ---
  if (String(topic) == topic_servo) { // Si message sur "tp/servo"
    int degree = constrain(data.toInt(), 0, 180); // Convertir en nombre et limiter 0-180°
    servo.write(degree);              // Déplacer le servo
    Serial.print(" Servo déplacé à : ");
    Serial.println(degree);           // Afficher la valeur reçue
  }
}

// --- Fonction de reconnexion MQTT ---
void reconnect() {
  if (!client.connected()) {          // Vérifier si le client est déconnecté
    Serial.print("Connexion MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX); // ID aléatoire
    if (client.connect(clientId.c_str())) { // Tentative de connexion
      Serial.println("✅ MQTT connecté");
      client.subscribe(topic_led);    // S’abonner au topic LED
      client.subscribe(topic_servo);  // S’abonner au topic Servo
      Serial.println("📌 Topics abonnés");
    } else {
      Serial.print("❌ Échec, code erreur = ");
      Serial.println(client.state()); // Afficher le code d’erreur
      Serial.println("Nouvelle tentative dans 5 secondes");
    }
  }
}

// --- Lecture capteur ultrason ---
long readDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);   // Envoyer impulsion basse
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); // Envoyer impulsion haute (10 µs)
  digitalWrite(TRIG_PIN, LOW);                         // Couper l’impulsion
  long duration = pulseIn(ECHO_PIN, HIGH);             // Lire temps de retour
  return duration * 0.034 / 2; // Convertir en distance (cm) (vitesse du son = 0.034 cm/µs)
}

// --- Test initial du DHT ---
void testDHT() {
  float t = dht.readTemperature();  // Lire température
  float h = dht.readHumidity();     // Lire humidité
  if (!isnan(t) && !isnan(h)) {     // Vérifier si valeurs valides
    Serial.printf("✅ Test DHT11 OK → %.2f°C, %.2f%%\n", t, h);
  } else {
    Serial.println("⚠️ Erreur lecture DHT11 ! Vérifiez câblage et résistance.");
  }
}

// --- Fonction d’initialisation ---
void setup() {
  Serial.begin(9600);               // Démarrer la communication série à 9600 bauds

  pinMode(LED_PIN, OUTPUT);         // Configurer LED comme sortie
  digitalWrite(LED_PIN, LOW);       // Éteindre LED par défaut
  pinMode(TRIG_PIN, OUTPUT);        // TRIG en sortie
  pinMode(ECHO_PIN, INPUT);         // ECHO en entrée

  servo.attach(SERVO_PIN);          // Attacher servo à sa broche
  servo.write(0);                   // Position initiale à 0°

  dht.begin();                      // Initialiser le capteur DHT
  delay(2000);                      // Attente pour stabilisation du capteur
  testDHT();                        // Faire un premier test

  setup_wifi();                     // Connexion au WiFi
  client.setServer(mqttServer, mqttPort); // Définir serveur MQTT
  client.setCallback(callback);     // Associer fonction callback
}

// --- Boucle principale ---
void loop() {
  client.loop();                    // Garder la connexion MQTT active
  reconnect();                      // Vérifier si MQTT est connecté sinon retenter

  unsigned long currentMillis = millis(); // Temps actuel en ms
  if (currentMillis - previousMillis >= interval) { // Toutes les 2 secondes
    previousMillis = currentMillis; // Mise à jour du temps

    // --- Lecture DHT11 ---
    float temp = dht.readTemperature(); // Température
    float hum  = dht.readHumidity();    // Humidité
    if (!isnan(temp) && !isnan(hum)) {
      String dhtMsg = String(temp) + "," + String(hum); // Formater données
      client.publish(topic_dht, dhtMsg.c_str());        // Publier MQTT
      Serial.printf("🌡️ DHT11 → %.2f°C, %.2f%%\n", temp, hum);
    } else {
      Serial.println("⚠️ Erreur lecture DHT11 !");
    }

    // --- Lecture LDR ---
    int ldrValue = analogRead(LDR_PIN);   // Lire valeur analogique
    client.publish(topic_ldr, String(ldrValue).c_str()); // Publier MQTT
    Serial.println("🔆 LDR → " + String(ldrValue));

    // --- Lecture Ultrason ---
    long distance = readDistance();       // Calculer distance
    client.publish(topic_distance, String(distance).c_str()); // Publier MQTT
    Serial.println("📏 Distance → " + String(distance) + " cm");
  }
}
