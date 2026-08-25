# Ear6 Web UI

## Product Goal

The Ear6 project is a multi-system emulator library. The current Web UI is an
immersive NES host because its file detection and input mapping only expose the
NES system today. The game framebuffer is the primary surface; controls and
diagnostics stay compact and readable around it. UI effects must never alter
the framebuffer pixels.

## Visual Direction

Theme: precision console in a dark room.

- Neutral near-black environment instead of a blue-tinted application shell
- Warm amber for the primary action only
- Green, orange, and red reserved for runtime and performance state
- High-contrast warm-white text and muted labels that remain readable
- Square, hardware-like controls with restrained borders and motion
- Responsive one-screen layout on common desktop and mobile viewports

## Layout

1. **Header**
   - Ear6 identity, current state, and loaded ROM name
2. **Desktop workspace**
   - Open ROM, save/load state, reset, run/pause, fullscreen, and keyboard
     controls in a left rail
   - Unmodified 256x240 framebuffer centered in a minimal hardware frame
   - Live status and one combined FPS/step-load performance panel in a right
     rail
3. **Responsive controls**
   - Below 1000px, controls and diagnostics move beneath the screen so the
     framebuffer keeps the full viewport width
   - On mobile, the seven controls wrap into a four-column icon grid
4. **Build strip**
   - Short Git revision aligned left and build timestamp aligned right
   - Build timestamp is stored as ISO UTC and rendered in the browser's local
     timezone

## Runtime Metrics

`FPS` counts actual emulated frames, not browser repaint callbacks. Emulation
uses a fixed 60Hz timestep even on 120Hz and 144Hz displays.

`STEP LOAD` measures only the average wall-clock duration of `ear6_step()`:

```text
average step time / 16.67 ms frame budget * 100
```

A 5% load means the core has roughly 20x real-time compute headroom. Values at
or above 100% mean the core alone cannot sustain 60 FPS. Canvas copying and
browser rendering are intentionally excluded from this metric.

## State Files

The Save control writes the current `.e6s` state to `localStorage`. Records are
keyed by system type and the core container's 64-bit content identity, so each
ROM has exactly one local state and a newer save replaces the older one. The
Load control opens a menu sorted by save time with ROM names, local-time
timestamps, and framebuffer previews.

The same menu retains explicit `.e6s` import and export actions. Loading either
a local or imported state restores an otherwise empty NES context; no separate
ROM selection or network request is required because the state embeds the
original content. A loaded state starts paused. The cold-reset control is
disabled until the host has original ROM bytes available locally.

## File Selection

The ROM picker accepts the supported filename extensions and generic binary
files. The binary MIME fallback is required for iOS browsers, where Files may
identify `.nes` content as `application/octet-stream` instead of mapping the
custom extension directly. The core still validates selected content before it
is loaded.

## Build Metadata

Vite reads the short Git revision during the build and embeds an ISO build
timestamp. GitHub Actions already checks out the repository before running the
Vite build, so the Pages workflow requires no extra metadata step or variable.

## Keyboard Mapping

- Arrow keys: D-Pad
- `Z`: A
- `X`: B
- `Enter`: Start
- `Shift`: Select
