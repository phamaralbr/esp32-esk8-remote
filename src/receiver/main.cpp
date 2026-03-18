#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include <packets.h>

// ---------- PIN CONFIG ----------
#define PPM_PIN 6
#define BAT_PIN 2

// ---------- TIMING ----------
#define RADIO_TIMEOUT_MS 200
#define TELEMETRY_INTERVAL_MS 200
#define PAIR_WINDOW_MS 1000

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

uint8_t remoteAddress[6];
bool paired = false;

unsigned long lastPacketTime = 0;

bool controlMode = false;

int16_t throttle = 0;

// ---------- BATTERY ----------
float readBattery()
{
    int raw = analogRead(BAT_PIN);
    float voltage = raw * ADC_REF_VOLTAGE / ADC_MAX;
    return voltage * VOLTAGE_DIVIDER_RATIO;
}

// ---------- SAVE REMOTE ----------
void saveRemote(uint8_t *mac)
{
    prefs.begin("pair", false);
    prefs.putBytes("remote", mac, 6);
    prefs.end();
}

// ---------- LOAD REMOTE ----------
bool loadRemote()
{
    prefs.begin("pair", true);

    if (prefs.isKey("remote"))
    {
        prefs.getBytes("remote", remoteAddress, 6);
        prefs.end();
        return true;
    }

    prefs.end();
    return false;
}

// ---------- SEND PAIR OK ----------
void sendPairOK(uint8_t *mac)
{
    PairPacket pkt;
    pkt.magic = PAIR_MAGIC;
    pkt.type = PACKET_PAIR_OK;

    esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));
}

// ---------- SWITCH TO CONTROL MODE ----------
void enableControlMode()
{
    if (controlMode)
        return;

    controlMode = true;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, remoteAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;

    esp_now_add_peer(&peer);

    Serial.println("Control mode active");
}

// ---------- UNIFIED RECEIVE CALLBACK ----------
void onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
    // ---------- PAIR ----------
    if (len == sizeof(PairPacket) && !controlMode)
    {

        PairPacket pkt;
        memcpy(&pkt, data, sizeof(pkt));

        if (pkt.magic == PAIR_MAGIC && pkt.type == PACKET_PAIR_REQUEST)
        {
            memcpy(remoteAddress, mac, 6);

            saveRemote(remoteAddress);
            paired = true;

            sendPairOK(remoteAddress);

            Serial.println("Remote paired");

            enableControlMode();
        }

        return;
    }

    // ---------- CONTROL ----------
    if (len == sizeof(ControlPacket) && controlMode)
    {

        if (memcmp(mac, remoteAddress, 6) != 0)
            return;

        ControlPacket packet;
        memcpy(&packet, data, sizeof(packet));

        throttle = constrain(packet.throttle, THROTTLE_MIN, THROTTLE_MAX);
        lastPacketTime = millis();

        return;
    }
}

// ---------- RADIO SETUP ----------
void setupRadio()
{
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onReceive);

    Serial.println("Pair window open...");

    unsigned long start = millis();
    while (millis() - start < PAIR_WINDOW_MS)
        delay(10);

    if (paired)
        enableControlMode();
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
    if (!controlMode)
        return;

    static unsigned long lastSend = 0;
    unsigned long now = millis();

    if (now - lastSend >= TELEMETRY_INTERVAL_MS)
    {
        lastSend = now;

        TelemetryPacket packet;
        packet.skateBat = readBattery() * 100;

        esp_now_send(remoteAddress, (uint8_t *)&packet, sizeof(packet));
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

// ---------- SETUP ----------
void setup()
{
    Serial.begin(115200);

    pinMode(BAT_PIN, INPUT);

    setupPPM();

    paired = loadRemote();

    setupRadio();
}

// ---------- LOOP ----------
void loop()
{
    updatePPM();
    sendTelemetry();
}