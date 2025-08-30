#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#define BOTtoken "yourbottoken"
#define CHAT_ID "yourchatid"
#define MOTION_SENSOR_PIN 15
#define INTERNAL_LED 2

const char* ssid = "yourwifissid";
const char* password = "yourwifipassword";

unsigned long reconnectAttemptTime = 0; // Time of the last WiFi connect attempt
const unsigned long WIFI_CONNECTING_TIMEOUT = 1 * 60 * 1000; // 1 minute

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 10 second.
int botRequestDelay = 10000;
int restarted = 0; // Number of times the ESP32 has restarted
int restarted2 = 0; // Number of times the ESP32 has restarted

// Initialize EEPROM and load restarted2 from EEPROM
void initRestarted2() {
    EEPROM.begin(4); // Allocate 4 bytes for EEPROM
    restarted2 = EEPROM.read(0);
}
unsigned long lastTimeBotRan;

void ledSuccess();
void ledError();
void handleMotionDetected();
void handleNewMessages(int numNewMessages);

void setup() {
    Serial.begin(115200);
    pinMode(MOTION_SENSOR_PIN, INPUT);
    pinMode(INTERNAL_LED, OUTPUT);

    digitalWrite(INTERNAL_LED, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    reconnectAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println(".");
        if (millis() - reconnectAttemptTime > WIFI_CONNECTING_TIMEOUT) { // 2 minutes timeout
            Serial.println("ESP32: Failed to connect to WiFi within " + String((WIFI_CONNECTING_TIMEOUT / 1000) / 60) + " minute(s). Restarting ESP...");
            ledError();
            ESP.restart();
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");

    bot.last_message_received = 0;

    bot.sendMessage(CHAT_ID, "Started!");

    ledSuccess();
}

void loop() {
    if (millis() > lastTimeBotRan + botRequestDelay)  {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

        if(restarted2 >= 20) {
            Serial.println("Stopping ESP32 due to too many restarts...");
            ledError();
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            lastTimeBotRan = millis();
            return;
        }

        while(numNewMessages) {
            Serial.println("got response");
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();
    }

    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, restarting...");
        ledError();
        if(restarted >= 20) {
            WiFi.disconnect();
            delay(1000);
            WiFi.begin(ssid, password);
            if(WiFi.status() == WL_CONNECTED) {
                Serial.println("Reconnected to WiFi");
                ledSuccess();
                restarted = 0; // Reset the restart counter
                return;
            }
        }
        restarted++;
        ESP.restart();
        return;
    }

    if(digitalRead(MOTION_SENSOR_PIN) == HIGH) {
            Serial.println("Motion detected!");
            handleMotionDetected();
    }
}

void ledSuccess() {
    digitalWrite(INTERNAL_LED, HIGH);
    delay(750);
    digitalWrite(INTERNAL_LED, LOW);
    delay(500);
    digitalWrite(INTERNAL_LED, HIGH);
    delay(750);
    digitalWrite(INTERNAL_LED, LOW);
}

void ledError() {
    digitalWrite(INTERNAL_LED, HIGH);
}

void handleMotionDetected() {
    bot.sendMessage(CHAT_ID, "INFO: Motion detected!");
    ledSuccess();

    delay(5000);
    return;
}

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/status") {
      bot.sendMessage(chat_id, "Status: OK", "");
    }
    
    if (text == "/again") {
      bot.sendMessage(chat_id, "Restarting now...", "");
      bot.getUpdates(bot.last_message_received + 1);
        // Increment and save restarted2 to EEPROM so it persists after restart
        restarted2++;
        EEPROM.write(0, restarted2);
        EEPROM.commit();
      delay(1000);
      ESP.restart();
    }
  }
}