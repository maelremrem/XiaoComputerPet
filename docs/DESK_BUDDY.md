# Desk Buddy behavior

## Priority

The pet uses a simple priority model so autonomous behavior never fights the user:

1. Pomodoro / menus / explicit button interaction
2. Active MPU motion and orientation
3. Side-touch gestures
4. Temporary Desk Buddy emotions
5. Rare idle events
6. Normal random idle expressions

## Persistent needs

- **Mood**: general positive/negative state.
- **Energy**: slowly decreases while awake and recovers during scheduled sleep/breaks.
- **Affection**: grows with interactions and slowly fades over long periods.
- **No attention debt**: the pet never becomes bored because you leave it alone. Mood and affection do not decay from inactivity.

## Side-touch gestures

When Advanced side-touch gestures is enabled:

- Hold left > ~0.5 s: left scratch.
- Hold right > ~0.5 s: right scratch.
- Touch both sides > ~0.12 s: hug.
- Move from one side to the other within ~0.5 s, then release the first side: lateral pet/swipe.

The gesture layer is only active on the normal pet screen. In menus the same sensors remain dedicated to previous/next navigation.

## Rare events

Ambient idle events only fire when the pet is idle, not sleeping, no control is touched, and MPU motion is low. They are intentionally silent and never display attention-seeking speech. The interval is configured in Web BLE. Available events are yawn, sneeze, hiccup, dream and mini-dance.

## Pomodoro companion

When enabled, tiny eyes appear above the progress bar during an active timer. During focus they gradually become more tired, occasionally blink, then open wider during the final minute. Break timers use a relaxed breathing expression.

## Animation tuning

Web BLE exposes shared renderer controls:

- Idle animation speed
- Eye-follow strength
- Cartoon inertia
- Squash / stretch
- Heart particle count

These parameters are applied to the common animation engine and therefore affect every skin consistently.
