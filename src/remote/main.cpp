#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#include <packets.h>

// ---------- PINS ----------
#define THROTTLE_PIN 1
#define DEADMAN_PIN 3
#define LED_PIN 4
#define REMOTE_BAT_PIN 2

// ---------- LED ----------
#define LED_COUNT 8
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------- TIMING ----------
#define SEND_INTERVAL_MS 20
#define MODE_HOLD_TIME_MS 5000
#define MODE_CONFIRM_TIME_MS 1500
#define RADIO_TIMEOUT_MS 200

// ---------- ADC ----------
#define ADC_REF_VOLTAGE 3.3
#define ADC_MAX 4095.0
#define REMOTE_DIVIDER_RATIO 2.0

// ---------- BATTERY ----------
#define SKATE_MIN_V 33.0
#define SKATE_MAX_V 42.0
#define REMOTE_MIN_V 3.3
#define REMOTE_MAX_V 4.2
#define REMOTE_CRITICAL_V 3.3

// ---------- DEFAULT THROTTLE CALIBRATION ----------
#define DEFAULT_THROTTLE_MIN 1000
#define DEFAULT_THROTTLE_CENTER 2000
#define DEFAULT_THROTTLE_MAX 3000

#define ESPNOW_CHANNEL 1

// ---------- STATE ----------
unsigned long lastSend = 0;
float skateBattery = 0;
uint8_t receiverAddress[] = {0xE0, 0x72, 0xA1, 0x68, 0xCF, 0x2C};
unsigned long lastPacketTime = 0;

// ---------- STORAGE ----------
Preferences prefs;

// ---------- CALIBRATION ----------
int throttleMin = DEFAULT_THROTTLE_MIN;
int throttleMax = DEFAULT_THROTTLE_MAX;
int throttleCenter = DEFAULT_THROTTLE_CENTER;

// ---------- LED HELPERS ----------
void setAll(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < LED_COUNT; i++)
    leds.setPixelColor(i, leds.Color(r, g, b));

  leds.show();
}

void blink(uint8_t r, uint8_t g, uint8_t b, int times, int delayMs)
{
  for (int i = 0; i < times; i++)
  {
    setAll(r, g, b);
    delay(delayMs);
    setAll(0, 0, 0);
    delay(delayMs);
  }
}

// ---------- REMOTE BATTERY ----------
float readRemoteBattery()
{
  int raw = analogRead(REMOTE_BAT_PIN);

  float voltage = (raw / ADC_MAX) * ADC_REF_VOLTAGE * REMOTE_DIVIDER_RATIO;

  return voltage;
}

// ---------- LOAD CALIBRATION ----------
void loadCalibration()
{
  prefs.begin("throttle", false);

  throttleMin = prefs.getInt("min", DEFAULT_THROTTLE_MIN);
  throttleMax = prefs.getInt("max", DEFAULT_THROTTLE_MAX);
  throttleCenter = prefs.getInt("center", DEFAULT_THROTTLE_CENTER);

  prefs.end();

  if (throttleMin >= throttleCenter || throttleCenter >= throttleMax)
  {
    throttleMin = DEFAULT_THROTTLE_MIN;
    throttleCenter = DEFAULT_THROTTLE_CENTER;
    throttleMax = DEFAULT_THROTTLE_MAX;
  }
}

// ---------- SAVE CALIBRATION ----------
void saveCalibration()
{
  prefs.begin("throttle", false);

  prefs.putInt("min", throttleMin);
  prefs.putInt("max", throttleMax);
  prefs.putInt("center", throttleCenter);

  prefs.end();
}

// ---------- UNIFIED RECEIVE ----------
void onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
  Serial.print("Received packet");
  if (len == sizeof(TelemetryPacket))
  {

    // if (memcmp(mac, receiverAddress, 6) != 0)
    //   return;

    TelemetryPacket packet;
    memcpy(&packet, data, sizeof(packet));

    skateBattery = packet.skateBat / 100.0;
    lastPacketTime = millis();

    return;
  }
}

// ---------- RADIO ----------

void setupRadio()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  // This forces the ESP32 to stay on Channel 1
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the Receiver as a peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  Serial.println("Remote Radio Ready - Linked to Receiver");
}

// ---------- THROTTLE ----------
int16_t readThrottle()
{
  int raw = analogRead(THROTTLE_PIN);

  raw = constrain(raw, throttleMin, throttleMax);

  if (raw > throttleCenter)
    return map(raw, throttleCenter, throttleMax, 0, 1000);

  return map(raw, throttleMin, throttleCenter, -1000, 0);
}

