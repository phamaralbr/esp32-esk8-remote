#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include <packets.h>

// ---------- PIN CONFIG ----------
#define PPM_PIN 1
#define BAT_PIN 0

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
#define VOLTAGE_DIVIDER_RATIO 16.0

#define ESPNOW_CHANNEL 1

esp_now_peer_info_t peerInfo = {};
static unsigned long lastSend = 0;
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
    // Serial.println("Received packet");
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
    // Serial.print("Send status: ");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ---------- RADIO SETUP ----------

void setupRadio()
{
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // esp_wifi_set_max_tx_power(34); //8.5dbm
    // esp_wifi_set_max_tx_power(44); // 11dbm
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to get the status of Trasnmitted packet
    esp_now_register_send_cb(onSend);

    // Register peer
    memcpy(peerInfo.peer_addr, remoteAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    // peerInfo.ifidx = WIFI_IF_STA;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(onReceive);
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
    unsigned long now = millis();

    if (now - lastSend < TELEMETRY_INTERVAL_MS)
        return;

    lastSend = now;

    TelemetryPacket packet;
    packet.skateBat = readBattery() * 100;

    esp_err_t result = esp_now_send(remoteAddress, (uint8_t *)&packet, sizeof(packet));

    // if (result == ESP_OK)
    // {
    //     Serial.println("Sent with success");
    // }
    // else
    // {
    //     Serial.println("Error sending the data");
    // }
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
    static unsigned long lastDebug = 0;
    unsigned long now = millis();

    if (now - lastDebug < 200)
        return;

    lastDebug = now;

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