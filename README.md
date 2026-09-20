# XIAO Computer Pet

A small animated **Desk Buddy + Pomodoro companion** built around the Seeed Studio XIAO nRF52840, a 128×64 I²C OLED, a passive buzzer, two side touch sensors, an MPU6050 and a BMP180.

The pet is intentionally non-demanding: it reacts to you and its environment, but it never creates an attention debt or interrupts an active focus session.

## Features

- 13 modular pet skins with shared emotions and motion physics.
- Skin-specific behaviour profiles: idle pace, curiosity, energy and micro-sleep timing.
- MPU6050 gravity, tilt, shake, fall and cartoon spring/inertia reactions.
- Automatic neutral-position learning after 10 seconds of stillness after boot.
- BMP180 temperature and pressure telemetry.
- Top-button petting with heart eyes and heart particles.
- Side-touch gestures: look, scratch, swipe and hug.
- Speech bubbles with 1–2 word messages, multiple phrase packs, per-context rarity and a global anti-spam cooldown.
- Sleep schedule plus optional non-intrusive micro-sleep.
- Pomodoro modes: Focus, Short Break and Long Break.
- Pomodoro companion layouts:
  - pet above + timer below;
  - pet left + timer right;
  - classic timer-only mode by disabling Focus Companion.
- BLE Web configurator using Web Bluetooth.
- Configuration presets and JSON import/export.
- Persistent XP, streaks, interactions and 7-day focus history.
- Runtime diagnostics for loop rate, render rate and skipped OLED frames.
- OLED frame deduplication to avoid unnecessary full I²C transfers.
- GitHub Actions for firmware builds and GitHub Pages deployment.

## Hardware

| Part | Connection |
| --- | --- |
| Seeed XIAO nRF52840 | Main MCU |
| 1.3" 128×64 I²C OLED | SDA `D4`, SCL `D5`, default `0x3C` |
| Top push button | `D1` to GND, internal pull-up |
| Passive buzzer | `D2` |
| Left touch sensor | `D3` |
| Right touch sensor | `D6` |
| MPU6050 | Shared I²C, `0x68` or `0x69` |
| BMP180 | Shared I²C, `0x77` |

The default OLED driver is SH1106. If your module is SSD1306, set `OLED_USE_SH1106` to `0` in `include/ProjectConfig.h`.

## Controls

### Pet screen

- **Top short press:** pet the companion.
- **Top double click:** open the OLED menu.
- **Top long press:** open Pomodoro selection.
- **Left / right touch:** contextual Desk Buddy reactions.
- **Hold a side touch:** scratch that side.
- **Swipe left ↔ right:** pet gesture.
- **Touch both sides:** hug.

### OLED menus

- **Left touch:** previous item.
- **Right touch:** next item.
- **Top short press:** select / validate.
- **Top long press:** back.
- **SAVE & BACK:** persist changes made in the current submenu.

### Pomodoro ready screen

- **Left / right touch:** cycle Focus → Short Break → Long Break.
- **Top short press:** start the selected timer.
- **Top long press:** return to the pet.

### Running Pomodoro

- **Top short press:** pause / resume.
- **Top long press:** cancel and return to the pet.
- Focus sessions intentionally suppress autonomous attention-seeking behaviour.

## Firmware

### Requirements

- VS Code + PlatformIO, or PlatformIO Core.
- A XIAO nRF52840 connected by USB.

### Build

```bash
pio run -e seeed_xiao_nrf52840
```

### Upload

```bash
pio run -e seeed_xiao_nrf52840 -t upload
```

### Serial monitor

```bash
pio device monitor -b 115200
```

## Web BLE Configurator

The configurator is a static site in `web/`; there is no Node.js build step and no server-side component.

For local development:

```bash
cd web
python3 -m http.server 8080
```

Open:

```text
http://localhost:8080
```

Use desktop Chrome or Chromium. Web Bluetooth requires a secure context; `localhost` is allowed and GitHub Pages is served over HTTPS.

The configurator includes:

