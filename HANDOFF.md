# HANDOFF.md - IgiWatchAlarm Project Handoff Documentation

## 1. Project Overview

**IgiWatchAlarm** is a multifunctional Pebble smartwatch application (watchapp) developed in C using the Pebble SDK. It combines a dynamic and configurable clock display with advanced typography, step counting, orientation modes, and an integrated alarm system configurable via smartphone settings.

### Key Capabilities
- **Time & Date Display**: Supports custom fonts (Avenir Next, Leco 1976, Avenir Next Regular) rendered via `pebble-fctx`, with orientation settings (Portrait, Landscape Left, Landscape Right) and multilingual date localization (Italian, English, French, German, Spanish).
- **Status Indicators**: Real-time battery indicator with color coding, Bluetooth connection status, and alarm status icon positioned directly below the battery.
- **Step Counting & Date Glance**: Triggered by a double-tap to display daily step count and expanded date.
- **Smart Alarm System**:
   - Supports up to three independently enabled alarms. Each slot has a time and a selected set of weekdays, configured in the Pebble mobile app via Clay (`Alarm1Active`/`Alarm1Time`/`Alarm1Days`, through slot 3).
   - Schedules the next occurrence of each active alarm with Pebble OS `wakeup_schedule()`, so alarms fire whether the app is open or closed. An alarm with no selected days is not scheduled.
   - Persists each slot's enabled state, time, and weekday mask across app restarts. Existing single-alarm settings are migrated to slot 1 as a daily alarm.
  - Cadence vibration: a distinct "tic" pulse every second, automatically stopping after 15 seconds if unhandled.
  - Dedicated alarm UI: Displays current time at the top, a large ringing alarm clock graphic (`ALARM_LARGE`) in the center, and dismissal hint at the bottom.
  - Interactive dismissal: Pressing any physical button (UP, SELECT, DOWN, BACK) or tapping/shaking the watch immediately stops the alarm.
  - Auto-timeout: Automatically silences after 15 seconds if not dismissed manually.

---

## 2. Supported Platforms

The project builds for all major Pebble platforms:
- **aplite**: Pebble Classic / Pebble Steel (monochrome, 144x168)
- **basalt**: Pebble Time / Pebble Time Steel (color, 144x168)
- **chalk**: Pebble Time Round (color, 180x180 round)
- **diorite**: Pebble 2 SE / Pebble 2 HR (monochrome high-contrast, 144x168)
- **emery**: Pebble Time 2 (color high-res, 200x228)

---

## 3. Project File Structure

```
igiwatchalarm/
├── package.json              # Package manifest, dependencies, resources, and messageKeys
├── package-lock.json         # Locked npm dependencies
├── wscript                   # Waf build configuration script
├── CLAUDE.md                 # Antigravity / AI interaction guide
├── README.md                 # Project description and user guide
├── HANDOFF.md                # Technical handoff documentation (this file)
├── resources/
│   ├── fonts/                # Vector ffonts used by pebble-fctx
│   │   ├── AvenirNextDemiBold.ffont
│   │   ├── AvenirNextRegular.ffont
│   │   ├── LECO1976-Regular.ffont
│   │   └── PeaceSansThickOutline.ffont
│   └── images/               # Bitmaps & app icons
│       ├── menu_icon.png     # 24x24 app menu icon
│       ├── alarm_large.png   # 84x84 large alarm ringing illustration
│       └── bluetooth.png     # Bluetooth status graphic
└── src/
    ├── pkjs/
    │   ├── index.js          # PebbleKit JS initialization (Pebble Clay)
    │   └── config.js         # Clay configuration schema for mobile settings
    └── c/
        ├── igiwatchalarm.h   # Public header
        └── igiwatchalarm.c   # Main application logic and layer rendering
```

---

## 4. Architecture & Key Subsystems

### 4.1 Watchapp vs Watchface Configuration
In `package.json`:
```json
"watchapp": {
  "watchface": false,
  "iconImage": "MENU_ICON"
}
```
> [!IMPORTANT]
> Pebble OS intercepts all hardware button presses on watchfaces (`"watchface": true`), preventing button click handlers from running. By configuring the app as `"watchface": false`, the application can use `window_set_click_config_provider()` to intercept physical button clicks (UP, SELECT, DOWN, BACK) to silence alarms.

