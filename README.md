# ESP32 Skateboard Remote Firmware

This firmware implements an ESP-NOW based wireless remote for an electric skateboard. It supports **throttle control**, **deadman switch**, **battery monitoring**, **throttle calibration**, and **pairing**.

---

## Features

- ESP-NOW wireless communication
- Throttle control with deadman switch
- Skateboard and remote battery display via NeoPixel LEDs
- Throttle calibration mode
- Pairing mode
- Persistent storage of calibration and paired receiver address

---

## Boot Modes

Hold the deadman switch and **throttle fully forward or backward** during power-on for to enter special modes. LEDs will light up to indicate which mode it's entering:

| Throttle          | Mode                 | LEDs   |
| ----------------- | -------------------- | ------ |
| > `Full Throttle` | **Calibration Mode** | Blue   |
| < `Full Brake`    | **Pairing Mode**     | Purple |

After holding buttons for 5 seconds, the LEDs will become white. Let go of the inputs to confirm and enter the selected mode.

---

## Modes & Instructions

### 1. Calibration Mode

Calibration allows adjusting throttle min, max, and center for better precision.

1. Follow LED prompts:
    - **Red:** Hold throttle on **minimum**.
    - **Green:** Hold throttle on **maximum**.
    - **Yellow:** Release throttle to **center/neutral**.
2. **Green fast blink** indicates calibration complete.
3. Firmware saves the calibration to non-volatile memory.

> Use calibration if your throttle range feels off or sticks.

---

### 2. Pairing Mode

Pairing binds the remote to a new receiver.

1. The remote continuously sends **pairing requests** until the receiver responds.
2. Power cycle the reciever. It listens for a remote for 1 second after power up.
3. Upon successful pairing:
    - LED blinks **Red 3 times**
    - Remote stores receiver address in non-volatile memory.
4. Once paired, the remote enters **Normal Mode** automatically.
5. Remote stays in pairing mode until either successfully paired or powered off.

> **Important:** Pairing is permanent until a new pairing sequence.

---

## LED Battery Indicators

- **Skateboard Battery (LEDs 0–4)**
    - Red → Green gradient and number of LEDs lit corresponds to battery level.

- **Remote Battery (LED 6)**
    - Red blinking if below `REMOTE_CRITICAL_V`
    - Gradient from Red → Green indicates charge

> LED 5 and 7 are currently unused.
