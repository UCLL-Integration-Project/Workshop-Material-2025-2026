#include <Arduino.h>

// include libraries
#include <Adafruit_SH1106.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SocketIOclient.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <Wire.h>

// config
#include "config.h"

WiFiClient WiFiclient;
SocketIOclient socketIO;

#define i2C_ADDRESS 0x3C
Adafruit_SH1106 display;
bool displayAvailable = false;

// buffer for the oled message
char messageBuffer[256];

// buton config
#define BUTTON_PIN 4
bool buttonState = false;

// wifi client for https request
HTTPClient http;
const char* request_url[] = {"https://websocket.itip-demo.ucll.cloud/demotext.txt"};

/**
 * @brief helper function to display a message on the OLED
 *
 * @param messageBuffer
 */
void displayMessage(const char* messageBuffer) {
    if (!displayAvailable) {
        return;
    }

    String message = String(messageBuffer);
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(message);
    display.display();
}

/**
 * @brief helper function to check the button state
 *
 */
void checkButton() {
    bool buttonToggle = false;

    // check if the button is pressed and state is changed
    if ((digitalRead(BUTTON_PIN) == LOW) && (buttonState == false)) {
        // button is pressed
        buttonState = true;
        buttonToggle = true;
        Serial.println("Button pressed");
        displayMessage("Button pressed");
    } else if ((digitalRead(BUTTON_PIN) == HIGH) && (buttonState == true)) {
        // button is released
        buttonState = false;
        buttonToggle = true;
        Serial.println("Button released");
        displayMessage("Button released");
    }

    if (buttonToggle) {
        // create JSON object
        JsonDocument payloadObject = DynamicJsonDocument(1024);
        JsonArray dataArray = payloadObject.to<JsonArray>();
        dataArray.add("espUpdate");

        JsonObject eventDataObject = dataArray.add<JsonObject>();
        eventDataObject["buttonState"] = (uint8_t)buttonState;
        eventDataObject["syncword"] = syncword;

        // JSON to String (serialization)
        String output;
        serializeJson(payloadObject, output);
        Serial.printf("[IOc] send event: %s\n", output.c_str());

        // Send event
        socketIO.sendEVENT(output);
    }
}

/**
 * @brief event handler for the socketIO client
 *
 * @param type
 * @param payload
 * @param length
 */
void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case sIOtype_DISCONNECT:
            Serial.printf("[SocketIo] Disconnected!\n");
            break;

        case sIOtype_CONNECT:
            Serial.printf("[SocketIo] Connected to url: %s\n", payload);

            socketIO.send(sIOtype_CONNECT, "/");
            socketIO.sendEVENT("[\"joinRoom\",\"" + syncword + "\"]");  // ugly way to manually serialize the JSON
            break;

        case sIOtype_EVENT:
            Serial.printf("[SocketIo] get event: %s\n", payload);

            // parse the payload into a json object
            JsonDocument payloadObject = DynamicJsonDocument(1024);
            DeserializationError error = deserializeJson(payloadObject, payload, length);

            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
                return;
            }

            String eventName = payloadObject[0];
            Serial.printf("[IOc] event name: %s\n", eventName.c_str());

            if (eventName == "update") {
                JsonObject attributesObject = payloadObject[1].as<JsonObject>();

                // print all attributes
                /*
                for (JsonPair artribute : attributesObject) {
                    const char* key = artribute.key().c_str();
                    JsonVariant value = artribute.value();

                    Serial.printf("Key: %s, Value: %s\n", key, value.as<String>().c_str());
                }
                */

                if (attributesObject.containsKey("buttonState")) {
                    bool buttonState = attributesObject["buttonState"];
                    Serial.printf("[IOc] button state: %s\n", buttonState ? "true" : "false");
                }

                if (attributesObject.containsKey("message")) {
                    String message = attributesObject["message"];
                    Serial.printf("[IOc] message: %s\n", message.c_str());
                    if (message != "null") {
                        displayMessage(message.c_str());
                    }
                }
            }

            break;
    }
}

/**
 * @brief demo https request
 * 
 */
void makeHttpRequest() {
    // make a https request
    http.begin(request_url[0]);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.GET();

    Serial.println();
    Serial.println("[HTTP] request");
    Serial.println();

    // httpCode will be negative on error
    if (httpCode > 0) {
        // file found at server
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println(payload);
        } else {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    Serial.println();
    Serial.println("[HTTP] request done");
    Serial.println();
}

void setup() {
    Serial.begin(115200);

    Serial.println("socketioClient example");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    SPI.begin();

    Wire.begin();

    Wire.beginTransmission(i2C_ADDRESS);
    displayAvailable = (Wire.endTransmission() == 0);

    if (displayAvailable) {
        display.begin(SH1106_SWITCHCAPVCC, i2C_ADDRESS);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);

        displayMessage("oled init");
    } else {
        Serial.println("OLED not detected, continuing without display");
    }

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    String ip = WiFi.localIP().toString();
    Serial.printf("WiFi Connected %s\n", ip.c_str());

    sprintf(messageBuffer, "WiFi connected\n\nIp: %s\n", ip.c_str());
    displayMessage(messageBuffer);

    // server address, port and URL
    socketIO.begin(host, port, "/socket.io/?EIO=4");

    // event handler
    socketIO.onEvent(socketIOEvent);

    // demo https request
    makeHttpRequest();
}

void loop() {
    socketIO.loop();
    checkButton();
}