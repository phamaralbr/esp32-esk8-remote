#pragma once

// ---------- PAIRING ----------
#define PAIR_MAGIC 0xBEEFCAFE

enum PacketType
{
    PACKET_PAIR_REQUEST = 1,
    PACKET_PAIR_OK = 2,
    PACKET_CONTROL = 3,
    PACKET_TELEMETRY = 4
};

// ---------- PAIR PACKET ----------
struct PairPacket
{
    uint32_t magic;
    uint8_t type;
};

// ---------- CONTROL PACKET ----------
struct ControlPacket
{
    int16_t throttle; // -1000 to 1000
};

// ---------- TELEMETRY PACKET ----------
struct TelemetryPacket
{
    uint16_t skateBat; // battery voltage * 100
};