# v4.7
- Made Desk Buddy behavior explicitly non-intrusive: no attention requests or absence penalties.
- Deprecated boredom-driven behavior and removed boredom from OLED/Web vitals.
- Mood and affection no longer decay because the pet is ignored.
- Passive energy drift is frozen outside the normal pet screen, including Pomodoro.
- Rare/ambient idle animations remain visual but no longer create speech bubbles.
- Removed needy/attention-seeking speech variants while preserving greetings and reactive feedback.

## v4.6
- Speech bubbles now type quickly and remain readable on screen for roughly five to six seconds.
- Added low-g drop detection independent from normal shake detection.
- A detected drop immediately switches both eyes to thick X shapes.
- A landing impact retriggers/extends the knocked-out X-eye reaction for a short recovery beat.
- Fall reactions interrupt speech so the physical event always wins visually.

# Changelog

## 4.5
- Added modular speech-bubble events with a zoomed-out pet, animated bubble pop and typewriter text.
- Speech copy is limited to one or two words and varies by context: boot, petting, pickup, settle, shake, idle events, focus completion, sleep and wake.
- Added `Speech bubbles` and contextual text chance controls to the Web BLE configurator.
- Added animated `zZZ` while the scheduled sleep state is active.
- Increased gravity/orientation priority over low-priority idle emotions.
- Made gravity response faster by reducing gravity/gaze filter latency and lowering side-orientation activation thresholds.
- Side-touch gaze no longer overrides a strong physical gravity pose.
- Cleaned duplicate draw calls/checks and several unused-variable warning sources in the OLED renderer.

## 4.4
- Added persistent boredom and expanded pet memory/state statistics.
- Added advanced touch gestures: side hold scratches, side-to-side sweep pets, dual-touch hugs.
- Added modular rare idle events: yawn, sneeze, hiccup, dream and mini-dance.
- Added configurable rare-event interval and independent enable/disable toggles.
- Added an optional Pomodoro focus companion whose eyes evolve with session progress and wake up for the final minute.
- Added Web BLE animation tuning for idle speed, eye-follow strength, cartoon inertia, squash/stretch and heart-particle count.
- Added focus XP, hugs, scratches, swipes and rare-event counters to persistent state and Web BLE stats.
- OLED vitals now include boredom as the fourth meter.
- All new emotions remain skin-independent and use the shared Skin + Emotion + Motion renderer.


## 4.3
- Added automatic session rest-zero calibration after 10 continuous seconds of stillness after boot.
- Any detected motion resets the 10-second calibration window.
- Automatic boot calibration is kept in RAM and does not write to flash.
- Manual MPU calibration remains available and overrides the automatic rest zero for the current session.
- Added Web BLE telemetry for boot rest calibration progress/status.

## v4.2
- Rebuilt MPU6050 motion processing around separate stable-gravity and dynamic-acceleration signals.
- Tilt no longer reads raw acceleration, so shakes do not masquerade as orientation changes.
- Replaced moving-target + jerk wobble with one force-driven damped cartoon head spring.
- Added adaptive gravity trust during acceleration/rotation.
- Added motion event hysteresis/persistence for pickup, shake and settle.
- Strong shake uses physical motion while active and only shows Dizzy after settling.
- Orientation mood reactions require a stable orientation before firing.
- Added runtime MPU mount rotation (0/90/180/270°) and motion sensitivity in Web BLE.
- Added gravity and linear-acceleration telemetry to the Sensors panel.
- Reset stale spring state after menu/timer interruptions.
- Rounded rendered motion coordinates for smoother pixel movement.

# v4.1

- Replaced BME280 support with BMP180 (temperature + pressure, fixed I2C address 0x77).
- Added configurable OLED auto-dim after physical button/touch inactivity; contrast drops to minimum and wakes immediately on input.
- Reworked MPU6050 shake physics with jerk-driven cartoon inertia, stronger follow-through, overshoot and smooth settling shared by all skins.
- Rebuilt heart eyes from a clean mathematical heart shape and added animated heart particles around the face during petting.
- Pomodoro ambient header now shows BMP180 temperature + pressure instead of unavailable humidity.

## 4.0
- Reworked OLED menu controls: side touch = previous/next, short top-button press = select/validate, long press = back.
- Long-back restores the settings snapshot from when the submenu was entered; `SAVE & BACK` remains the explicit persistence action.
- Centered Pomodoro `MM:SS` using its real five-glyph width.
- Added directional slide + dithered fade when switching FOCUS / SHORT / LONG from the side touch sensors.
- Moved the Pomodoro READY button pulse from the bottom to the top of the OLED.
- Replaced canned shake wobble with directional MPU6050 inertial spring motion, overshoot, damping and gyro-driven cartoon head tilt.
- Shake/movement physics remain skin-independent and therefore apply to all 13 pet designs.

# v3.9

