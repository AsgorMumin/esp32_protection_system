# ESP32 Voltage Protector

A Wokwi simulation of voltage and over-current protection, with local settings buttons and computer control over Wi-Fi/MQTT.

## Documentation and project files

Use the guides below to run the project, understand its design, and repeat the verification steps.

| Document | Contents |
|---|---|
| [Setup, controls, and troubleshooting](project_files/readme.md) | Wokwi setup, MQTT Explorer configuration, commands, and local button operation |
| [System design and block diagram](schematics/block_diagram.md) | Component roles, signal flow, pin mapping, and protection states |
| [Test evidence and verification checklist](test_evidence/readme.md) | Existing screenshots, their scope, and repeatable functional checks |
| [One-page design report](design_document.pdf) | Design, observed results, and limitations; an assisted draft for applicant review |
| [Complete ESP32 firmware](src/sketch.ino) | Arduino sketch with protection, controls, display, and MQTT |
| [Editable circuit diagram](project_files/diagram.json) | Wokwi components and connections |
| [Required libraries](project_files/libraries.txt) | LiquidCrystal I2C and PubSubClient |

## Features

- Disconnects the represented appliance during high voltage, low voltage, or over-current.
- Waits for a healthy interval before reconnecting after startup or voltage recovery.
- Latches over-current until safe readings and an explicit reset.
- Supports settings changes using local buttons and MQTT commands.
- Displays readings and fault state on the LCD and publishes JSON status.
- Validates settings before applying them.

The potentiometers represent processed voltage/current readings. The green LED represents supply availability to an appliance, so it may remain ON at zero measured current.

## Run

Open the [saved Wokwi project](https://wokwi.com/projects/477495719368653825). See the [step-by-step setup guide](project_files/readme.md#recreate-the-project) if rebuilding it from these files.

To recreate it, start an ESP32 Arduino project at https://wokwi.com/projects/new/esp32 . Copy src/sketch.ino into the sketch.ino tab (not main.c), and project_files/diagram.json into the diagram tab. Add LiquidCrystal I2C and PubSubClient using Library Manager or copy project_files/libraries.txt. Start the simulation with readings near 230 V and 2 A. The load connects after 3 seconds of healthy readings.

## Computer control

Connect [MQTT Explorer](https://mqtt-explorer.com/) to test.mosquitto.org, port 1883, TLS off, no credentials. Use a client ID such as frontier-pc-4d2848ff and subscribe to frontier/vp-4d2848ff/#. The [MQTT operating guide](project_files/readme.md#computer-control) explains the exact connection and Publish fields.

Publish raw text, QoS 0, Retain OFF, to frontier/vp-4d2848ff/command:

    STATUS
    SET 190 220 10 3000
    SET 190 250 10 3000
    RESET

SET specifies minimum voltage, maximum voltage, current limit in amperes, and reconnect delay in milliseconds. Replies appear under /reply and live JSON readings/settings under /status. At 230 V, reducing the high limit to 220 V trips the load. Invalid limits such as SET 260 190 10 3000 are rejected.

The public test broker is unauthenticated and unencrypted; anyone can publish to these topics. Its availability is not guaranteed. Do not retain commands, which could be replayed on reconnect. QoS 0 messages may be lost; confirm the live status. This is demonstration communication only. Broker information: https://test.mosquitto.org/ .

## Device controls

MENU cycles through readings, low limit, high limit, current limit, reconnect delay, then readings again. Separate UP/DOWN presses change voltage by 1 V, current by 0.5 A, or delay by 1 second. Changes apply immediately and restart qualification. RESET clears an over-current latch only when both readings are safe. Settings are volatile and return to defaults on restart.

## Wiring

| Function | GPIO |
|---|---:|
| Voltage / current potentiometer signals | 34 / 35 |
| LCD SDA / SCL | 21 / 22 |
| Relay control | 23 |
| MENU / UP / DOWN / RESET | 18 / 19 / 25 / 26 |
| Fault LED | 27 |
| Buzzer | 32 |

Potentiometers use 3.3 V and ground. Simulated LCD/relay use 5 V and ground. The green LED receives power through relay COM/NO and a 330-ohm resistor; its cathode goes to ground. Relay ON=HIGH was checked in the user's Wokwi setup. See the [full circuit](project_files/diagram.json) and [design explanation](schematics/block_diagram.md).

![ESP32 voltage protector block diagram](schematics/block_diagram.png)

## Protection and limitations

Defaults: 190-250 V inclusive, current up to 10 A inclusive, reconnect delay 3000 ms. Voltage faults recover after a healthy interval. Over-current remains latched until reset. Zero current is allowed: the green LED means supply connected, not current flow.

Potentiometers emulate processed readings, not actual AC waveforms. No physical prototype, calibrated sensing, AC RMS measurement, relay contact timing, or fast-surge suppression is validated. Checks occur approximately every 20 ms; worst-case latency is unmeasured. MQTT connections run in a separate task. A sampling gap above 100 ms restarts qualification. Do not connect a physical prototype to mains; use low-voltage stand-ins.

## Evidence and submission

See the [evidence guide](test_evidence/readme.md) for observed evidence and remaining checks. The [one-page report](design_document.pdf) is an assisted factual draft: review and rewrite it in your own words. The earlier Python/DLL prototype and its tests are excluded from this ESP32 submission.

Publish this repository publicly and provide its full HTTPS URL in the assessment form. Upload the demonstration video separately, at most 5 minutes and below the specified size limit. Do not put the video in GitHub.
