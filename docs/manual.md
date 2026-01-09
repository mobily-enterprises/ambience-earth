# ambience-earth - User Manual

This is a very verbose, step-by-step guide for someone who has never used the device before.
If you just want build steps and wiring, see `README.md`.

## What this device does (plain English)

This controller waters plants based on soil moisture. You define one or more "feed slots."
Each slot says when watering is allowed (time window), when it should start (soil is too dry),
and when it should stop (soil is wet enough, runoff detected, max volume reached).

The system also keeps a "baseline" moisture value. Some feed slots can use that baseline
instead of fixed percentages. The baseline is updated automatically from a special kind
of feed called a "baseline setter."

It also keeps a log, so you can see when feeds happened and what the moisture was.

## Controls and navigation (very important)

The device has 5 buttons (UP, DOWN, LEFT, RIGHT, OK).

- UP/DOWN: change a value or move the selection.
- RIGHT or OK: go forward or accept a value.
- LEFT: go back or cancel.

In menus, a small triangle cursor points at the current choice.
Press OK to enter that item.

Whenever you see a number field (time, percentage, ml), use UP/DOWN to change it.
RIGHT/OK moves to the next field or accepts the value.
LEFT goes back.

## First power-on (what you will see)

### 1) Button calibration

On first power-on, or if the button configuration is missing, the device runs button setup.
It shows prompts like:

- Press UP
- Press DOWN
- Press LEFT
- Press RIGHT
- Press OK

For each prompt, press the requested button once. The device records the analog value.

Tip: You can force this setup later by holding the button that reads 0 (often the OK button,
depending on your keypad wiring) while powering the device.

### 2) Initial setup menu (mandatory)

After buttons are calibrated, the device checks if required settings are present.
If not, it shows a short Settings menu with only the missing items.
You must complete all of them before normal operation begins.

The required items are:

1. Set time/date
2. Max daily
3. Lights on/off
4. Calibrate moisture sensor (dry and wet)
5. Calibrate flow (dripper)

The menu will keep returning until everything is complete.

## Home screen (idle screen) explained

When the device is idle, you see a compact status screen:

Line 1:
- `Moist: 57%` = current soil moisture percent
- `Db: --%` or `Db: 12%` = dryback percent (see "Dryback" below)

Line 2:
- `DAY` or `NIGHT` = based on Lights on/off schedule
- time on the right, like `09:27`

Line 3:
- `Last 0.1h ago 3ml`
  - how long since the last feed ended
  - how many ml were delivered in that feed

Line 4:
- `Td:3ml` = total delivered today
- `B/l: N/A` or `B/l: 80%` = current baseline moisture (only shown if a baseline setter exists)

Also:
- A `!` on the bottom line means a runoff warning happened.
  - This warning clears when you open Logs or Settings.

### Dryback (Db)

Dryback is an estimate of how much moisture dropped between "lights on" and "lights off."
It uses periodic "Values" logs. If there are not enough data points, it shows `--%`.

## Screensaver

After some idle time, the screen backlight turns off and a star `*` (or `!` if there is a runoff warning)
appears at a random location. Press any button to wake the screen.

## Main menu (press OK on the home screen)

The main menu has these items:

1. Logs
2. Feeding
3. Force feed
4. Pause feeding / Unpause feeding
5. Settings

Use UP/DOWN to move and OK to enter.

### Pause feeding

This disables automatic feeding. It also stops an active feed (unless it was forced).
Use it for maintenance.

### Force feed

Force feed starts a selected slot immediately, ignoring start conditions.
The feed still stops based on stop conditions (moisture target, runoff, max volume, max daily).

## Feeding menu

The Feeding menu has:

1. Schedule
2. Max daily
3. Baseline - X
4. Baseline - Y

### Max daily

Sets the maximum total volume (ml) allowed per day across all slots.
If the total is reached, further feeds are skipped until the next day.

### Baseline - X and Baseline - Y

These are global offsets (2 to 20, step 2).
They are used when you choose "Baseline - X" or "Baseline - Y" in slot configuration:

- Start condition: Baseline - X
- Stop condition: Baseline - Y

Example: if baseline is 80% and X is 10, "Baseline - X" means 70%.

## Schedule and slot configuration

There are 8 feed slots. Each slot can be enabled or disabled.

### Slot list

In Schedule, you see a list of slots with their name and (On)/(Off).
Select a slot to view its summary. Press OK to edit it.

### Slot summary screen (read-only view)

Line 1: Slot name and (On/Off). If it is a baseline setter, you also see `Settr`.

Line 2: Start conditions
  - Time window: `T+HH:MM` and optional `/HH:MM`
  - Moisture start: `M<XX%`
  - If baseline is not available, it shows `--` instead of a number.

Line 3: Stop conditions
  - `Stop:` then `M>XX%` (moisture target), and/or `R-off` (runoff stop)
  - `R+` means "must runoff"
  - `R-` means "avoid runoff"

Line 4: Volume and delay
  - `Max:500ml` or `Max:N/A`
  - `D:1040m` = minimum minutes between feeds

### Editing a slot (question by question)

1) **Slot on?**
   - Yes = slot is active
   - No = slot is ignored

