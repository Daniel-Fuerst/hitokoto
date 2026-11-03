# S027 — Smooth scrolling in the editor view

- **Status:** In review · **Order:** 28 · **Size:** S · **Depends on:** S019
- **Basis:** User request on 2026-11-03: implement smooth scrolling in the view. Mouse wheel scrolling
  animates fluidly towards the target scroll position instead of jumping instantaneously in a single
  step, with target accumulation for rapid successive wheel notches, immediate reversal when scrolling
  in the opposite direction, boundary clamping, and instant interruption when the user directly
  interacts with the scrollbar or keyboard.

## Story

As a reader and writer, I want mouse wheel scrolling in the editor to animate smoothly, so that
moving through the document feels fluid and natural rather than jarring and abrupt.

## Acceptance scenarios

**Scenario: mouse wheel scrolling animates smoothly**
- Given an editor window showing a document with a vertical scroll range
- When a mouse wheel event occurs (e.g. `angleDelta.y() == -120` to scroll down)
- Then the vertical scroll position does not jump to the destination in a single frame
- And it animates smoothly across multiple frames to the target using an easing curve (OutCubic)
- And after the animation duration (200 ms), the scroll bar settles at the exact target value

**Scenario: rapid wheel events accumulate target smoothly**
- Given an active smooth scroll animation
- When another wheel event in the same direction arrives
- Then the new target accumulates the additional distance
- And the animation continues smoothly toward the combined target without stuttering

**Scenario: reversing wheel direction redirects immediately**
- Given an active smooth scroll animation moving downward
- When a wheel event in the upward direction arrives
- Then the animation reverses immediately towards the upward target from the current position,
  without fighting or having to wait out previous downward travel

**Scenario: direct interaction cancels animation**
- Given an active smooth scroll animation
- When the user presses the scroll bar slider, clicks in the editor, or presses a key
- Then the animation stops immediately at the current position, giving immediate control to the user

**Scenario: boundary clamping**
- Given the editor near the top (0) or bottom (`maximum()`)
- When wheel events scroll beyond the boundaries
- Then the animation clamps cleanly at 0 or `maximum()` without overshooting

**Scenario: touchpad pixel deltas animate smoothly**
- Given a scroll event with non-zero `pixelDelta` (such as a touchpad)
- When the event arrives
- Then the view animates smoothly towards the target without instantaneous jumps

**Scenario: toggle via options**
- Given `Options::smoothScrolling` is false (or disabled on the editor)
- When a mouse wheel event occurs
- Then the view scrolls instantaneously in standard single steps without animation

