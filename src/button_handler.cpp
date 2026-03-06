#include "button_handler.h"

ButtonHandler button;

void ButtonHandler::begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    stableState = digitalRead(BUTTON_PIN);
    lastReading = stableState;
    lastChange = millis();
    Serial.println("Button handler initialized");
}

void ButtonHandler::update() {
    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastReading) {
        lastChange = millis();
    }
    lastReading = reading;

    if ((millis() - lastChange) >= BUTTON_DEBOUNCE_MS) {
        if (reading != stableState) {
            stableState = reading;
            // Falling edge: button pressed (active LOW with INPUT_PULLUP)
            if (stableState == LOW) {
                pressed = true;
            }
        }
    }
}

bool ButtonHandler::wasPressed() {
    if (pressed) {
        pressed = false;
        return true;
    }
    return false;
}
