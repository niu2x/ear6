# Ear6 Web UI Redesign (v2)

## Goals

This version focuses on practical usability improvements:

- Larger menu/button text for readability on desktop and laptop displays
- English-only interface text
- Simpler structure with one primary NES workflow instead of maintaining three active contexts
- Dedicated fullscreen button
- Built-in instructions/help modal for first-time users
- Cleaner retro-hardware style with better visual hierarchy

## Visual Direction

Theme: **warm industrial console** (not pixel-art UI).

- Dark steel background with subtle gradients
- Bright amber/orange action color for primary controls
- High-contrast text with clear type sizes (15px-24px)
- CRT-like framed screen area with standby text animation

## Layout

1. **Top Bar**
   - Power/status indicator + title `Ear6 Emulator`
   - ROM file name on the right

2. **Control Toolbar**
   - `Open ROM`, `Reset`, `Run/Pause`, `Fullscreen`, `How to Play`
   - `Region` selector (`NTSC`, `PAL`)
   - System pills shown as status only: `NES Ready`, `Test (soon)`, `Flash (soon)`

3. **Screen Area**
   - Main canvas centered in a CRT-style frame
   - Standby message when no ROM is loaded

4. **Bottom Status Bar**
   - Left: state text (`Ready`, `Loading ROM...`, `Running`, `Paused`, errors)
   - Right: live FPS counter

## Interaction Rules

- `Open ROM`: loads file and shows first frame
- `Run/Pause`: toggles emulation loop
- `Reset`: reloads the currently loaded ROM data
- `Region`: applies NES region immediately
- `Fullscreen`: enters/exits browser fullscreen on the screen container
- `How to Play`: opens modal instructions and key mapping

## Keyboard Mapping

- Arrow keys: D-Pad
- `Z`: A
- `X`: B
- `Enter`: Start
- `Shift`: Select

## Scope Notes

- This UI version prioritizes a strong NES experience first
- Test/Flash are intentionally shown as non-interactive placeholders to reduce cognitive load
- Future versions can reintroduce multi-system active switching when those systems have complete UX flows
