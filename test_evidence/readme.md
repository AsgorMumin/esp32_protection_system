# Evidence

[Main README](../readme.md) · [Operating guide](../project_files/readme.md) · [System design](../schematics/block_diagram.md)

The applicant reported successful Wokwi operation of sensor controls, LCD, buttons, voltage protection, current latch/reset, and MQTT settings control. These are user-observed functional results, not an automated test run.

- [mqtt_status.png](mqtt_status.png): supplied screenshot showing 229.9 V, 1.5 A, relay=true, LOAD ON and default limits in MQTT Explorer.
- [current_latch.png](current_latch.png): supplied screenshot showing CURRENT LATCH at 233.4 V and 0.6 A after a previous over-current event. It illustrates the retained latch, not its original trigger or reset timing.

## Verification checklist

Follow the [operating guide](../project_files/readme.md). Before independent tests, restore SET 190 250 10 3000 and clear any latch under safe readings. Do not mark a case passed without observing it.

| Check | Procedure | Expected result |
|---|---|---|
| Startup | Restart near 230 V / 2 A | Relay OFF initially; ON after healthy delay |
| High voltage | Raise above 250 V | HIGH VOLTAGE; relay OFF; fault alerts active |
| Low voltage | Lower below 190 V | LOW VOLTAGE; relay OFF; fault alerts active |
| Voltage recovery | Restore 230 V / 2 A | Alarm clears; delayed reconnection |
| Interrupted recovery | Introduce fault before delay finishes | Qualification restarts |
| Over-current | Raise above 10 A | CURRENT LATCH; relay OFF |
| Latched recovery | Lower current to 2 A | Remains disconnected until RESET |
| Unsafe reset | Request RESET while readings are unsafe | Rejected; load remains disconnected |
| Safe reset | RESET near 230 V / 2 A | Latch clears; delayed reconnection |
| Local settings | MENU then UP/DOWN | LCD and telemetry show changed limit |
| Remote settings | At 230 V, SET 190 220 10 3000 | OK reply; high=220; voltage trip |
| Invalid settings | SET 260 190 10 3000 | Error; previous configuration preserved |
| Zero current | Healthy voltage, zero current, no latch | Load can connect |
| Restart settings | Change settings and restart | Defaults restored |

For each captured result, record readings, limits, command/button action, and observed LCD/relay/reply. Add screenshot links here after saving files. Screenshots alone do not establish response latency; timing claims need traces or logs.

Before submission, add evidence for voltage trips, current trigger/reset, local settings, remote SET acceptance, invalid SET rejection and reconnect delay. Record actual readings, settings and results. Wi-Fi-outage behavior and worst-case response timing remain unmeasured. No hardware results are claimed.