// ---------- SEND CONTROL ----------
void sendControl()
{
  unsigned long now = millis();

  if (now - lastSend < SEND_INTERVAL_MS)
    return;

  lastSend = now;

  ControlPacket packet;
  packet.throttle = !digitalRead(DEADMAN_PIN) ? readThrottle() : 0;
  esp_now_send(receiverAddress, (uint8_t *)&packet, sizeof(packet));
}

// ---------- SKATE BATTERY DISPLAY ----------
void drawSkateBattery()
{
  if (millis() - lastPacketTime <= RADIO_TIMEOUT_MS)
  {
    float percent = (skateBattery - SKATE_MIN_V) / (SKATE_MAX_V - SKATE_MIN_V);
    percent = constrain(percent, 0.0, 1.0);

    // Using 5.0 for float math accuracy
    int ledsOn = round(percent * 5.0);

    for (int i = 0; i < 5; i++)
    {
      if (i < ledsOn)
      {
        // Calculate Red/Green mix based on battery level
        // Dividing by 10 to keep brightness manageable (e.g., 25 max)
        uint8_t r = (255 * (1.0 - percent)) / 10;
        uint8_t g = (255 * percent) / 10;

        leds.setPixelColor(i, leds.Color(r, g, 0));
      }
      else
      {
        leds.setPixelColor(i, 0); // Turn off unused battery LEDs
      }
    }
  }
  else
  {
    // SIGNAL LOST: Clear these specific LEDs (0-4)
    for (int i = 0; i < 5; i++)
    {
      leds.setPixelColor(i, 0);
    }

    // Blink LED 0 Red to show "Disconnected"
    if ((millis() / 500) % 2)
    {
      leds.setPixelColor(0, leds.Color(10, 0, 0));
    }
  }
}

// ---------- REMOTE BATTERY DISPLAY ----------
void drawRemoteBattery()
{
  static bool blinkState = false;
  static unsigned long lastBlink = 0;

  float v = readRemoteBattery();

  if (v < REMOTE_CRITICAL_V)
  {
    if (millis() - lastBlink > 500)
    {
      blinkState = !blinkState;
      lastBlink = millis();
    }

    if (blinkState)
      leds.setPixelColor(6, leds.Color(255, 0, 0));
    else
      leds.setPixelColor(6, 0);

    return;
  }

  float percent = (v - REMOTE_MIN_V) / (REMOTE_MAX_V - REMOTE_MIN_V);
  percent = constrain(percent, 0.0, 1.0);

  uint8_t r = 10 * (1 - percent);
  uint8_t g = 10 * percent;

  leds.setPixelColor(6, leds.Color(r, g, 0));
}

// ---------- LED UPDATE ----------
void updateBatteryDisplay()
{
  drawSkateBattery();
  drawRemoteBattery();
  leds.show();
}

// ---------- CALIBRATION ----------
void calibrateThrottle()
{

  int minVal = 4095;
  int maxVal = 0;

  unsigned long start = millis();

  // --- SAMPLE FOR 8 SECONDS ---
  while (millis() - start < 8000)
  {
    // --- PING-PONG ANIMATION ---
    int activeLed = abs(((int)(millis() / 150) % 8) - 4);

    for (int i = 0; i < 5; i++)
    {
      leds.setPixelColor(i, (i == activeLed) ? leds.Color(0, 0, 100) : 0);
    }
    leds.show();

    // --- SAMPLING ---
    int v = analogRead(THROTTLE_PIN);
    if (v < minVal)
      minVal = v;
    if (v > maxVal)
      maxVal = v;

    delay(5);
  }

  // --- WAIT FOR RELEASE (CENTER) ---

  int last = analogRead(THROTTLE_PIN);
  unsigned long stableStart = millis();

  while (true)
  {
    // --- ANIMATION: Meet at Center (0,1,2 and 4,3,2) ---
    int step = (millis() / 200) % 3; // 0, 1, 2... repeat

    for (int i = 0; i < 5; i++)
    {
      // Light up i if it matches the current step OR the mirrored step
      bool isMatch = (i == step || i == (4 - step));
      leds.setPixelColor(i, isMatch ? leds.Color(10, 10, 0) : 0);
    }
    leds.show();

    // --- STABILITY CHECK ---
    int v = analogRead(THROTTLE_PIN);
    if (abs(v - last) > 10)
    {
      stableStart = millis();
      last = v;
    }

    if (millis() - stableStart > 1000)
    {
      throttleCenter = v;
      break;
    }
    delay(10);
  }

  // throttleMin = minVal;
  // throttleMax = maxVal;

  // Pull the Max down by 10% of the upper travel
  throttleMax = throttleCenter + (int)((maxVal - throttleCenter) * 0.9);

  // Pull the Min up by 10% of the lower travel
  throttleMin = throttleCenter - (int)((throttleCenter - minVal) * 0.9);

  // --- VALIDATION ---
  if (throttleMin >= throttleCenter || throttleCenter >= throttleMax)
  {
    blink(10, 0, 0, 5, 200); // error
    return;
  }

  if ((throttleMax - throttleMin) < 300)
  {
    blink(10, 0, 0, 5, 200); // error
    return;
  }

  saveCalibration();

  blink(0, 10, 0, 5, 200); // success
}

