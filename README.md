# ESP32 Skateboard Remote Firmware

This firmware implements an ESP-NOW based wireless remote for an electric skateboard. It supports **throttle control**, **deadman switch**, **battery monitoring** and **throttle calibration**.

---

## Features

- ESP-NOW wireless communication
- Throttle control with deadman switch
- Skateboard and remote battery display via NeoPixel LEDs
- Throttle calibration mode

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
- **8x16x5 bearing**
- **TP4056 charging module** (Li-ion charging and protection)
- **18650 Li-ion battery**
- **10T85 limit switch** (deadman switch)
- **8-LED WS2812 / NeoPixel module** (status and battery indicators)
- **KCD11 power switch**
- **Resistors for battery voltage divider**
- **4× M3 × 15 mm button head screws**

---

### Calibration Mode

Calibration allows adjusting throttle min, max, and center for better precision.

#### Entering Calibration Mode

1. Hold the Deadman Switch and Power ON the remote.
2. Keep holding the switch for 8 seconds. LEDs will fill up with White.
3. When LEDs pulse Red, RELEASE the Deadman switch within 1 second.
4. When LEDs pulse White, PRESS the Deadman switch again.

LEDs will turn Green. You are now in Calibration Mode.

#### Calibration Steps

Range Sampling: A Blue "Ping-Pong" animation plays. Move the throttle stick/trigger through its full range multiple times.

Center Finding: A Yellow animation plays. Release the throttle to its natural neutral position and keep it still for 1 second.

Success: LEDs blink Green 5 times. Your values are saved.

---

## LED Battery Indicators

- **Skateboard Battery (LEDs 0–4)**
    - Yellow → Red gradient and number of LEDs lit corresponds to battery level.

- **Remote Battery (LED 6)**
    - Red blinking if below `REMOTE_CRITICAL_V`
    - Gradient from Red → Green indicates charge