2) **Name**
   - Give the slot a name (optional).

3) **Start when -> Time window?**
   - No: skip time window.
   - Yes: enter two values:
     - `L+ start`: offset from lights-on (HH:MM)
     - `Check for`: window length (HH:MM)
       - If `00:00`, the slot only starts at the exact minute of `L+ start`
       - If non-zero, it can start any time within that window

4) **Start when moist <**
   Options:
   - `Baseline - X`: uses the baseline and subtracts X
   - `%`: enter a fixed percentage
   - `No`: ignore moisture for start

5) **Stop wnen moist >**
   Options:
   - `Baseline - Y`: uses the baseline and subtracts Y
   - `%`: enter a fixed percentage
   - `No`: ignore moisture for stop

6) **Max volume**
   - Maximum volume in ml for that slot.
   - If 0 or unset, the summary shows `Max:N/A`.

7) **Runoff present?**
   - Yes: feed will stop when runoff is detected.
   - No: runoff is ignored as a stop condition.

8) **Must runoff? / Avoid runoff?**
   - This is only about expectations and warnings, not control logic.
   - If you answered "Runoff present? Yes", you will see **Must runoff?**
     - Yes means runoff should be seen.
     - No means runoff should not be seen.
     - Neither means do not check.
   - If you answered "Runoff present? No", you will see **Avoid runoff?**
     - Yes means runoff should NOT be seen.
     - No means runoff is acceptable.
     - Neither means do not check.

9) **Baseline setter?**
   - This question appears only if:
     - Runoff present? = Yes
     - Must runoff? = Yes
   - If Yes, this feed will be used to update the baseline (see below).

10) **Min time btwn**
   - Minimum minutes between feeds for this slot.
   - 0 means no minimum delay.
   - This delay is enforced across reboots.

11) **Save?**
   - Yes saves the slot.
   - No exits without saving.

## Baseline (what it is and how it updates)

Baseline is a reference moisture percent that can be used as a moving target.

How it works:

- You mark a slot as a **Baseline setter**.
- When that slot runs, its log entry is created without a baseline value.
- Between 30 and 60 minutes after that feed ends, the device captures a stable moisture reading.
- That reading is written into the log entry and becomes the new baseline.

The device always uses the most recent log entry that has a baseline.
If no baseline exists yet, the home screen shows `B/l: N/A` and any baseline-based start/stop
conditions are treated as not met.

## Logs (how to read them)

Open Logs from the main menu. The most recent entry appears first.

Controls:
- UP: older log
- DOWN: newer log
- LEFT: exit logs

Log types:

1) **BootUp**
   - Shows when the device booted and the soil moisture at boot.

2) **Feed**
   - Shows slot number, start time, volume, daily total, and stop reason.
   - Stop reason codes:
     - `Mst` = moisture target reached
     - `Run` = runoff detected
     - `Max` = max volume reached
     - `Day` = max daily water reached
     - `Cfg` = UI pause / settings
     - `Cal` = not calibrated
   - A `!` means a runoff warning happened.
   - The bottom line shows moisture before/after, and `R` if runoff was seen.

3) **Values**
   - Periodic moisture samples used for dryback calculations.

## Settings menu (full menu)

Open Settings from the main menu. It contains:

- Set time/date
- Calibrate (moisture + dripper flow)
- Lights on/off
- Test peripherals
- Reset

### Set time/date

1) Set time (HH:MM) using UP/DOWN.
2) Set month, day, year.
3) Confirm with Save? = Yes.

### Calibrate -> Moisture sensor

You need two calibration points:

- **Calibrate DRY**
  - Keep the sensor dry (air).
  - The device collects a short window and displays a raw value.
  - Press OK to accept.

- **Calibrate WET**
  - Place the sensor in fully wet conditions (water).
  - The device collects a short window and displays a raw value.
  - Press OK to accept.

Do both, or the initial setup will not complete.

### Calibrate -> Calibrate flow

This determines how fast your pump/dripper delivers water.

Step-by-step:
1) Place a container under the output.
2) Press OK to start the pump.
3) When exactly 100 ml is collected, press OK again.
4) Enter your desired flow rate in ml/h.
5) The device shows the calculated pulse On/Off times.

### Lights on/off

Set the lights-on and lights-off times. These are used for:
- Day/Night display
- Time window offsets (L+ start)
- Dryback calculations

### Test peripherals

- **Test sensors**: shows raw moisture ADC value and the runoff sensor state (0/1).
- **Test pump**: blinks the pump output three times.

### Reset

- Reset logs: clears the log history only.
- Reset data: clears config and logs, then forces full setup again.

## Runoff warnings (the "!" indicator)

The device sets a runoff warning when:
- Runoff was required but not seen, or
- Runoff was supposed to be avoided but was detected.

The `!` clears when you open Logs or Settings.

## Troubleshooting tips

- If the screen shows `RTC ERR`, check the RTC wiring and I2C hub.
- If baseline always shows N/A, make sure at least one slot is a baseline setter and that it actually runs.
- If buttons act weird, re-run button setup at boot.
- If moisture never updates, confirm sensor calibration (both dry and wet).
