# XIAO Computer Pet v4.7

Minimal computer pet + Pomodoro timer for a Seeed Studio XIAO nRF52840 and a 128x64 I2C OLED.

The pet deliberately uses only **two large rounded-square eyes and two thick rounded arc eyebrows**. There is no mouth, nose, ears or name under the face. The face fills almost the entire OLED and expressions are created by moving/resizing the eyes and changing eyebrow angles.


## v4.7 non-intrusive Desk Buddy

- The pet never asks for attention and never penalizes the user for leaving it alone.
- Removed boredom-driven behavior, boredom speech and the visible boredom meter.
- Mood and affection no longer decay from inactivity; only energy changes passively while the pet is on its normal screen.
- Pomodoro, menus and timer selection freeze passive need drift completely.
- Ambient idle animations remain available but are silent: no speech bubble follows a yawn, sneeze, hiccup, dream or dance.
- Removed attention-seeking copy such as `Again Please`, `Hey You`, `Still Here?`, `Play Maybe` and `Watch Me`.
- Speech remains for greetings, explicit pet interactions, physical pickup/shake/settle reactions, sleep/wake and timer completion.
- OLED/Web vitals now show only Mood, Energy and Affection.


## v4.6 readable speech + fall reaction

- Speech bubbles now type quickly, then stay visible long enough to read comfortably.
- Added real drop detection using a sustained low-g phase instead of treating every shake as a fall.
- During a detected fall the two eyes become thick `X` shapes immediately.
- A following landing impact extends the X-eye recovery beat for about a second.
- Fall reactions have priority over shake, settle, orientation, sleep `zZZ` and speech bubbles.


## v4.5 speech + gravity polish

- Gravity/orientation animation has priority over low-priority idle emotions and reacts faster to real device tilt.
- Scheduled sleep now draws animated `zZZ` around the pet.
- A new modular speech-bubble system temporarily zooms the pet out, opens a bubble and types short text progressively.
- Speech is intentionally limited to **two words maximum**.
- Example/contextual lines include `Hello!`, `Good Again!`, `Happy Happy`, `Nice Work`, `Good Night`, `Whoa Whoa`, `Achoo!`, and many variants.
- Boot greeting appears after about 15 seconds of awake idle time. Petting queues an affectionate text after the heart animation.
- Pickup, settle, shake, rare idle events, focus completion, sleep and wake can also generate contextual text.
- Web BLE adds a Speech bubbles switch and a contextual text probability slider.

## v4.4 Desk Buddy life system

This release expands the software-only personality layer without requiring extra hardware:

- legacy v4.4 introduced boredom; v4.7 deprecates it so absence never creates attention debt;
- advanced side-touch gestures: hold left/right = scratch, sweep side-to-side = pet, both sides = hug;
- configurable rare idle events: yawn, sneeze, hiccup, dream and mini-dance;
- a tiny focus-companion face evolves during Pomodoro sessions and becomes alert for the last minute;
- Web BLE exposes animation tuning for idle speed, eye-follow strength, cartoon inertia, squash/stretch and heart-particle count;
- rare-event timing, advanced touch gestures and the focus companion can all be disabled independently;
- focus XP, hugs, scratches, swipes and rare-event counters are persisted and visible in the Web BLE stats;
- OLED vitals now use `M / E / A`; boredom is deprecated in v4.7.

All of these features use the existing modular **Skin + Emotion + Motion** renderer, so they work with all 13 pet skins.


## v4.2 motion rewrite

The MPU6050 motion path was rebuilt to keep **orientation** and **movement** independent:

- gravity is estimated with an adaptive low-pass filter and is not allowed to absorb strong shakes;
- tilt/gaze is computed from the filtered gravity vector, never from raw acceleration;
- linear acceleration is calculated after gravity removal and drives one damped head spring;
- no derived jerk is added on top of the movement signal;
- pickup, shake and settle events use persistence + hysteresis to reject desk vibration;
- shake motion is physical first; `Dizzy` is only shown after a strong shake settles;
- orientation-triggered moods require ~750 ms of stable orientation;
- old spring state is cleared after menus/timers so the face cannot resume stale wobble;
- Web BLE adds MPU mount rotation (0/90/180/270°), motion sensitivity and raw motion/gravity telemetry.

Recommended setup: choose the MPU mount rotation, **Save to pet**, place the pet in its normal resting position, then press **Calibrate MPU**. Start at 100% motion sensitivity.

## v3.8 display rotation

The OLED can be flipped 180° from **OPTIONS → SCREEN** or the Web BLE Display panel. The setting is stored in flash and applies to the entire UI.

## Hardware

