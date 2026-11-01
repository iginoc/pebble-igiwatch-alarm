---
alwaysApply: true
---

## Project Overview

This is a Pebble smartwatch application written in C using the Pebble SDK.

## Supported Platforms

The app targets multiple Pebble watch models:
- aplite (Pebble classic)
- basalt (Pebble Time)
- chalk (Pebble Time Round)
- diorite (Pebble 2)
- emery (Pebble Time 2)

## Commands

```bash
# Build the app for all platforms
pebble build

# Clean build artifacts
pebble clean

# Install the app on specific emulator
pebble install --emulator basalt

# Screenshot the running emulator
pebble screenshot --no-open screenshot.png
```

If you need more information on the `pebble` command or a sub-command, append `--help`.

## Project Structure

```
src/c/           - C source files for the watchface
src/pkjs/        - PebbleKit JS entry point and Clay settings schema
resources/fonts/ - Custom vector fonts
resources/images/ - App icon and alarm image
build/           - Generated build output; do not edit by hand
```
## Configuration

This project is configured as a watchface (`"watchface": true`) in `package.json`.

The Clay configuration in `src/pkjs/config.js` provides up to three alarm slots. Each slot has an enable toggle, a time, and weekday checkboxes. `src/pkjs/index.js` converts the selected weekdays to a bitmask before sending settings to the watch.

## Architecture

The application follows the standard Pebble app architecture:

1. **Main Entry Point**: `src/c/igiwatchalarm.c` - `main()` initializes the watchface and starts the event loop.
2. **Window Management**: A single window contains canvas and status layers for the clock, alarm screen, and status indicators.
3. **Alarm Handling**: Three persisted alarm slots are evaluated against their weekday masks. `schedule_alarm_wakeups()` schedules the next selected occurrence for each active slot.
4. **Event Handling**: Pebble OS reserves hardware buttons for watchfaces. An accelerometer tap dismisses a ringing alarm; a double tap outside alarm mode shows the steps glance.

## SDK Documentation

The full Pebble SDK documentation is available at https://developer.repebble.com.

Main Categories:
- Tutorials - Step-by-step learning (C watchface tutorial in 5 parts, advanced topics)
- Developer Guides - Comprehensive reference organized by topic

Key Sections:
- App Resources - Images, fonts, vector graphics, 256 resource limit
- User Interfaces - Layer hierarchy, TextLayer, MenuLayer, round vs rectangular displays
- Events & Services - Buttons, accelerometer, compass, health data, background workers
- Communication - Bluetooth AppMessage, PebbleKit JS/Android/iOS integration
- Graphics & Animations - Drawing APIs, property animations, vector graphics
- Debugging - App logs, GDB, common errors and solutions
- Best Practices - Multi-platform support, battery conservation, modular architecture
- Design & Interaction - Glance-first design, one-click actions, platform guidelines
- App Store Publishing - Submission requirements, assets, analytics

Key Entry Points:
- https://developer.repebble.com/tutorials/watchface-tutorial/part1 - C development start
- https://developer.repebble.com/guides/events-and-services/buttons - Button handling
- https://developer.repebble.com/guides/user-interfaces/layers - UI foundations

## Development Best Practices

- Whenever making changes, run `pebble screenshot` and view the screenshot to make sure it's what the user requested. If not, make more changes until it does what it's supposed to.

## Antigravity CLI & AI Interaction Guidelines

This project transitioned to the Antigravity multi-agent platform. Use the `antigravity` CLI for AI-assisted tasks.

- When given an image of a watchface to replicate, describe the target watchface in precise detail. Note every visual element present, as well as size, alignment, font weight, spacing, and location.

## Code Review Guidelines

- Once you think you've fulfilled the user's request, ask yourself if you see any issues with the current screenshot, and if there are any differences between the screenshot and the reference image or the user's description. If so, fix them.