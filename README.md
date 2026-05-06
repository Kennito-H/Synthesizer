# Synthairsizer (MIDI Edition)

An air-gesture MIDI controller built on the **ESP32** that turns the movement of your hands into expressive music. Originally an internal FPU synthesizer, the project has been completely overhauled to serve as a wireless-capable, gestural MIDI controller perfectly mapped for the **Elektron Syntakt**.

The left hand controls notes and menus via FSR (Force Sensitive Resistor) finger pads, alongside an IMU for octave transposition and arpeggiator chord latching. The right hand modulates sound parameters (Syntakt Macros, Filters, Envelopes) in real-time using a second MPU6050 IMU.

---

## Hardware Requirements

| Component | Quantity | Notes |
|---|---|---|
| ESP32 Development Board | 1 | |
| Force Sensitive / Piezo Sensors | 5 | One per finger on the left hand glove |
| MPU6050 IMU Module | 2 | Left hand (0x69) and Right hand (0x68) |
| 5-Pin Female DIN MIDI Jack | 1 | For direct hardware connection to Syntakt |
| Resistors (10kΩ) | 5 | Pull-down for each FSR voltage divider |
| Resistors (10Ω & 33Ω) | 2 | Required for 3.3V MIDI Out specification |

### Wiring

**Left Hand FSRs (ADC1):**
```
FSR[0] (Pinkie)   -> GPIO 6   (ADC1_CH5)
FSR[1] (Ring)     -> GPIO 7   (ADC1_CH6)
FSR[2] (Middle)   -> GPIO 8   (ADC1_CH7)
FSR[3] (Pointer)  -> GPIO 9   (ADC1_CH8)
FSR[4] (Thumb)    -> GPIO 10  (ADC1_CH9)
```
Each FSR is wired as a voltage divider with a 10kΩ pull-down resistor between the FSR and GND.

**Right Hand MPU6050 (I2C Address 0x68):**
```
SDA -> GPIO 18
SCL -> GPIO 19
VCC -> 3.3V
GND -> GND
```

**Left Hand MPU6050 (I2C Address 0x69):**
*Wire SDA and SCL to the same pins as the right hand (GPIO 18/19).*
*Wire the **AD0** pin to **3.3V** to change its I2C address to 0x69.*

**MIDI Output (UART 1 - 3.3V Logic):**
*   **Pin 2 (Center):** Ground
*   **Pin 4:** 3.3V via 33Ω resistor
*   **Pin 5:** GPIO 17 (TX) via 10Ω resistor

---

## Firmware Setup

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) v5.x installed and sourced
- The MPU6050 component is managed via `idf_component.yml`

### Build & Flash

```bash
# Install the MPU6050 managed component on first build:
idf.py update-dependencies

# Build the project:
idf.py build

# Flash to your connected ESP32:
idf.py flash monitor
```

---

## Playing the Synthairsizer

### Normal Keyboard Mode (Default)

When the device boots, you are in **Keyboard Mode**. Each finger on your left hand plays a monophonic MIDI note from the selected scale.

| Finger | Note (Default: Minor Pentatonic in C) |
|---|---|
| Pinkie | Root (C4 — Note 60) |
| Ring | Minor 3rd (Eb4 — Note 63) |
| Middle | 4th (F4 — Note 65) |
| Pointer | 5th (G4 — Note 67) |
| Thumb | Minor 7th (Bb4 — Note 70) |

- **Left Hand Swipe (Left/Right):** Transposes the entire scale up or down by 5 semitones.
- **Left Hand Flick (Up):** Toggles Arpeggiator Latch (Hold). When on, notes are held in the arpeggiator's memory until a new chord is pressed.

### Right Hand Modulation

While playing, tilt your right hand to modulate sound in real-time via standard MIDI CCs. The default mappings are uniquely tailored for the **Elektron Syntakt**:

| Assignment | Right Hand Up (Y+) | Right Hand Right (X+) |
|---|---|---|
| **Filter** (default) | Resonance (CC 75) | Cutoff Freq (CC 74) |
| **SY Wave** | Osc 2 Detune (CC 18) | Waveform (CC 21) |
| **Effects** | Reverb Send (CC 85) | Delay Send (CC 84) |
| **SY Bits** | Bit Reduction (CC 23) | Sample Rate Reduction (CC 22) |
| **Arp Speed** | Speed↑ | Note probability↑ (right) / ↓ (left) |

---

## Menu System

### Entering & Exiting the Menu