- Seeed Studio XIAO nRF52840
- 1.3" 128x64 I2C OLED, default configuration: SH1106 at `0x3C`
- passive buzzer on `D2`
- push button between `D1` and GND
- left digital touch sensor on `D3`
- right digital touch sensor on `D6`
- MPU6050 on the shared I2C bus
- BMP180 on the shared I2C bus

The push button uses the internal pull-up resistor. The side touch inputs are configured for active-HIGH digital modules (for example TTP223). If your modules are active LOW, change `TOUCH_ACTIVE_HIGH` in `include/ProjectConfig.h`.


### OLED wiring

| OLED | XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | SDA |
| SCL | SCL |

If your 1.3" module is SSD1306 instead of SH1106, set `OLED_USE_SH1106` to `0` in `include/ProjectConfig.h`.

### Added controls and sensors

| Module | XIAO |
|---|---|
| Top button | D1 → button → GND |
| Buzzer signal | D2 |
| Left touch OUT | D3 |
| Right touch OUT | D6 |
| MPU6050 SDA | SDA / D4 |
| MPU6050 SCL | SCL / D5 |
| BMP180 SDA | SDA / D4 |
| BMP180 SCL | SCL / D5 |

Power the touch modules and I2C breakouts from **3V3** so their logic levels remain safe for the nRF52840 GPIO.

## Controls

### Pet screen

- short press: pet / affection interaction
- double click: open the on-device pet menu
- long press: start a Pomodoro focus session

### Pet menu

Primary controls with the two side touch sensors:

- right touch (`D6`): next item/value
- left touch (`D3`): previous item/value
- short press on the top button (`D1`): select / validate
- long press on the top button (`D1`): back / cancel unsaved submenu changes

Menu navigation is now deliberately split between controls: side touch = previous/next, short top-button press = select, long top-button press = back.

The root carousel contains `STATUS`, `ACCESSORY`, `STYLE`, `OPTIONS` and `EXIT`. `OPTIONS` now also contains `DESK BUDDY`, alongside sound, sleep, pet SFX, focus-complete melody and break-complete melody. Every submenu still ends with `SAVE & BACK`.

### Pomodoro

- short/double press: pause/resume
- long press: cancel and return to the pet
- any press while the completion melody is playing: stop it immediately


## Desk Buddy mode

Desk Buddy is enabled by default and is treated as the normal operating mode of the pet.

- The **MPU6050** drives subtle eye parallax from device tilt.
- Picking the unit up makes the pet react with surprise/curiosity.
- Setting it back down gives a short positive reaction.
- A strong shake triggers a temporary dizzy expression.
- Touching the **left** or **right** side makes the pet look toward that side.
- Holding one side gives a scratch reaction on that side.
- Sweeping from one side to the other becomes a lateral petting gesture.
- Holding both sides together gives a hug reaction.
- The **BMP180** provides ambient temperature and pressure. Temperature extremes can subtly bias spontaneous expressions when environment reactions are enabled.

Desk Buddy, touch reactions, motion reactions and environment reactions can all be enabled/disabled independently from the BLE page. The main Desk Buddy switch is also available directly in the OLED `OPTIONS` menu.

The Web Bluetooth page exposes a sensor diagnostic panel showing MPU/BMP detection, computed gaze direction, motion level, temperature and pressure.

### I2C addresses

The firmware probes both common addresses automatically:

- MPU6050: `0x68`, then `0x69`
- BMP180: fixed `0x77`
- OLED: `0x3C`

The OLED, MPU6050 and BMP180 share the same SDA/SCL bus.

### MPU orientation tuning

If eye movement is reversed or rotated because of how your MPU6050 is mounted, edit these values in `include/ProjectConfig.h`:

```cpp
constexpr bool MPU_SWAP_GAZE_AXES = false;
constexpr float MPU_GAZE_X_SIGN = 1.0f;
constexpr float MPU_GAZE_Y_SIGN = 1.0f;
```

Set a sign to `-1.0f` to invert that axis, or enable axis swapping if the breakout is rotated by 90 degrees.

## Pet system

Persistent state is stored in internal LittleFS:

- mood: 0-100
- energy: 0-100
- affection: 0-100
- boredom: deprecated compatibility field, always neutral in v4.7
- XP and level
- total pets and boops
- total focus sessions, focus minutes and focus XP
- hug, scratch, swipe and rare-event counters
- daily focus stats
- daily streak

Interactions improve mood/affection and give a small amount of XP. Completing focus sessions gives much more XP, improves mood and affection, and consumes some energy. Breaks restore energy.

### Unlocks

- Level 2: Spark
- Level 4: Orbit
- Level 7: Crown

Accessories are drawn around the face; the facial design itself remains eyes + eyebrows only.

## On-device menu

