#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include <packets.h>

// ---------- PIN CONFIG ----------
#define PPM_PIN 5
#define BAT_PIN 4

// ---------- TIMING ----------
#define RADIO_TIMEOUT_MS 200
#define TELEMETRY_INTERVAL_MS 200

// ---------- THROTTLE RANGE ----------
#define THROTTLE_MIN -1000
#define THROTTLE_MAX 1000

// ---------- PPM SIGNAL ----------
#define PPM_MIN 1000
#define PPM_NEUTRAL 1500
#define PPM_MAX 2000

// ---------- ADC ----------
#define ADC_MAX 4095.0
#define ADC_REF_VOLTAGE 3.3
#define VOLTAGE_DIVIDER_RATIO 11.0

#define ESPNOW_CHANNEL 1

Servo ppm;
Preferences prefs;

uint8_t remoteAddress[] = {0xE0, 0x72, 0xA1, 0x6C, 0x13, 0x58};

unsigned long lastPacketTime = 0;

int16_t throttle = 0;

// ---------- BATTERY ----------
float readBattery()
{
    int raw = analogRead(BAT_PIN);
    float voltage = raw * ADC_REF_VOLTAGE / ADC_MAX;
    return voltage * VOLTAGE_DIVIDER_RATIO;
}

// ---------- RECEIVE CALLBACK ----------
void onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
    Serial.print("Received packet");
    // ---------- CONTROL ----------
    if (len == sizeof(ControlPacket)) //&& memcmp(mac, remoteAddress, 6) == 0)
    {
        ControlPacket packet;
        memcpy(&packet, data, sizeof(packet));

        throttle = constrain(packet.throttle, THROTTLE_MIN, THROTTLE_MAX);
        lastPacketTime = millis();

        return;
    }
}

void onSend(const uint8_t *mac, esp_now_send_status_t status)
{
    Serial.print("Send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ---------- RADIO SETUP ----------

void setupRadio()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register the Remote as a peer (so we can send telemetry back)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, remoteAddress, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    esp_err_t addStatus = esp_now_add_peer(&peerInfo);

    Serial.print("Add peer status: ");
    Serial.println(addStatus);

    if (addStatus != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    // Register callback to receive throttle packets
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSend);

    Serial.println("Receiver Radio Ready - Linked to Remote");
}

// ---------- PPM SETUP ----------
void setupPPM()
{
    ppm.attach(PPM_PIN, PPM_MIN, PPM_MAX);
    ppm.writeMicroseconds(PPM_NEUTRAL);
}

// ---------- SEND TELEMETRY ----------
void sendTelemetry()
{
    static unsigned long lastSend = 0;
    unsigned long now = millis();

    if (now - lastSend >= TELEMETRY_INTERVAL_MS)
    {
        lastSend = now;

        TelemetryPacket packet;
        packet.skateBat = readBattery() * 100;

        esp_err_t result = esp_now_send(remoteAddress, (uint8_t *)&packet, sizeof(packet));

        Serial.print("Send call result: ");
        Serial.println(result);
    }
}

// ---------- UPDATE PPM ----------
void updatePPM()
{
    int ppmValue = PPM_NEUTRAL;

    if (millis() - lastPacketTime <= RADIO_TIMEOUT_MS)
    {
        ppmValue = map(throttle, THROTTLE_MIN, THROTTLE_MAX, PPM_MIN, PPM_MAX);
    }

    ppm.writeMicroseconds(constrain(ppmValue, PPM_MIN, PPM_MAX));
}

void logData()
{
    int ppmValue = map(throttle, THROTTLE_MIN, THROTTLE_MAX, PPM_MIN, PPM_MAX);

    Serial.print("thr=");
    Serial.print(throttle);

    Serial.print(" | ppm=");
    Serial.print(ppmValue);

    Serial.print(" | lastPacket age=");
    Serial.print(millis() - lastPacketTime);
    Serial.print("ms");

    Serial.print(" | bat=");
    Serial.print(readBattery(), 2);
    Serial.print("V");

    Serial.println();
}

// ---------- SETUP ----------
void setup()
{
    Serial.begin(115200);
    delay(5000);
    Serial.println("Receiver Starting...");

    pinMode(BAT_PIN, INPUT);

    setupPPM();

    setupRadio();
}

// ---------- LOOP ----------
void loop()
{
    updatePPM();
    sendTelemetry();
    // logData();
}