| Gesture (Left Hand) | Action |
|---|---|
| **Double-tap all 5 fingers** | Enter Main Menu (from Keyboard Mode) |
| **Double-tap all 5 fingers** | Force-quit menu (from any depth) → Keyboard Mode |
| **Single-tap all 5 fingers** | Go back one level (Sub-menu → Main Menu → Keyboard) |

### Main Menu — Finger Map

Once in the menu, each finger selects a sub-menu category:

| Finger | Sub-Menu |
|---|---|
| **Pinkie** | Syntakt Machine Selection |
| **Ring** | Arpeggiator Options |
| **Middle** | Tuning / Scale |
| **Pointer** | Amp Envelope Shaping |
| **Thumb** | Modulation Assignment |

---

### Syntakt Machine Selection (Pinkie)

Because the Elektron Syntakt does not allow changing Machine architectures dynamically via MIDI CC, this menu sends MIDI Program Changes (PC 0-4) to swap patterns. You must save the following machines to **Patterns A01 through A05**.

| Finger | Program Change | Target Syntakt Pattern | Intended Machine |
|---|---|---|---|
| Pinkie | PC 0 | A01 | Bits |
| Ring | PC 1 | A02 | Swarm |
| Middle | PC 2 | A03 | Dual VCO |
| Pointer | PC 3 | A04 | Chord |
| Thumb | PC 4 | A05 | Toy |

### Arpeggiator Options Sub-menu (Ring)

When you enter this sub-menu, the arpeggiator **automatically starts playing** a preview sequence.

*   **Thumb (Double Tap):** Toggles Arpeggiator ON/OFF globally and returns to the Main Menu.
*   **Pinkie:** Enters the **Arp Mode** sub-menu (Up, Down, Ping-Pong, Random).
*   **Ring:** Enters the **Octave Range** sub-menu (1, 2, 3, or 4 octaves).
*   **Middle:** Enters the **Arp Speed** sub-menu (0.5x, 1x, 2x, 4x, 8x).

### Tuning / Scale Sub-menu (Middle)

Select the musical scale. The 5 notes of the new scale are played ascending as a preview.

| Finger | Scale | Notes (Root = C4) |
|---|---|---|
| Pinkie | **Major** | C D E F G |
| Ring | **Minor** | C D Eb F G |
| Middle | **Minor Pentatonic** | C Eb F G Bb |
| Pointer | **Dorian** | C Eb F G A |
| Thumb | **Phrygian** | C Db Eb F G |

### Amp Envelope Shaping Sub-menu (Pointer)

The arpeggiator plays a live preview while you shape the amplitude envelope using the right hand. This targets the Syntakt AMP page.

| Right Hand Motion | Parameter |
|---|---|
| **Up** (Y+) | Attack time increases (CC 79) |
| **Right** (X+) | Decay time increases (CC 80) |

### Modulation Assignment Sub-menu (Thumb)

Choose what parameter your right hand controls during normal keyboard play. The selection takes effect immediately.

| Finger | Right Hand Controls |
|---|---|
| Pinkie | Filter Cutoff (X) & Resonance (Y) |
| Ring | SY Wave & Detune (CC 21, CC 18) |
| Middle | Delay & Reverb Sends (CC 84, CC 85) |
| Pointer | SY Bits Crush (CC 22, CC 23) |
| Thumb | Internal Arp Speed (Y) & Probability (X) |

---

## Project File Structure

```
Synthairsizer/
├── main/
│   ├── main.cpp               # Entry point; FreeRTOS task setup
│   ├── midi_engine.h/.cpp     # UART MIDI driver and state tracker
│   ├── menu_system.h/.cpp     # Gesture-driven menu state machine
│   ├── left_hand.h/.cpp       # Sensor reading, debouncing, gesture detection
│   ├── right_hand.h/.cpp      # MPU6050 IMU complementary filter
│   ├── left_hand_imu.h/.cpp   # Transpose gestures and chord latching
│   ├── arpeggiator.h/.cpp     # Multi-mode, probabilistic arpeggiator
│   ├── tuning.h               # MIDI Note scales
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── CMakeLists.txt
└── sdkconfig
```

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Syntakt isn't responding | Wiring or MIDI settings | Ensure Syntakt "Receive CC/NRPN" is ON. Check the 10Ω/33Ω resistor wiring on the DIN jack. |
| MPU6050 fails to init | I2C wiring issue | Check SDA/SCL pins; verify pull-ups and AD0 pins. |
| Gestures not detected | TOUCH_THRESHOLD too high | Lower `TOUCH_THRESHOLD` in `left_hand.cpp` |
| Arpeggiator stuck | Latch is ON | Flick your left wrist UP to toggle Arpeggiator Latch off. |

---

## License

The Synthairsizer firmware is original code, released under the MIT License.