The menu is designed for the 128x64 OLED as an animated horizontal carousel. The active card is wider, while a thin slice of the previous and next cards remains visible at the left/right edges. Cards slide with eased transitions and page dots show position. Holding the button draws a second rounded outline progressively around the selected card before returning to the previous menu. Stats use dedicated vitals, focus and progression pages.

Accessories that are not yet unlocked are visible but marked `LOCKED`; selecting a locked item does not equip it. Changes are kept in RAM until `SAVE & BACK` is selected.

## Pet personalities

The BLE page can switch between thirteen monochrome graphical personalities:

- Soft
- Robot
- Compact
- Wide
- Arcade
- Alien
- Cat
- Visor
- Core
- Sleepbot
- Scout
- Bubble
- Mask

They change eye proportions, spacing, corner radius and eyebrow weight without changing the basic design language.

## Sleep mode

Sleep scheduling is optional and disabled by default.

When enabled, Chrome sends its current local time and timezone offset to the XIAO when the BLE configuration page connects. The pet then uses the configured sleep/wake hours to display a sleepy face and slowly recover energy.

The nRF52840 has no battery-backed wall clock in this project, so after a full power loss the schedule becomes active again after the next BLE connection/time sync.

## Pomodoro

Configurable from BLE:

- focus duration
- short break
- long break
- long-break interval
- automatic break start

The OLED Pomodoro UI is intentionally minimal: mode, large remaining time, optional tiny focus-companion eyes and progress bar. When the timer ticks, only the digits that change slide vertically and fade using 1-bit ordered dithering, while unchanged digits remain fixed.

## Melodies

The BLE interface offers multiple presets for:

- pet interaction sound
- focus-complete melody
- break-complete melody

The buzzer player is non-blocking, so animations and BLE continue to run while a melody plays.

## BLE configuration page

The XIAO nRF52840 has BLE but no Wi-Fi, so configuration uses Web Bluetooth in Chrome/Chromium.

From the project directory:

```bash
cd web
python3 -m http.server 8080
```

Open:

```text
http://localhost:8080
```

Then click **Connect BLE** and select `XIAO Computer Pet`.

The page can configure Pomodoro, animation timing, Desk Buddy life/gesture settings, animation tuning, sleep hours, personality, accessories and sound presets. Double click is reserved for the on-device pet menu. It also shows live persistent stats, XP, level and unlock state.

Web Bluetooth requires Chrome/Chromium and a secure context; `localhost` is accepted.

## Build with PlatformIO

Open this folder in VS Code with PlatformIO and build/upload the `seeed_xiao_nrf52840` environment.

```ini
[env:seeed_xiao_nrf52840]
platform = https://github.com/Seeed-Studio/platform-seeedboards.git
board = seeed-xiao-afruitnrf52-nrf52840
framework = arduino
```

## Main files

```text
include/
  ProjectConfig.h
  Settings.h
  PetState.h
  ButtonInput.h
  SideTouchInput.h
  SensorHub.h
  Buzzer.h
  DisplayUI.h
  PomodoroTimer.h
  BleConfigService.h
  SettingsStore.h
  PetStateStore.h
src/
  main.cpp
  ButtonInput.cpp
  SideTouchInput.cpp
  SensorHub.cpp
  Buzzer.cpp
  DisplayUI.cpp
  PomodoroTimer.cpp
  BleConfigService.cpp
  SettingsStore.cpp
  PetStateStore.cpp
web/
  index.html
  app.js
  style.css
```

## Display performance

The animation loop is configurable from 10 to 60 FPS and defaults to 40 FPS. With a 128x64 I2C display, actual visible refresh rate is mainly limited by the OLED controller and I2C transfer speed rather than the nRF52840 CPU. A SPI OLED remains the best path if a strict 60 FPS display refresh is required.


## Audio preview over BLE

The web configurator can preview every pet SFX, focus-complete melody and break-complete melody directly on the XIAO buzzer. Preview does not save the selected option and can be stopped immediately with **Stop preview**. Explicit preview also works while the saved sound toggle is disabled.

## Web Bluetooth troubleshooting

Start the configurator from the `web` folder with:

```bash
./serve.sh
```

Then open **http://localhost:8080** directly in Chrome/Chromium. Do not use a plain `http://192.168.x.x:8080` LAN address: Web Bluetooth requires a secure context. The UI now detects missing Web Bluetooth support and displays a diagnostic instead of throwing a JavaScript exception. On Linux, if `navigator.bluetooth` is still absent in Chrome/Chromium, enable `chrome://flags/#enable-experimental-web-platform-features` and relaunch the browser.

## v3.2 interaction polish