- Added 9 new pet skins: Arcade, Alien, Cat, Visor, Core, Sleepbot, Scout, Bubble and Mask (13 total).
- Refactored pet visuals into modular Skin + Emotion + Motion layers in `PetVisual.h/.cpp`.
- Added gravity-based side resting: eyes fall, cluster and squash toward the lower side.
- Added continuous MPU inertia/wobble and orientation-change reactions.
- Added fast-rotation reaction and face-down reaction.
- Expanded OLED STYLE carousel and BLE web selector to all 13 skins.

# Changelog

## 3.8
- Added a Pomodoro ready screen: entering timer mode no longer starts the countdown immediately.
- Side touch left/right cycles `FOCUS -> SHORT -> LONG` before start.
- Top button starts the selected timer with a new short start melody.
- Long press on the ready screen returns to the pet.
- Added a short UI tick for every OLED menu-card change and timer-mode change.
- Added Web BLE preview for the new timer-start melody.
- Active timer now keeps the selected `FOCUS`, `SHORT`, or `LONG` label.
- Auto-break now opens the appropriate break ready screen and still requires a button press before countdown.

## 3.7
- Added persistent left/right touch swap option (OLED + BLE).
- Added MPU6050 neutral-position calibration and reset (OLED + BLE).
- MPU gaze calibration offsets persist in flash.
- Pomodoro header now shows BME280 temperature and humidity when available.
- Removed Pomodoro session # / number from the timer screen.

## v3.5
- Added D3 left and D6 right digital touch sensors.
- Side touch is now primary previous/next navigation in OLED menus while button navigation remains as fallback.
- Added MPU6050 support with automatic 0x68/0x69 detection.
- Added BME280 support with automatic 0x76/0x77 detection.
- Added Desk Buddy behavior: tilt gaze, pickup, settle and shake reactions.
- Added side-touch gaze and two-side happy reaction on the pet screen.
- Added optional environment-driven spontaneous reactions.
- Added Desk Buddy OLED submenu with SAVE & BACK.
- Added BLE toggles and sensor telemetry.
- Added configurable MPU gaze axis swap/signs in ProjectConfig.h.


## v3.3
- Removed text from adjacent carousel cards.
- Added immediate tap feedback while retaining double-click menu detection.
- Added organic button squash/rebound animation.
- Deferred pet-state flash writes to avoid animation stalls.
- Improved BLE bandwidth/connection interval and buffered TX.
- Added compact one-packet audio-preview commands.
- Web UI now treats chooser cancellation as normal and waits for save acknowledgement.


## v3.2
- Fixed Web UI crash when `navigator.bluetooth` is unavailable.
- Added secure-context / embedded-page / browser diagnostics and Linux guidance.
- Added `web/serve.sh` for a localhost BLE-safe launch.
- Pomodoro now animates only changing digits with vertical slide + simulated 1-bit fade.
- Pet interaction morphs both eyes into animated pulsing hearts.
- Eyebrows are thicker smooth rounded arcs.
- OLED menu cards are wider and show neighbour-card peeks on both sides.
- Holding the button in the OLED menu draws a progressive rounded confirmation contour before validation.

## v3.1
- Added BLE web preview buttons for every pet sound, focus melody and break melody.
- Added immediate Stop Preview command.
- Audio preview can temporarily bypass the global sound toggle without changing saved settings.


## v3

- Double click on the pet now always opens the on-device pet menu.
- Added animated horizontal carousel navigation with eased slide transitions and pagination dots.
- Added root menu: Status, Accessory, Style, Options and Exit.
- Added three stat pages: vitals, focus, and progression/streak.
- Added on-device accessory selection with locked-state handling.
- Added on-device personality selection.
- Added Options submenu for sound, sleep, pet SFX, focus melody and break melody.
- Menu navigation is fixed to short press = next, double click = previous, long press = validate.
- Removed the obsolete configurable double-click action from BLE settings.
- Menu setting changes are persisted immediately to flash.

## v2

- Reworked pet renderer: no mouth, nose, ears or OLED name label.
- Increased face size to use nearly the entire 128x64 display.
- Added more eye/eyebrow-only expressions and animation states.
- Added mood, energy, affection, XP, levels and persistent statistics.
- Added unlockable Spark, Orbit and Crown peripheral accessories.
- Added selectable Soft, Robot, Compact and Wide graphical personalities.
- Added optional sleep schedule driven by BLE browser time synchronization.
- Added double-click recognition and configurable double-click behavior.
- Added multiple pet, focus-complete and break-complete buzzer presets.
- Expanded BLE/Web UI with progression, stats and all new settings.


## v3.4
- Added a permanent `SAVE & BACK` card to every OLED submenu.
- Submenu selections now stay in the current carousel instead of immediately returning.
- Settings are persisted when `SAVE & BACK` is validated with a long press.
- Leaf submenus return to `OPTIONS`; top-level submenus return to the root pet menu.
- Added a dedicated save/back icon compatible with the compact carousel layout.
