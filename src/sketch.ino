#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int VOLTAGE_PIN = 34;
const int CURRENT_PIN = 35;
const int RELAY_PIN = 23;
const int RED_LED = 27;
const int BUZZER_PIN = 32;

// HIGH closes the relay in this Wokwi circuit.
const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;

// MENU, UP, DOWN, RESET
const int BUTTONS[] = {18, 19, 25, 26};

const char* BASE_TOPIC = "frontier/vp-4d2848ff";
const char* MQTT_HOST = "test.mosquitto.org";

char commandTopic[96];
char statusTopic[96];
char replyTopic[96];

float lowLimit = 190.0f;
float highLimit = 250.0f;
float currentLimit = 10.0f;
unsigned long reconnectDelay = 3000;

float voltage = 0;
float current = 0;

bool relayOn = false;
bool overCurrentLatched = false;
bool waitingToReconnect = false;
bool alarmOn = false;

unsigned long healthySince = 0;
unsigned long lastSample = 0;
unsigned long lastDisplay = 0;
unsigned long lastTelemetry = 0;

int menu = 0;

int lastButtonReading[] = {HIGH, HIGH, HIGH, HIGH};
int buttonState[] = {HIGH, HIGH, HIGH, HIGH};
unsigned long buttonChangedAt[] = {0, 0, 0, 0};

enum State {
  LOW_VOLTAGE,
  HIGH_VOLTAGE,
  OVER_CURRENT,
  WAITING,
  CONNECTED
};

State state = WAITING;

struct Command {
  char text[96];
};

struct Message {
  char text[320];
};

QueueHandle_t commands;
QueueHandle_t replies;
QueueHandle_t telemetry;

void buildStatusMessage(Message &message);
void handleCommand(const Command &command);

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

void restartReconnectTimer() {
  setRelay(false);
  waitingToReconnect = false;
}

// The two knobs stand in for voltage and current sensors.
void readSensors() {
  voltage = analogRead(VOLTAGE_PIN) * 300.0f / 4095.0f;
  current = analogRead(CURRENT_PIN) * 20.0f / 4095.0f;
}

const char* stateText() {
  switch (state) {
    case LOW_VOLTAGE:
      return "LOW VOLTAGE";

    case HIGH_VOLTAGE:
      return "HIGH VOLTAGE";

    case OVER_CURRENT:
      return "CURRENT LATCH";

    case WAITING:
      return "RECONNECT WAIT";

    case CONNECTED:
      return "LOAD ON";
  }

  return "UNKNOWN";
}

void updateProtection(unsigned long now) {
  if (current > currentLimit) {
    overCurrentLatched = true;
  }

  if (overCurrentLatched) {
    state = OVER_CURRENT;
    restartReconnectTimer();
  } else if (voltage < lowLimit) {
    state = LOW_VOLTAGE;
    restartReconnectTimer();
  } else if (voltage > highLimit) {
    state = HIGH_VOLTAGE;
    restartReconnectTimer();
  } else {
    // Start a fresh delay after readings return to the allowed range.
    if (!waitingToReconnect) {
      waitingToReconnect = true;
      healthySince = now;
    }

    if (now - healthySince >= reconnectDelay) {
      state = CONNECTED;
      setRelay(true);
    } else {
      state = WAITING;
      setRelay(false);
    }
  }

  bool fault =
      state == LOW_VOLTAGE ||
      state == HIGH_VOLTAGE ||
      state == OVER_CURRENT;

  digitalWrite(RED_LED, fault ? HIGH : LOW);

  if (fault != alarmOn) {
    alarmOn = fault;

    if (fault) {
      tone(BUZZER_PIN, 2000);
    } else {
      noTone(BUZZER_PIN);
    }
  }
}

bool applySettings(float low, float high, float amps, long delayMs) {
  if (!isfinite(low) ||
      !isfinite(high) ||
      !isfinite(amps) ||
      low < 50 ||
      high > 300 ||
      high - low < 10 ||
      amps < 0.5f ||
      amps > 20 ||
      delayMs < 1000 ||
      delayMs > 30000) {
    return false;
  }

  lowLimit = low;
  highLimit = high;
  currentLimit = amps;
  reconnectDelay = (unsigned long)delayMs;

  restartReconnectTimer();
  return true;
}

bool resetLatch() {
  readSensors();

  if (current > currentLimit || voltage < lowLimit || voltage > highLimit) {
    return false;
  }

  overCurrentLatched = false;
  restartReconnectTimer();
  return true;
}