- Pomodoro durations and timer layout.
- Skin/accessory selection.
- Desk Buddy, touch, motion and sleep settings.
- Animation tuning.
- Calm / Expressive / Cartoon / Minimal / Focus presets.
- Speech packs and custom two-word greeting/petting responses.
- Audio preview directly on the XIAO buzzer.
- MPU calibration and live sensor telemetry.
- 7-day focus history.
- JSON configuration import/export.
- Runtime diagnostics.

## GitHub Pages

The workflow `.github/workflows/pages.yml` publishes the contents of `web/`.

After pushing the repository:

1. Open **GitHub → Repository Settings → Pages**.
2. Set **Source** to **GitHub Actions** if it is not already selected.
3. Run the `Deploy Web Configurator` workflow or push a change under `web/`.

For the repository `maelremrem/XiaoComputerPet`, the expected URL is:

```text
https://maelremrem.github.io/XiaoComputerPet/
```

GitHub Pages availability for a private repository depends on the GitHub plan/account settings.

## Continuous Integration

### Firmware

`.github/workflows/firmware.yml` runs PlatformIO on GitHub-hosted Ubuntu runners for firmware changes and pull requests.

Artifacts contain:

- `firmware.hex` renamed with the commit SHA;
- `firmware.elf`;
- `firmware.bin` when produced by the board platform;
- `build-info.txt`.

This workflow is also the reference build when the local development environment does not have the full nRF52840 toolchain installed.

### Web

The Pages workflow uploads `web/` directly and deploys it without a frontend build step.

## Architecture

```text
Button / Touch / MPU6050 / BMP180
                │
                ▼
         input + sensor state
                │
                ▼
          BehaviorEngine
      priority / queue / timing
                │
                ▼
       Emotion + Motion state
                │
                ▼
       modular Skin renderer
                │
                ▼
          128×64 OLED
```

Behaviour priority is designed so critical physical events and explicit user interactions win over ambient animation. Gravity remains a continuous visual layer, while low-priority autonomous events can wait or be discarded.

The OLED renderer hashes the 1-bit framebuffer before presentation. Identical frames are skipped, avoiding unnecessary full-buffer I²C transfers while keeping SH1106 and SSD1306 support on the same rendering path.

## Configuration Storage

Settings and persistent pet state are stored in the XIAO internal LittleFS filesystem.

- `/pet_config.json` — user configuration.
- `/pet_state.json` — XP, stats, streak and history.

Pet-state writes are deferred during interactive animation to avoid visible pauses. User configuration is persisted only on explicit save operations.

## Project Structure

```text
.
├── .github/workflows/     GitHub Pages + PlatformIO CI
├── docs/                  Design and behaviour documentation
├── include/               Firmware headers
├── src/                   Firmware implementation
├── web/                   Static Web Bluetooth configurator
├── platformio.ini
└── README.md
```

## Motion Calibration

At boot, static tilt is ignored until the MPU6050 has remained still for 10 consecutive seconds. That position becomes the session neutral position.

If the MPU is physically rotated relative to the OLED:

1. Set **MPU mount rotation** in the Web configurator.
2. Save to the pet.
3. Put the pet in its normal desk position.
4. Leave it still for 10 seconds, or use **Calibrate MPU**.

## Troubleshooting

### Web Bluetooth is unavailable

- Use desktop Chrome/Chromium.
- Open the GitHub Pages HTTPS URL or `http://localhost:8080`.
- Do not run the configurator in an embedded preview/webview.
- On some Linux Chromium builds, Web Bluetooth may require experimental web-platform features.

### OLED is blank

- Verify `0x3C` on the I²C bus.
- Check SDA/SCL wiring.
- Switch `OLED_USE_SH1106` if the panel is actually SSD1306.

### Motion directions are wrong

Change the MPU mount rotation in the Web configurator, save, then recalibrate the neutral position.

## Development

The firmware intentionally keeps hardware polling, behaviour selection and rendering separate. New skins should reuse the shared emotion/motion system instead of embedding application behaviour in the drawing code.

See the files in `docs/` for the Desk Buddy, motion, speech and skin design notes.

## License

No license has been selected yet. Add an explicit license before distributing or accepting external contributions.
