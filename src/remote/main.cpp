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

// ---------- BOOT THRESHOLDS ----------
#define BOOT_THROTTLE_HIGH 900
#define BOOT_THROTTLE_LOW -900

#define ESPNOW_CHANNEL 1

// ---------- MODES ----------
enum Mode
{
  MODE_NORMAL,
  MODE_CALIBRATION,
  MODE_PAIRING
};
Mode mode = MODE_NORMAL;

// ---------- STATE ----------
unsigned long lastSend = 0;
unsigned long lastPairSend = 0;
float skateBattery = 0;
uint8_t receiverAddress[6];
bool paired = false;

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

  float voltage = raw * ADC_REF_VOLTAGE / ADC_MAX;
  voltage *= REMOTE_DIVIDER_RATIO;

  return voltage;
}

// ---------- LOAD CALIBRATION ----------
void loadCalibration()
{
  prefs.begin("throttle", true);

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

// ---------- TELEMETRY RECEIVE ----------
void onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(TelemetryPacket))
    return;

  TelemetryPacket packet;
  memcpy(&packet, data, sizeof(packet));

  skateBattery = packet.skateBat / 100.0;
}

// ---------- SAVE/LOAD RECEIVER ----------
void saveReceiver(uint8_t *mac)
{
  prefs.begin("pair", false);
  prefs.putBytes("receiver", mac, 6);
  prefs.end();
  paired = true;
}

bool loadReceiver()
{
  prefs.begin("pair", true);
  if (prefs.isKey("receiver"))
  {
    prefs.getBytes("receiver", receiverAddress, 6);
    prefs.end();
    paired = true;
    return true;
  }
  prefs.end();
  return false;
}

// ---------- RECEIVE PAIR RESPONSE ----------
void onReceivePair(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(PairPacket))
    return;

  PairPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (pkt.magic != PAIR_MAGIC)
    return;

  if (pkt.type == PACKET_PAIR_OK)
  {
    memcpy(receiverAddress, mac, 6);
    saveReceiver(receiverAddress);
    blink(255, 0, 0, 3, 200);
    mode = MODE_NORMAL;
  }
}

// ---------- RADIO ----------
void setupRadio()
{
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  if (paired)
  {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  esp_now_register_recv_cb(onReceive);
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
  packet.throttle = digitalRead(DEADMAN_PIN) ? readThrottle() : 0;
  esp_now_send(receiverAddress, (uint8_t *)&packet, sizeof(packet));
}

// ---------- SKATE BATTERY DISPLAY ----------
void drawSkateBattery()
{
  float percent = (skateBattery - SKATE_MIN_V) / (SKATE_MAX_V - SKATE_MIN_V);
  percent = constrain(percent, 0.0, 1.0);

  int ledsOn = round(percent * 5);

  for (int i = 0; i < 5; i++)
  {
    if (i < ledsOn)
    {
      uint8_t r = 255 * (1 - percent);
      uint8_t g = 255 * percent;

      leds.setPixelColor(i, leds.Color(r, g, 0));
    }
    else
      leds.setPixelColor(i, 0);
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

  uint8_t r = 255 * (1 - percent);
  uint8_t g = 255 * percent;

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
  setAll(0, 0, 0);
  delay(2000);

  setAll(255, 0, 0);
  delay(3000);
  throttleMin = analogRead(THROTTLE_PIN);

  setAll(0, 255, 0);
  delay(3000);
  throttleMax = analogRead(THROTTLE_PIN);

  setAll(255, 255, 0);
  delay(3000);
  throttleCenter = analogRead(THROTTLE_PIN);

  saveCalibration();

  blink(0, 255, 0, 5, 200);
}

// ---------- SEND PAIR REQUEST ----------
void sendPairRequest()
{
  PairPacket pkt;
  pkt.magic = PAIR_MAGIC;
  pkt.type = PACKET_PAIR_REQUEST;
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
}

// ---------- PAIRING ----------
void pairingMode()
{
  setAll(255, 0, 255);
  esp_now_register_recv_cb(onReceivePair);
  while (mode == MODE_PAIRING)
  {
    unsigned long now = millis();

    if (now - lastPairSend > SEND_INTERVAL_MS)
      sendPairRequest();

    lastPairSend = now;
  }
}

// ---------- BOOT MODE CHECK ----------
void checkBootModes()
{
  delay(500);
  if (!digitalRead(DEADMAN_PIN))
    return;

  unsigned long start = millis();

  while (millis() - start < MODE_HOLD_TIME_MS)
  {
    int16_t throttle = readThrottle();

    if (!digitalRead(DEADMAN_PIN) || (throttle < BOOT_THROTTLE_HIGH && throttle > BOOT_THROTTLE_LOW))
    {
      mode = MODE_NORMAL;
      return;
    }

    if (mode == MODE_NORMAL)
    {
      if (throttle > BOOT_THROTTLE_HIGH)
        mode = MODE_CALIBRATION;
      setAll(0, 0, 255); // blue

      if (throttle < BOOT_THROTTLE_LOW)
        mode = MODE_PAIRING;
      setAll(255, 0, 255); // purple
    }

    delay(50);
  }

  setAll(255, 255, 255); // white
  start = millis();
  while (digitalRead(DEADMAN_PIN))
  {
    if (millis() - start > MODE_CONFIRM_TIME_MS)
    {
      mode = MODE_NORMAL;
      setAll(0, 0, 0); // black
      return;
    }
    delay(50);
  }

  if (mode == MODE_CALIBRATION)
    calibrateThrottle();

  if (mode == MODE_PAIRING)
    pairingMode();
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

  loadReceiver();

  loadCalibration();

  checkBootModes();

  setupRadio();
}

// ---------- LOOP ----------
void loop()
{
  sendControl();
  updateBatteryDisplay();
}