void adjustSetting(int direction) {
  float low = lowLimit;
  float high = highLimit;
  float amps = currentLimit;
  long wait = (long)reconnectDelay;

  if (menu == 1) {
    low += direction;
  } else if (menu == 2) {
    high += direction;
  } else if (menu == 3) {
    amps += direction * 0.5f;
  } else if (menu == 4) {
    wait += direction * 1000L;
  } else {
    return;
  }

  bool accepted = applySettings(low, high, amps, wait);

  Serial.println(accepted ? "Setting applied" : "Setting rejected");
}

void buttonPressed(int index) {
  if (index == 0) {
    menu = (menu + 1) % 5;
    Serial.printf("MENU page %d\n", menu);
  } else if (index == 1) {
    adjustSetting(1);
  } else if (index == 2) {
    adjustSetting(-1);
  } else {
    Serial.println(resetLatch() ? "RESET accepted" : "RESET rejected: remove fault first");
  }
}

void buildStatusMessage(Message &message) {
  snprintf(
      message.text,
      sizeof(message.text),
      "{\"uptime_ms\":%lu,"
      "\"voltage\":%.1f,"
      "\"current\":%.1f,"
      "\"relay\":%s,"
      "\"state\":\"%s\","
      "\"low\":%.1f,"
      "\"high\":%.1f,"
      "\"current_limit\":%.1f,"
      "\"delay_ms\":%lu}",
      millis(),
      voltage,
      current,
      relayOn ? "true" : "false",
      stateText(),
      lowLimit,
      highLimit,
      currentLimit,
      reconnectDelay
  );
}

void handleCommand(const Command &command) {
  Message reply = {};

  float low, high, amps;
  long delayMs;
  char extra;

  if (strcmp(command.text, "STATUS") == 0) {
    buildStatusMessage(reply);
  } else if (strcmp(command.text, "RESET") == 0) {
    bool accepted = resetLatch();

    snprintf(
        reply.text,
        sizeof(reply.text),
        "%s",
        accepted
            ? "OK RESET: reconnect wait"
            : "ERROR RESET: remove fault first"
    );
  } else if (
      sscanf(
          command.text,
          "SET %f %f %f %ld %c",
          &low, &high, &amps, &delayMs, &extra
      ) == 4
  ) {
    bool accepted = applySettings(low, high, amps, delayMs);

    snprintf(
        reply.text,
        sizeof(reply.text),
        "%s",
        accepted
            ? "OK SET: settings applied"
            : "ERROR SET: low>=50, high<=300, gap>=10, "
              "amps=0.5..20, delay=1000..30000"
    );
  } else {
    snprintf(
        reply.text,
        sizeof(reply.text),
        "ERROR: use STATUS, RESET, "
        "or SET low high amps delay_ms"
    );
  }

  readSensors();
  updateProtection(millis());

  Serial.printf("PC command: %s | %s\n", command.text, reply.text);

  if (xQueueSend(replies, &reply, 0) != pdTRUE) {
    Serial.println("Reply queue full; use STATUS to confirm");
  }

  Message currentStatus = {};
  buildStatusMessage(currentStatus);
  xQueueOverwrite(telemetry, &currentStatus);
}

// Full LCD redraws were delaying the sensor checks; write only changed characters.
void printRow(int row, const char* text) {
  static char previous[2][16] = {};

  char next[16];
  memset(next, ' ', sizeof(next));

  size_t length = strlen(text);

  if (length > sizeof(next)) {
    length = sizeof(next);
  }

  memcpy(next, text, length);

  for (int column = 0; column < 16; column++) {
    if (next[column] != previous[row][column]) {
      lcd.setCursor(column, row);
      lcd.write((uint8_t)next[column]);
      previous[row][column] = next[column];
    }
  }
}

