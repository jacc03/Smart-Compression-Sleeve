# SCS — Smart Compression Sleeve

Real-time pressure monitoring and motor control dashboard for an ESP32-based smart compression sleeve.

**Hardware:** ESP32 · DRV8833 motor driver · 4× FSR 406 force-sensitive resistors

---

## Features

- Live 4-channel pressure gauges with arc visualization
- Target zone (25–35 mmHg) highlighted on every gauge
- Per-channel bar strip for quick comparison
- Average pressure readout with status badge
- Manual motor contract / retract / stop controls
- WebSocket connection to ESP32 (auto-falls back to demo mode)

---

## Quick Start

### 1. Web App

```bash
npm install
npm run dev
```

Open `http://localhost:5173/SCS/` in your browser (phone or desktop).

### 2. Firmware

Open `firmware/SmartSleeve.ino` in the Arduino IDE.

**Required libraries** (install via Library Manager):
- `ESPAsyncWebServer` by Me-No-Dev
- `AsyncTCP` by Me-No-Dev

Update your credentials at the top of the file:

```cpp
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
```

Flash to your ESP32. Open Serial Monitor at **115200 baud** — the IP address will be printed on boot.

### 3. Connect

Enter the ESP32's IP in the web app's Connect field (e.g. `192.168.1.42`) and press **Connect**.

---

## WebSocket Protocol

| Direction       | Format                                              |
|-----------------|-----------------------------------------------------|
| ESP32 → client  | `{"s":[12.3,15.1,14.8,13.9],"avg":14.0,"st":"Target Met"}` every 500 ms |
| Client → ESP32  | `contract` · `retract` · `stop`                    |

Manual commands pause auto-control for 10 seconds, then the PID loop resumes.

---

## Project Structure

```
SCS/
├── src/
│   ├── App.jsx                  # Root component, tab routing
│   ├── App.css                  # All styles (dark theme)
│   ├── main.jsx                 # React entry point
│   ├── components/
│   │   ├── PressureGauge.jsx    # SVG arc gauge
│   │   ├── GaugesPanel.jsx      # 4-gauge grid + average + bar strip
│   │   └── MotorPanel.jsx       # Motor controls + hardware map
│   ├── hooks/
│   │   └── useWebSocket.js      # WS connection + demo mode
│   └── utils/
│       └── gaugeUtils.js        # Arc math, constants
├── firmware/
│   └── SmartSleeve.ino          # ESP32 firmware
├── index.html
├── vite.config.js
└── package.json
```

---

## Deploy to GitHub Pages

```bash
npm run build
# Push the dist/ folder to your gh-pages branch, or use:
npx gh-pages -d dist
```

The `base: '/SCS/'` in `vite.config.js` matches the repo name for GitHub Pages routing.

---

## Hardware Pinout

| Signal | GPIO |
|--------|------|
| AIN1   | 25   |
| AIN2   | 26   |
| SLP    | 27   |
| FSR 1  | 34   |
| FSR 2  | 35   |
| FSR 3  | 32   |
| FSR 4  | 33   |

Pressure mapping: raw ADC (0–4095) → 0–50 mmHg using a 4.7 kΩ pull-down.

Target range: **20–40 mmHg** — below triggers tightening (red warning), above triggers releasing (blue warning).