- Short pet interaction animates both eyes into pulsing hearts, then returns to the normal face.
- Eyebrows are rendered as thick smooth arcs with round caps.
- Pomodoro digits use per-digit vertical slide + 1-bit fade transitions.
- OLED menu cards are wider and expose neighbouring-card peeks on both sides.
- Holding in the OLED menu draws a progressive rounded confirmation outline before validating.
- Web BLE now guards missing `navigator.bluetooth` and shows troubleshooting instead of throwing `requestDevice` errors.


## V3.3 interaction + BLE responsiveness

- Adjacent OLED menu cards no longer render text; only the central card shows labels/details.
- A short press starts pet feedback immediately; pet stats are committed only after the double-click window expires.
- Pet-state flash writes are deferred so the heart animation does not hitch.
- Pressing the physical button squashes the face downward, followed by a springy heart-eye rebound.
- Web Bluetooth chooser cancellation is treated as a normal cancellation rather than a console error.
- Save-to-pet waits for the firmware save acknowledgement before sending follow-up commands.
- Audio preview uses one-packet BLE commands plus maximum peripheral bandwidth for lower latency.

### OLED menu navigation (v3.4)
Every submenu ends with a **SAVE & BACK** card. Navigate with the left/right touch sensors, use a short top-button press to select or validate, and use **SAVE & BACK** to persist changes. A long top-button press goes back without saving the current submenu edits.


## v3.5 Desk Buddy + sensors

- Added left/right digital touch inputs on D3/D6.
- Side touches are now the primary next/previous controls in OLED menus.
- Added MPU6050 auto-detection at 0x68/0x69.
- Added BMP180 support at its fixed 0x77 address.
- Added tilt-driven gaze, pickup/settle reactions and shake/dizzy reaction.
- Added side-touch gaze/reactions on the pet screen.
- Added Desk Buddy enable/disable submenu with SAVE & BACK.
- Added BLE controls for Desk Buddy/touch/motion/environment reactions.
- Added sensor telemetry panel to the Web Bluetooth configurator.


## V3.8 additions
- Swap D3/D6 logical left/right from OLED `OPTIONS > TOUCH SIDE` or BLE.
- Calibrate the MPU6050 neutral desk position from `OPTIONS > MPU CAL` or the BLE Sensors panel.
- During Pomodoro, BMP180 temperature and pressure appear compactly at the top; the session `#` counter is removed.


## Pomodoro ready screen (v3.8)
Entering Pomodoro now opens a ready screen instead of starting immediately. Use the side touch sensors to cycle `FOCUS`, `SHORT`, and `LONG`; press the top button to start. A short start melody confirms launch and can be previewed from the Web BLE Sound card. Long-press the top button while still on the ready screen to return to the pet. Menu navigation and Pomodoro mode changes emit a short UI tick when sound is enabled.


## Modular pet visuals (v3.9)

The pet renderer is split into three independent layers: **Skin**, **Emotion**, and **Motion**. `PetVisual.h/.cpp` owns the 13 skin definitions, shared emotion profiles, and gravity/inertia pose calculation. `DisplayUI` only renders the resolved result. This makes it possible to add a new pet design without duplicating every emotion or MPU reaction.

When Desk Buddy motion reactions are enabled, a strong side tilt makes the eyes fall toward that side, move closer together and compress against the edge. Orientation changes and fast rotations also trigger short reactions while the continuous gravity deformation remains active.

## v4.1 BMP180, idle dim and cartoon motion

- Hardware ambient sensor corrected to **BMP180** at fixed address `0x77`; it provides temperature and pressure, not humidity.
- Pomodoro header now shows temperature + pressure when the BMP180 is available.
- OLED auto-dims to minimum contrast after physical-control inactivity and wakes instantly on the top button or either side touch.
- `Auto-dim after` is configurable from the Web BLE Display card; `0` disables dimming.
- MPU shake rendering now uses jerk-driven cartoon head inertia with follow-through, overshoot, squash/stretch and smooth settling across all skins.
- Affection heart eyes were redrawn and now emit animated heart particles around the face.

### v4.0 interaction and motion update
- Side touch sensors are the only previous/next controls inside OLED menus.
- Short top-button press selects/validates; long press goes back.
- Long-back discards unsaved changes from the current submenu; `SAVE & BACK` persists them.
- Pomodoro READY time is truly centered and mode changes use directional slide + dithered fade.
- The READY button pulse moved to the top of the OLED, matching the physical top button.
- MPU shake response now uses directional acceleration plus an under-damped spring: the whole face lags, overshoots and settles naturally, with a small gyro-driven head tilt.


## Automatic MPU rest zero
After boot, keep the pet still for 10 continuous seconds. Any movement restarts the timer. The learned neutral orientation is session-only and is not written to flash.
