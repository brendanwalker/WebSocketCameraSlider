# WebSocket Camera Slider

ESP32-based 3-axis camera slider (Pan / Tilt / Slide) controlled via a TypeScript web app over WiFi/WebSocket — no Bluetooth or desktop app required.

## Hardware

- SparkFun ESP32 Thing
- 3× stepper motors + drivers (Pan, Tilt, Slide)
- Hall effect sensors (pan center, tilt center, slide min/max)
- Rotary encoder with button
- SSD1306 OLED display (128×64, I2C)
- Cooling fan

## Project Structure

```
WebSocketCameraSlider/
├── ESP32Firmware/      # PlatformIO firmware project
│   ├── platformio.ini
│   ├── wifi_config.h.template
│   └── *.h / *.cpp / *.ino
└── WebApp/             # TypeScript + Vite web app
    ├── package.json
    ├── vite.config.ts
    └── src/
```

## First-Time Setup

### 1. WiFi setup (captive portal)

WiFi credentials are configured at runtime — no source file edits required.

**First boot:**

1. Power on the ESP32. The OLED will show:
   ```
   WiFi Setup Mode
   Connect to:
   CameraSlider-Setup
   192.168.4.1
   ```
2. On your phone or laptop, connect to the `CameraSlider-Setup` WiFi network.
3. A browser page should open automatically (captive portal). If it doesn't, navigate to `http://192.168.4.1`.
4. Enter your network SSID and password, then tap **Save & Connect**.
5. The ESP32 connects to your network and the OLED shows the assigned IP address.

Credentials are saved to flash (NVS) and used automatically on every subsequent boot.

**Reconfiguration:**

Hold the rotary encoder button while powering on. The device will re-enter AP mode and you can enter new credentials.

### 2. Install tooling

- **PlatformIO**: install the [PlatformIO IDE VSCode extension](https://platformio.org/install/ide?install=vscode)
  or install the CLI: `pip install platformio`
- **Node.js** (v18+): [nodejs.org](https://nodejs.org)

## Building & Flashing

All commands run from the `ESP32Firmware/` directory.

### Flash firmware

```sh
pio run -t upload
```

### Build and deploy the web app

```sh
cd WebApp
npm install        # first time only
npm run deploy     # builds TypeScript → ESP32Firmware/data/
```

Then flash the web app to ESP32 flash (LittleFS):

```sh
cd ESP32Firmware
pio run -t uploadfs
```

> **Note:** `uploadfs` and `upload` are separate operations. Flash the firmware first, then the filesystem.

### Serial monitor

```sh
pio device monitor
```

On first boot the ESP32 runs the WiFi captive portal (see First-Time Setup above). On subsequent boots it connects automatically and prints the IP address to serial.

## Development Workflow

To iterate on the web app without reflashing:

```sh
# Set the ESP32's IP and start the Vite dev server
cd WebApp
ESP32_IP=192.168.x.x npm run dev
```

Vite proxies `/ws` to the ESP32, so the browser at `http://localhost:5173` talks live to the device.

When ready to deploy:

```sh
npm run deploy
cd ../ESP32Firmware && pio run -t uploadfs
```

## WebSocket Protocol

The web app communicates with the ESP32 over a WebSocket at `ws://<esp32-ip>/ws`.

### Client → ESP32

```json
{ "cmd": "set_pos 0.5 0.0 -0.2", "id": "1" }
```

| Command | Description |
|---|---|
| `set_pos <slide> <pan> <tilt>` | Move to position (fractions, −1 to 1) |
| `stop` | Stop all motors immediately |
| `set_speed <0-1>` | Set speed fraction |
| `set_accel <0-1>` | Set acceleration fraction |
| `calibrate [pan] [tilt] [slide]` | Run calibration for specified axes |
| `reset_calibration` | Clear stored calibration |
| `get_slider_calibration` | Get slide min/max step counts |
| `get_motor_pan_limits` | Get pan speed/accel limits |
| `get_motor_tilt_limits` | Get tilt speed/accel limits |
| `get_motor_slide_limits` | Get slide speed/accel limits |
| `set_pan_motor_limits <minAng> <maxAng> <minSpd> <maxSpd> <minAcc> <maxAcc>` | Set pan limits |
| `set_tilt_motor_limits <minAng> <maxAng> <minSpd> <maxSpd> <minAcc> <maxAcc>` | Set tilt limits |
| `set_slide_motor_limits <minSpd> <maxSpd> <minAcc> <maxAcc>` | Set slide limits |
| `save` | Persist configuration to flash |
| `ping` | Connectivity check (returns `pong`) |

### ESP32 → Client

```json
{ "type": "position", "slider": 0.42, "pan": -0.1, "tilt": 0.05, "speed": 0.5, "accel": 0.5 }
{ "type": "status",   "value": "move_start" }
{ "type": "response", "id": "1", "data": "slider_calibration 0 32767" }
```

Position values are fractions in the range **−1 to 1**.
