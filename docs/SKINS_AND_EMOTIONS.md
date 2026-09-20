# Pet visual architecture

The OLED pet renderer is split into three independent layers.

## 1. Skin

`include/PetVisual.h` defines `PetSkinDefinition` and `PetEyeStyle`.
`src/PetVisual.cpp` contains the `SKINS` table.

A skin owns only visual geometry:

- base eye position and size
- eye renderer
- brow renderer and thickness
- optional skin-specific details

There are 13 skins in v3.9: Soft, Robot, Compact, Wide, Arcade, Alien, Cat, Visor, Core, Sleepbot, Scout, Bubble and Mask.

## 2. Emotion

`resolvePetExpression()` converts a `PetMood` into generic expression parameters:

- eye openness
- horizontal scale
- per-eye offsets
- brow tilt / arch
- heart-eye mode
- sparkle / jitter effects

The emotion has no knowledge of the active skin. This means a new emotion automatically works on every skin.

## 3. Motion

`resolvePetMotion()` converts Desk Buddy motion into a physical pose:

- gravity shift X/Y
- side compression
- eye clustering toward the lower side
- vertical squash
- movement inertia / wobble
- face-down state

`DisplayUI::drawFace()` combines Skin + Emotion + Motion, then adds the physical top-button squash animation.

## Adding a new skin

1. Add a `PetEyeStyle` only if none of the existing renderers fit.
2. Add one entry to `SKINS` in `src/PetVisual.cpp`.
3. Increment `PET_SKIN_COUNT` in `include/PetVisual.h`.
4. If a new eye renderer is needed, implement its case in `DisplayUI::drawStyledEye()`.
5. Add the skin to the Web BLE `<select>` and preview CSS.

No emotion, Pomodoro, touch, or MPU behavior needs to be duplicated.

## Adding a new emotion

1. Add the enum value to `PetMood`.
2. Add its normalized expression to `resolvePetExpression()`.
3. Trigger that emotion from the behavior logic in `main.cpp`.

All skins will inherit it automatically.
