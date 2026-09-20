# MPU6050 motion setup

The motion system uses two independent signals:

- **Gravity / gaze**: slow, stable orientation used to make the eyes fall toward a side.
- **Linear motion**: gravity-removed acceleration used for the cartoon head inertia.

## Setup

1. Open the BLE configurator.
2. In **Desk Buddy → MPU mount rotation**, select 0°, 90°, 180° or 270° so physical left/right/up/down match the screen.
3. Save. Changing mount rotation clears the previous neutral offset.
4. Put the unit still in its normal desk position.
5. Press **Calibrate MPU**.
6. Start with **Motion sensitivity = 100%**. Lower it if the face travels too far; raise it if the movement feels too subtle.

## Telemetry

- `Gaze X / Y` should remain near 0 / 0 in the calibrated resting position.
- `Linear X / Y` should return near 0 / 0 when the unit is still.
- `Motion` should settle close to 0%.
- `Gravity X / Y / Z` changes slowly with orientation and should not jump violently during a quick shake.

The motion renderer is shared by all pet skins.

## Boot rest zero

At startup the MPU motion engine waits for **10 continuous seconds of genuine stillness**. Any detected movement resets the timer. Once the window completes, the current screen-space gravity X/Y becomes a session-only neutral offset. This does not write to flash.

While the rest zero is still pending, static gaze tilt is held at the center so accelerometer bias or a slightly tilted enclosure cannot make the pet visibly lean. Dynamic shake/inertial motion remains active. Manual `Calibrate MPU` still sets a persistent user zero; `Reset` disables session auto-zero until the next boot.
