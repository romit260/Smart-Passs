// ==============================================================================
// SMARTPASS SYSTEM - NODE B (ESP-NOW + BLYNK + FIREBASE)
// ==============================================================================

#define BLYNK_TEMPLATE_ID "TMPL3skSY3e9I"
#define BLYNK_TEMPLATE_NAME "Microproject"
#define BLYNK_AUTH_TOKEN "SAtc7oJuy-a3HaE-a5Xmgxer9RmXF_sp"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <esp_now.h>

// 🔥 Firebase
#include <Firebase_ESP_Client.h>

// --- NETWORK ---
char ssid[] = "TestWifi";
char pass[] = "qwertzyp";

// --- FIREBASE CONFIG ---
#define API_KEY "AIzaSyDJrSDBejUwGjV-t0MgEbEFa7pTed6JYYw"
#define DATABASE_URL "https://smartpass-d4fec-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- RELAYS ---
const int bulbRelayPin = 4;
const int secondRelayPin = 2;

// --- STRUCT ---
typedef struct struct_message {
  int waterLevel;
} struct_message;

struct_message incomingPayload;

// --- VARIABLES ---
unsigned long lastRecvTime = 0;
unsigned long lastSendTime = 0;

int lastWaterLevel = 0;
bool newESPNowData = false;
bool systemOnline = false;

const int DANGER_THRESHOLD = 15;
const int sendInterval = 2000; // Firebase send interval

// ==============================================================================
// ESP-NOW CALLBACK
// ==============================================================================
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingPayload, incomingData, sizeof(incomingPayload));

  lastRecvTime = millis();
  lastWaterLevel = incomingPayload.waterLevel;
  newESPNowData = true;
}

// ==============================================================================
// SETUP
// ==============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(bulbRelayPin, OUTPUT);
  pinMode(secondRelayPin, OUTPUT);

  digitalWrite(bulbRelayPin, LOW);
  digitalWrite(secondRelayPin, LOW);

  delay(1000);

  // --- WIFI + BLYNK ---
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // 🔥 FIREBASE INIT
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("🔥 Firebase Ready");

  // --- ESP-NOW ---
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  lastRecvTime = millis();
}

// ==============================================================================
// LOOP
// ==============================================================================
void loop() {

  Blynk.run();

  // ================= WATCHDOG =================
  if (millis() - lastRecvTime > 15000) {
    if (systemOnline) {
      systemOnline = false;

      digitalWrite(bulbRelayPin, LOW);
      digitalWrite(secondRelayPin, LOW);

      Blynk.virtualWrite(V2, "OFFLINE ⚠️");
      Blynk.virtualWrite(V1, 0);
      Blynk.virtualWrite(V0, 0);
    }
  }
  else if (!systemOnline) {
    systemOnline = true;
    Blynk.virtualWrite(V2, "ONLINE ✅");
  }

  // ================= PROCESS DATA =================
  if (newESPNowData && systemOnline) {
    newESPNowData = false;

    Blynk.virtualWrite(V0, lastWaterLevel);

    if (lastWaterLevel >= DANGER_THRESHOLD) {
      digitalWrite(bulbRelayPin, HIGH);
      digitalWrite(secondRelayPin, HIGH);
      Blynk.virtualWrite(V1, 1);
    } else {
      digitalWrite(bulbRelayPin, LOW);
      digitalWrite(secondRelayPin, LOW);
      Blynk.virtualWrite(V1, 0);
    }
  }

  // ================= FIREBASE =================
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();

    FirebaseJson json;

    json.set("waterLevel", lastWaterLevel);
    json.set("danger", lastWaterLevel >= DANGER_THRESHOLD ? 1 : 0);
    json.set("status", systemOnline ? "ONLINE" : "OFFLINE");
    json.set("lastUpdated", millis());

    if (Firebase.RTDB.setJSON(&fbdo, "/data", &json)) {
      Serial.println("✅ Firebase Updated");
    } else {
      Serial.print("❌ Firebase Error: ");
      Serial.println(fbdo.errorReason());
    }
  }
}
