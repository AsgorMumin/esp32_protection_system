# Setup and operating guide

[Main README](../readme.md) · [Design and wiring](../schematics/block_diagram.md) · [Test evidence](../test_evidence/readme.md)

## Recreate the project

1. Open the [saved simulation](https://wokwi.com/projects/477495719368653825), or create an [ESP32 Arduino project](https://wokwi.com/projects/new/esp32).
2. Copy the entire [firmware](../src/sketch.ino) into the sketch.ino tab. Do not use main.c: Arduino libraries require C++.
3. Replace the diagram tab with [diagram.json](diagram.json).
4. Add **LiquidCrystal I2C** and **PubSubClient** in Library Manager, or use [libraries.txt](libraries.txt).
5. Press Play. Set the readings near 230 V and 2 A. The load should connect after a healthy interval.

The diagram initializes the knobs near those values. Pot1 maps to 0-300 V; pot2 maps to 0-20 A. They emulate measurements, not calibrated physical sensors. Default limits are 190-250 V, 10 A, and 3000 ms reconnect delay.

## Device controls

| Button | Color | Action |
|---|---|---|
| MENU | Yellow | Select next screen/setting |
| UP | Blue | Increase selected setting |
| DOWN | Green | Decrease selected setting |
| RESET | Red | Clear current latch when both readings are safe |

MENU cycles through readings → low limit → high limit → current limit → reconnect delay → readings. Separate UP/DOWN presses change voltage by 1 V, current by 0.5 A, and delay by 1 second. Holding a button counts as one press. Settings apply immediately, without a Save action, and return to defaults on restart.

Protection continues on settings screens. Press MENU to return from RECONNECT DELAY to readings. RESET does not restore settings or reboot the ESP32.

## Computer control

Install [MQTT Explorer](https://mqtt-explorer.com/) and configure:

| Field | Value |
|---|---|
| Host | test.mosquitto.org |
| Port | 1883 |
| Protocol | mqtt:// |
| TLS | Off |
| Username/password | Blank |
| Client ID | frontier-pc-4d2848ff, or another unique computer ID |
| Advanced subscription | frontier/vp-4d2848ff/#, QoS 0 |

Remove broad default subscriptions such as # and $SYS/#. The subscription is a topic filter, not the client ID. Start Wokwi and wait for MQTT connected in its Serial Monitor.

In the Publish panel, set Topic to **frontier/vp-4d2848ff/command**, select **raw**, QoS **0**, and **Retain OFF**. Type one command without quotation marks, then click PUBLISH.

| Topic | Purpose |
|---|---|
| frontier/vp-4d2848ff/command | Computer sends commands here |
| frontier/vp-4d2848ff/reply | Device returns command responses |
| frontier/vp-4d2848ff/status | Device publishes JSON telemetry roughly every simulated second |

### Commands

```text
STATUS
SET minimum_voltage maximum_voltage current_limit delay_ms
RESET
```

SET changes protection thresholds, not potentiometer readings. Valid values require finite numbers, minimum voltage at least 50 V, maximum voltage at most 300 V, a gap of at least 10 V, current limit 0.5-20 A, and delay 1000-30000 ms. Invalid settings preserve the previous configuration. Valid changes open the relay and restart qualification without clearing an existing current latch.

| Payload | Purpose |
|---|---|
| SET 190 220 10 3000 | High-voltage trip at a measured 230 V |
| SET 240 260 10 3000 | Low-voltage trip at a measured 230 V |
| SET 190 250 1 3000 | Over-current test if measured current exceeds 1 A |
| SET 190 250 10 5000 | Five-second reconnect delay |
| SET 260 190 10 3000 | Invalid limits: expect rejection |
| SET 190 250 10 3000 | Restore defaults |

After over-current, restore safe readings and send RESET. Check the reply and fresh status to confirm the result. Restoring settings alone does not clear a latch.

## Troubleshooting

| Symptom | Check |
|---|---|
| Unknown class or missing functional during compile | Use sketch.ino in an Arduino project, not main.c |
| PubSubClient.h missing | Add exactly PubSubClient in Library Manager |
| Unexpected end of input | Copy the complete sketch, including setup and loop |
| CURRENT LATCH at low current | Previous over-current is retained; remove fault and request RESET |
| Green LED ON at zero current | Expected: available supply does not imply current draw |
| SET appears ineffective | Check /command topic, raw format, MQTT connection, reply and resulting limits |
| LCD remains on settings | Press and release MENU until readings appear |
| RECONNECT WAIT never clears | Check safe readings, repeated setting changes and sampling gaps |

The [public broker](https://test.mosquitto.org/) is unauthenticated and unencrypted and may be unavailable. Use no sensitive data. Do not retain commands: they could replay on reconnect. QoS 0 delivery may be lost; always verify fresh telemetry.