// ---------- BOOT MODE CHECK ----------
void checkBootMode()
{
  // 1. Initial Check: Must start with deadman pressed (LOW)
  if (digitalRead(DEADMAN_PIN))
    return;

  // --- STEP 1: Progress Bar Hold (8s - WHITE) ---
  unsigned long start = millis();
  while (millis() - start < 8000)
  {
    if (digitalRead(DEADMAN_PIN))
    {
      leds.clear();
      leds.show();
      return;
    }

    // Map 8 seconds across only 5 LEDs (0-4)
    int progress = map(millis() - start, 0, 8000, 0, 5);
    for (int i = 0; i < 5; i++)
    {
      leds.setPixelColor(4 - i, (i < progress) ? leds.Color(15, 15, 15) : 0);
    }
    leds.show();
    delay(20);
  }

  // --- STEP 2: Pulsing RED 1s (must RELEASE) ---
  start = millis();
  bool released = false;
  while (millis() - start < 1000)
  {
    // Simple sine wave for pulsing brightness (0-20)
    int pulse = 10 + sin(millis() / 100.0) * 10;
    for (int i = 0; i < 5; i++)
      leds.setPixelColor(i, leds.Color(pulse, 0, 0));
    leds.show();

    if (digitalRead(DEADMAN_PIN))
    {
      released = true;
      break;
    }
    delay(10);
  }
  if (!released)
  {
    leds.clear();
    leds.show();
    return;
  }

  // --- STEP 3: Wait Period (3s - OFF) ---
  leds.clear();
  leds.show();
  start = millis();
  while (millis() - start < 3000)
  {
    if (!digitalRead(DEADMAN_PIN))
      return; // Pressed too early -> cancel
    delay(10);
  }

  // --- STEP 4: Pulsing WHITE 1s (must PRESS) ---
  start = millis();
  bool pressed = false;
  while (millis() - start < 1000)
  {
    int pulse = 10 + sin(millis() / 100.0) * 10;
    for (int i = 0; i < 5; i++)
      leds.setPixelColor(i, leds.Color(pulse, pulse, pulse));
    leds.show();

    if (!digitalRead(DEADMAN_PIN))
    {
      pressed = true;
      break;
    }
    delay(10);
  }
  if (!pressed)
  {
    leds.clear();
    leds.show();
    return;
  }

  // --- STEP 5: Final Confirmation (GREEN) ---
  // If we got here, we are entering calibration
  for (int i = 0; i < 5; i++)
    leds.setPixelColor(i, leds.Color(0, 10, 0));
  leds.show();
  delay(1000);

  calibrateThrottle();
}

void debugPrint()
{
  static unsigned long lastDebug = 0;
  unsigned long now = millis();

  if (now - lastDebug < 200)
    return;

  lastDebug = now;

  int rawThrottle = analogRead(THROTTLE_PIN);
  int16_t mappedThrottle = readThrottle();
  bool deadman = digitalRead(DEADMAN_PIN);
  float remoteBat = readRemoteBattery();

  Serial.print("RAW: ");
  Serial.print(rawThrottle);

  Serial.print(" | THR: ");
  Serial.print(mappedThrottle);

  Serial.print(" | DEADMAN: ");
  Serial.print(deadman);

  Serial.print(" | REMOTE V: ");
  Serial.print(remoteBat, 2);

  Serial.print(" | SKATE V: ");
  Serial.print(skateBattery, 2);

  Serial.println();
}

// ---------- SETUP ----------
void setup()
{
  Serial.begin(115200);

  pinMode(DEADMAN_PIN, INPUT_PULLUP);

  analogSetAttenuation(ADC_11db);

  leds.begin();
  leds.clear();
  leds.show();

  checkBootMode();
  loadCalibration();
  setupRadio();
}

// ---------- LOOP ----------
void loop()
{
  sendControl();
  updateBatteryDisplay();
  debugPrint();
}