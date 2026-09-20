# Architecture V5

V5 separates four concerns:

1. **Inputs and sensors** report button, touch, gravity, motion and environment state.
2. **BehaviorEngine** arbitrates temporary events by priority and keeps a small pending queue.
3. **Emotion + motion** describe what the companion should feel and how the head should physically move.
4. **Skin rendering** turns the shared state into one of the visual designs.

Priority order is roughly:

`Critical fall > explicit user interaction > physical motion > gravity event > ambient event`

Gravity deformation itself remains continuous and is not queued.

## Rendering rates

- MPU6050 sampling: ~100 Hz.
- BMP180: every 1.5 s.
- OLED target: user-selectable 10–60 FPS.
- Web diagnostic telemetry: on demand / approximately 1 Hz snapshots.

## OLED transfer optimization

The renderer computes an FNV-1a hash of the 1024-byte framebuffer. If it is unchanged from the previously presented frame, `display()` is skipped. This safely reduces I²C traffic without controller-specific partial-page commands.