### 4.2 Alarm Subsystem
1. **Settings Delivery (`src/pkjs/config.js` -> `src/pkjs/index.js` -> AppMessage)**:
   - Three slots each provide an active boolean, an `HH:MM` time string, and a weekday bitmask (`Alarm1Active`, `Alarm1Time`, `Alarm1Days` through slot 3).
   - The Clay weekday options are ordered Monday through Sunday. The JavaScript maps these to Pebble `tm_wday` bits: bit 0 is Sunday, followed by Monday through Saturday.
   - A zero weekday mask leaves the slot enabled in settings but prevents it from being scheduled or triggered.
2. **Persistence (`persist_write_*` / `persist_read_*`)**:
   - Each slot's state is retained across app restarts using its corresponding `MESSAGE_KEY_AlarmNActive`, `MESSAGE_KEY_AlarmNTime`, and `MESSAGE_KEY_AlarmNDays` keys.
   - On upgrade, legacy `AlarmActive` and `AlarmTime` values are copied to slot 1, with all seven days enabled.
3. **Pebble Wakeup API**:
   - `schedule_alarm_wakeups()`: Finds the next selected weekday and time for each active slot. It cancels old app wakeups and schedules up to one next occurrence per slot, using the slot index as the wakeup cookie.
   - `wakeup_service_subscribe()`: Receives wakeup callbacks if the app is already open.
   - `APP_LAUNCH_WAKEUP`: Detects if the app was launched by a scheduled wakeup event when closed and triggers the alarm immediately.
4. **Trigger & Vibration**:
   - `trigger_alarm()` sets `s_alarm_ringing = true`, resets `s_alarm_second_count = 0`, triggers an immediate 80ms "tic" vibration pulse (`s_alarm_tic_pattern`), and schedules repeating ticks every 1000ms via `s_alarm_vibe_timer`.
   - On each second elapsed, `alarm_vibe_timer_callback()` emits another "tic". If `s_alarm_second_count >= 15` (15 seconds elapsed), it automatically calls `stop_alarm()`.
5. **UI Rendering (`layer_update_proc`)**:
   - When `s_alarm_ringing` is true, the canvas switches to the alarm screen:
     - Top: Current time in `FONT_KEY_BITHAM_34_MEDIUM_NUMBERS`.
     - Center: Large 84x84 `ALARM_LARGE` bitmap drawn with `GCompOpSet`.
     - Bottom: Dismissal instructions ("Premi un tasto").
   - Status bar rendering is suppressed during ringing.
6. **Dismissal**:
   - Physical button handlers (`button_click_handler` and `back_click_handler`) or accelerometer tap (`tap_handler`) call `stop_alarm()`.
   - `stop_alarm()` cancels timers, cancels vibration (`vibes_cancel()`), sets `s_alarm_ringing = false`, redraws layers, and reschedules the next wakeup.

---

## 5. Development & Testing Commands

### Build
```bash
pebble build
```

### Install on Emulator (e.g. Basalt)
```bash
pebble install --emulator basalt
```

### Emulate Button Clicks
```bash
pebble emu-button --emulator basalt click select
pebble emu-button --emulator basalt click up
pebble emu-button --emulator basalt click down
pebble emu-button --emulator basalt click back
```

### Send Settings AppMessage (Test Alarm)
Set alarm slots, times, and weekdays through the IgiWatchAlarm configuration page in the Pebble mobile app. The numeric AppMessage key IDs are generated from `package.json` and should not be hard-coded in test commands.

### Set Emulator Time
```bash
pebble emu-set-time --emulator basalt 10:00:00
```

### Take Screenshot
```bash
pebble screenshot --no-open screenshot.png
```

---

## 6. Current Status & Verification

- **Build**: Successfully compiles for `aplite`, `basalt`, `chalk`, `diorite`, and `emery` without warnings or errors.
- **Alarm UI**: Verified via emulator screenshots with proper layout on color and round displays.
- **Button Dismissal**: Verified on emulator; button clicks dismiss the alarm immediately and restore the normal watch face.
- **15-Second Timeout**: Verified on emulator; the alarm automatically turns off and returns to normal clock after 15 seconds.

