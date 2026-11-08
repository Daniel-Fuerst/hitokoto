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

**Scenario: a scroll by the program wins over the animation**
- Given an active smooth scroll animation
- When the program scrolls the view (for example to a search match, or to keep the content still
  while the layout above the viewport changes its height)
- Then a jump stops the animation and stays where the program put it
- And a shift of the layout moves the running animation and its target by the same distance

**Scenario: boundary clamping**
- Given the editor near the top (0) or bottom (`maximum()`)
- When wheel events scroll beyond the boundaries
- Then the animation clamps cleanly at 0 or `maximum()` without overshooting

**Scenario: touchpad pixel deltas scroll directly**
- Given a scroll event with a non-zero `pixelDelta` (a trackpad or high-resolution touchpad, which
  also reports an `angleDelta`)
- When the event arrives
- Then the view scrolls by exactly the pixel delta, without an animation of its own, as Qt's
  documentation of `QWheelEvent::pixelDelta()` asks; the system already moves the content smoothly
- And a running wheel animation stops and the view continues from its current position
- On X11, where pixel deltas are driver-specific and unreliable, the angle delta is used instead
  (review O4, decided by Armin on 2026-11-08)

**Scenario: wheel over an embedded table scrolls the editor**
- Given the mouse pointer over a table embedded in the editor (its cells or its controls)
- When a wheel event occurs
- Then the editor scrolls as it would anywhere else in the view; the table itself never scrolls

**Scenario: toggle via options**
- Given `Options::smoothScrolling` is false (or disabled on the editor)
- When a mouse wheel event occurs
- Then the view scrolls instantaneously in standard single steps without animation

