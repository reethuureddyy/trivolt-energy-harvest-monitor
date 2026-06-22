# TriVolt · Hybrid Green Path Pavement Power Generator

> Real-time energy harvesting monitor for piezoelectric pavement tiles — built on ESP32 with a live web dashboard.

---

## What is TriVolt?

TriVolt is an embedded systems project that harvests kinetic energy from foot traffic on pavement tiles using piezoelectric sensors. Every footfall generates a small voltage spike that is detected, measured, and logged in real time via a web-based dashboard served directly from the ESP32 microcontroller.

The system tracks press voltage, peak voltage, release voltage, and step duration for every footfall — giving insight into how much energy each step generates.

---

## Live Dashboard Preview

The ESP32 hosts a web dashboard accessible over WiFi that shows:

- **Total steps detected** — live footfall counter
- **Live voltage** — real-time ADC reading in volts
- **Tile status** — IDLE or PRESSED with animated indicator
- **Voltage bar** — visual 0V–1.5V level with step threshold marker
- **Step-by-step log table** — press, peak, release voltage + duration per step
- **Stats** — min, max, average peak voltage and average step duration

---

## Hardware Requirements

| Component | Details |
|---|---|
| Microcontroller | ESP32 (any devkit with ADC) |
| Sensor | Piezoelectric disc / pressure sensor |
| ADC Pin | GPIO 32 |
| Power | USB or 3.3V supply |
| Network | 2.4GHz WiFi |

**Wiring:**
- Piezoelectric output → GPIO 32 (ADC)
- GND → GND

> No voltage divider required for low-output piezo sensors (direct connection mode). For higher voltage outputs, add a resistor divider and update `DIVIDER_RATIO` in the code.

---

## Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support
- ESP32 board package installed via Board Manager
- No external libraries required (uses built-in `WiFi.h` and `WebServer.h`)

---

## Setup & Installation

**1. Clone this repo**
```bash
git clone https://github.com/yourusername/trivolt-energy-harvest-monitor.git
```

**2. Open in Arduino IDE**

Open `trivolt_monitor.ino` in Arduino IDE.

**3. Configure WiFi credentials**

Edit these lines at the top of the file:
```cpp
const char* SSID     = "YOUR_WIFI_NAME";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
```

**4. Adjust thresholds if needed**

```cpp
const float STEP_THRESHOLD    = 0.35f;  // Voltage above this = step detected
const float RELEASE_THRESHOLD = 0.10f;  // Voltage below this = step released
const int   MIN_PRESS_TIME    = 70;     // Minimum ms for a valid step
const int   DEBOUNCE_TIME     = 400;    // ms to ignore after a step is logged
```

**5. Upload to ESP32**

Select your ESP32 board and port in Arduino IDE, then upload.

**6. Open the dashboard**

Open Serial Monitor (115200 baud). You will see:
```
Connected! Open: http://192.168.x.x
```

Open that IP address in any browser on the same WiFi network.

---

## How It Works

```
Footfall
   │
   ▼
Piezoelectric sensor generates voltage spike
   │
   ▼
ESP32 ADC (GPIO32) samples voltage every 30ms
with 20-sample oversampling to reduce noise
   │
   ▼
Step detection state machine:
  - Voltage > 0.35V → foot DOWN detected
  - Track peak voltage while pressed
  - Voltage < 0.10V → foot UP detected
  - Log step if press duration > 70ms (filters phantom spikes)
  - Debounce 400ms before next step allowed
   │
   ▼
Step data stored in memory (up to 100 steps)
   │
   ▼
Web dashboard fetches /data endpoint every 200ms
and renders live voltage, step log, and stats
```

---

## API Endpoint

The ESP32 exposes a JSON API at `/data`:

```json
{
  "currentVoltage": 0.4231,
  "footOn": false,
  "totalSteps": 12,
  "steps": [
    {
      "num": 1,
      "press": 0.3712,
      "peak": 0.8943,
      "release": 0.0821,
      "duration": 210,
      "at": 4500
    }
  ]
}
```

---

## Key Parameters

| Parameter | Default | Description |
|---|---|---|
| `ADC_PIN` | 32 | GPIO pin for sensor input |
| `STEP_THRESHOLD` | 0.35V | Minimum voltage to detect a step |
| `RELEASE_THRESHOLD` | 0.10V | Voltage below which foot is considered lifted |
| `MIN_PRESS_TIME` | 70ms | Minimum press duration to log a step |
| `DEBOUNCE_TIME` | 400ms | Cooldown between consecutive steps |
| `SAMPLE_COUNT` | 20 | ADC samples averaged per reading |
| `READ_INTERVAL` | 30ms | How often voltage is sampled |
| `MAX_STEPS` | 100 | Maximum steps stored in memory |

---

## Project Context

TriVolt was developed as an academic mini project at **SRM Institute of Science and Technology** exploring renewable energy harvesting from pedestrian foot traffic. The system demonstrates how kinetic energy from everyday movement can be captured and monitored using low-cost embedded hardware.

**Potential applications:**
- Smart footpaths and pedestrian walkways
- Energy harvesting in high foot-traffic areas (airports, malls, stations)
- IoT-based energy monitoring systems

---

## Author

**Akepati Reethu**
B.Tech Computer Science and Engineering
SRM Institute of Science and Technology, Kattankulathur
[LinkedIn](https://linkedin.com/in/your-profile) · [GitHub](https://github.com/yourusername)

---

## License

This project is open source and available under the [MIT License](LICENSE).