void updateDisplay() {
  char line[17];

  if (menu == 0) {
    snprintf(line, sizeof(line), "V:%5.1f I:%4.1f", voltage, current);

    printRow(0, line);
    printRow(1, stateText());
  } else {
    if (menu == 1) {
      printRow(0, "LOW LIMIT");

      snprintf(line, sizeof(line), "%.0f V", lowLimit);
    } else if (menu == 2) {
      printRow(0, "HIGH LIMIT");

      snprintf(line, sizeof(line), "%.0f V", highLimit);
    } else if (menu == 3) {
      printRow(0, "CURRENT LIMIT");

      snprintf(line, sizeof(line), "%.1f A", currentLimit);
    } else {
      printRow(0, "RECONNECT DELAY");

      snprintf(line, sizeof(line), "%lu seconds", reconnectDelay / 1000);
    }

    printRow(1, line);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, commandTopic) != 0) {
    return;
  }

  Command command = {};

  // Reject oversized or malformed payloads.
  if (length == 0 || length >= sizeof(command.text) || memchr(payload, 0, length)) {
    return;
  }

  memcpy(command.text, payload, length);

  if (xQueueSend(commands, &command, 0) != pdTRUE) {
    Serial.println("Command queue full; retry later");
  }
}

void networkTask(void*) {
  WiFiClient socket;
  PubSubClient mqtt(socket);

  mqtt.setServer(MQTT_HOST, 1883);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(2);

  char clientId[64];

  snprintf(clientId, sizeof(clientId), "vp-4d2848ff-%08lx", (unsigned long)esp_random());

  unsigned long lastAttempt = millis() - 5000;
  bool wifiReported = false;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!wifiReported) {
        Serial.println("Wi-Fi connected");
        wifiReported = true;
      }

      if (!mqtt.connected() && millis() - lastAttempt >= 5000) {
        lastAttempt = millis();

        if (mqtt.connect(clientId)) {
          if (mqtt.subscribe(commandTopic)) {
            Serial.println("MQTT connected; command subscription requested");
          } else {
            Serial.println("MQTT subscription failed");
            mqtt.disconnect();
          }
        } else {
          Serial.printf("MQTT retry; error %d\n", mqtt.state());
        }
      }

      if (mqtt.connected()) {
        mqtt.loop();

        Message message = {};

        if (xQueueReceive(replies, &message, 0) == pdTRUE) {
          mqtt.publish(replyTopic, message.text, false);
        }

        if (xQueueReceive(telemetry, &message, 0) == pdTRUE) {
          mqtt.publish(statusTopic, message.text, false);
        }
      }
    } else {
      wifiReported = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setRelay(false);
  digitalWrite(RED_LED, LOW);

  for (int i = 0; i < 4; i++) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
  }

  analogReadResolution(12);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  snprintf(commandTopic, sizeof(commandTopic), "%s/command", BASE_TOPIC);

  snprintf(statusTopic, sizeof(statusTopic), "%s/status", BASE_TOPIC);

  snprintf(replyTopic, sizeof(replyTopic), "%s/reply", BASE_TOPIC);

  commands = xQueueCreate(8, sizeof(Command));
  replies = xQueueCreate(8, sizeof(Message));
  telemetry = xQueueCreate(1, sizeof(Message));

  if (!commands || !replies || !telemetry) {
    printRow(0, "QUEUE FAILURE");

    while (true) {
      setRelay(false);
      delay(1000);
    }
  }

  readSensors();
  updateProtection(millis());
  updateDisplay();

  WiFi.begin("Wokwi-GUEST", "", 6);

  // Broker connections can be slow, so they run in a separate task.
  if (xTaskCreatePinnedToCore(networkTask, "mqtt", 6144, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("Network task failed; local protection remains active");
  }

  Serial.printf("Commands: %s\nStatus: %s\nReplies: %s\n", commandTopic, statusTopic, replyTopic);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample >= 20) {
    // A long sampling gap restarts healthy qualification.
    if (now - lastSample > 100) {
      restartReconnectTimer();
    }

    lastSample = now;
    readSensors();
    updateProtection(now);
  }

  // Debounce buttons.
  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(BUTTONS[i]);

    if (reading != lastButtonReading[i]) {
      lastButtonReading[i] = reading;
      buttonChangedAt[i] = now;
    }

    if (now - buttonChangedAt[i] >= 30 && reading != buttonState[i]) {
      buttonState[i] = reading;

      if (reading == LOW) {
        buttonPressed(i);
        readSensors();
        updateProtection(millis());
      }
    }
  }

  // Process at most one remote command per iteration.
  Command command = {};

  if (xQueueReceive(commands, &command, 0) == pdTRUE) {
    handleCommand(command);
  }

  if (now - lastDisplay >= 250) {
    lastDisplay = now;
    updateDisplay();
  }

  if (now - lastTelemetry >= 1000) {
    lastTelemetry = now;

    Message message = {};
    buildStatusMessage(message);
    xQueueOverwrite(telemetry, &message);

    Serial.println(message.text);
  }

  delay(1);
}
