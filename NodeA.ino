// ==============================================================================
// SMARTPASS SYSTEM - NODE A (ULTRASONIC SENSOR & SENDER)
// ==============================================================================

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

// --- 1. NETWORK & PEER SETTINGS ---
// The MAC Address of Node B (The Gateway). This is exactly where we send the data.
uint8_t receiverAddress[] = {0x8C, 0x4F, 0x00, 0x3A, 0x0E, 0xC4}; 

// The Wi-Fi network Node B is connected to. 
// Node A uses this ONLY to scan the airwaves and find the correct radio frequency.
const char* routerSSID = "TestWifi"; 

// --- 2. HARDWARE PINS & CALIBRATION ---
const int trigPin = 5;       // Sends the ultrasonic soundwave
const int echoPin = 18;      // Listens for the returning echo
const int sensorHeight = 50; // Distance from the sensor down to the dry floor (cm)

// --- 3. DATA PAYLOAD STRUCTURE ---
// This acts as an envelope. It must match the structure on Node B EXACTLY.
typedef struct struct_message {
  int waterLevel;
} struct_message;

struct_message payload;

// --- 4. TIMING VARIABLES (Non-Blocking) ---
unsigned long lastPingTime = 0;
const int pingInterval = 3000; // Take and send a reading every 3 seconds

// ==============================================================================
// FUNCTION: ANTENNA CHANNEL SCANNER
// ESP-NOW requires both boards to be on the exact same radio channel.
// ==============================================================================
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i = 0; i < n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i); // Found the router! Return its channel number.
      }
    }
  }
  return 0; // Return 0 if the router isn't found in the air
}

// ==============================================================================
// SETUP: INITIALIZE HARDWARE & RADIO
// ==============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[SYSTEM] Booting Node A (Sensor)...");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Set device as a Wi-Fi Station (Standard mode for ESP-NOW)
  WiFi.mode(WIFI_STA);

  // --- ANTENNA TUNING ---
  Serial.print("[WIFI] Scanning for router channel to sync with Node B...");
  int32_t matchedChannel = getWiFiChannel(routerSSID);
  
  if (matchedChannel != 0) {
    Serial.printf(" Found on Channel %d\n", matchedChannel);
    // Deep-level command to force the ESP32 radio onto the correct channel
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(matchedChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  } else {
    Serial.println(" Router not found. Defaulting to Channel 1.");
    matchedChannel = 1;
  }

  // --- ESP-NOW INITIALIZATION ---
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW failed to initialize!");
    return;
  }

  // Register Node B as an official peer so we can fire data at it
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = matchedChannel; 
  peerInfo.encrypt = false; // Keep encryption off for lightning-fast transit
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("[ERROR] Failed to add Node B as peer.");
    return;
  }
  
  Serial.println("[SYSTEM] Sensor Node Active. Ready to scan.");
}

// ==============================================================================
// MAIN LOOP: READ SENSOR & TRANSMIT
// ==============================================================================
void loop() {
  // Use millis() for timing. This allows the ESP32 to handle background 
  // Wi-Fi/ESP-NOW tasks without being frozen by a delay() command.
  if (millis() - lastPingTime > pingInterval) {
    lastPingTime = millis();

    // 1. FIRE THE SENSOR: Send a 10-microsecond ultrasonic pulse
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    // 2. LISTEN FOR ECHO: Measure how long the sound took to return
    long duration = pulseIn(echoPin, HIGH);
    
    // 3. CALCULATE DISTANCE & WATER LEVEL
    // The speed of sound is ~0.034 cm/microsecond. 
    // We divide by 2 because the sound travels out AND back.
    int measuredDistance = duration * 0.034 / 2;
    
    // Subtract the measured gap from the total installation height
    int currentWaterLevel = sensorHeight - measuredDistance;
    
    // Failsafe: Prevent negative numbers if the sensor gets a bad echo reflection
    if (currentWaterLevel < 0) {
        currentWaterLevel = 0; 
    }

    // 4. TRANSMIT DATA: Pack the integer and blast it to Node B
    payload.waterLevel = currentWaterLevel;
    esp_error_t result = esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
    
    // 5. SERIAL LOGGING: Visually confirm the data was sent
    Serial.printf("[SENSOR] Water Level: %d cm | Transmit Status: ", payload.waterLevel);
    if (result == ESP_OK) {
      Serial.println("Success ✅");
    } else {
      Serial.println("Failed ❌");
    }
  }
}
