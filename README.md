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

## Hardware

The system consists of two devices:

- **Remote** – handheld controller with throttle input, battery monitoring, and LED indicators.
- **Receiver** – mounted on the skateboard and outputs a **PPM signal** to the ESC/VESC.

Both devices communicate wirelessly using **ESP-NOW**.

---

## Required Components

- **2× ESP32-C3 SuperMini** microcontrollers (remote + receiver)
- **49E Hall effect sensor** (throttle position sensing)
- **2× 5 mm neodymium magnets** (for hall throttle mechanism)
- **Pen spring** (for throttle return mechanism)
- **TP4056 charging module** (Li-ion charging and protection)
- **18650 Li-ion battery**
- **10T85 limit switch** (deadman switch)
- **8-LED WS2812 / NeoPixel module** (status and battery indicators)
- **KCD11 power switch**
- **Resistors for battery voltage divider**
- **4× M3 × 15 mm button head screws**

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

---

## LED Battery Indicators

- **Skateboard Battery (LEDs 0–4)**
    - Red → Green gradient and number of LEDs lit corresponds to battery level.

- **Remote Battery (LED 6)**
    - Red blinking if below `REMOTE_CRITICAL_V`
    - Gradient from Red → Green indicates charge
