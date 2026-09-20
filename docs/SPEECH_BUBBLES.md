# Speech bubbles

The speech layer is independent from **Skin + Emotion + Motion**.

When a speech event starts:

1. the normal full-screen face yields to a smaller, distant rendering of the current skin;
2. a rounded speech bubble pops in;
3. text is typed progressively with a small cursor;
4. after the event, the normal full-screen pet returns.

Copy is kept to **one or two words maximum** so it remains readable on the 128×64 OLED.

Contexts currently include:

- boot greeting;
- petting;
- pickup / settle;
- strong shake recovery;
- focus / break completion;
- sleep / wake transitions.

The Web BLE option `Speech bubbles` disables the complete feature. `Context text frequency` is a master probability multiplied by a per-context probability. Normal bubbles also have an approximately 30-second minimum gap, and stale queued lines expire instead of appearing late. Ambient idle events are deliberately silent so the companion never asks for attention. Boot, petting, focus-completion and sleep/wake messages remain available when speech is enabled, while their exact wording varies.
