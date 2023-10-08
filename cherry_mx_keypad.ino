/*
  2x3 Cherry MX Keypad

  Uses 6 switches to send media commands.
  Each switch has an led, all of them can be controlled separately.

  Setup:
  Connect LED_PIN to the first WS2812b led
  Connect FIRST_BUTTON_PIN to the first cherry switch, then connect all other switches
  to the FIRST_BUTTON_PIN + i pins, e.g: FIRST_BUTTON_PIN: D2 = SW1: D2, SW2: D3, SW3: D4,..
  Color, led mode and brightness can be switched by pressing 2/3 keys at the same time, change pins if needed.
  
  created by Marius Gebhard
  https://m-gebhard.dev
  https://github.com/m-gebhard

*/

#include <EEPROM.h>
#include <FastLED.h>
#include "HID-Project.h"

#define LED_PIN 8
#define NUM_BUTTONS 6
#define FIRST_BUTTON_PIN 2

#define STORE_MODE_ADRESS 0
#define STORE_COLOR_ADRESS 1
#define STORE_BRIGHTNESS_ADRESS 2

#define LEDS_UPDATE_TIMEOUT 600
#define FASTLED_INTERRUPT_RETRY_COUNT 0

CRGB leds[NUM_BUTTONS];

// Prototypes
bool noOtherKeyPressed(int);
void applyColor(String, int = -1, bool = false);

uint8_t hueWheelIndex = 0;
int currentLedMode = 0, currentColor = 0, currentBrightness = 0;
long updateLedsTimeout = -1;
bool isChangingColor = false, isChangingBrightness = false, hasColorUpdated = false;

// Led settings
int availableBrightnesses[] = { 2, 7, 20, 40, 70, 0 };
String availableColors[] = { "#ff0000", "#ff2814", "#00ff00", "#b1ff14", "#33ccff", "#0000ff", "#6600cc", "#ffffff", "#000000" };

// Buttons
int buttonActions[] = { MEDIA_PREVIOUS, MEDIA_PLAY_PAUSE, MEDIA_NEXT, MEDIA_VOLUME_DOWN, MEDIA_VOLUME_MUTE, MEDIA_VOLUME_UP };
int buttonTicks[NUM_BUTTONS];

void setup()
{
    Serial.begin(9600);
    Consumer.begin();
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_BUTTONS);

    for (int i = FIRST_BUTTON_PIN; i < (FIRST_BUTTON_PIN + NUM_BUTTONS); i++) {
        pinMode(i, INPUT);
        buttonTicks[i - FIRST_BUTTON_PIN] = -1;
    }

    int brightness = EEPROM.read(STORE_BRIGHTNESS_ADRESS);

    LEDS.setBrightness(availableBrightnesses[brightness]);

    currentColor = EEPROM.read(STORE_COLOR_ADRESS);
    currentLedMode = EEPROM.read(STORE_MODE_ADRESS);
    currentBrightness = brightness;
}

void loop()
{
    // Update leds
    switch (currentLedMode) {
        case 0:
            applyColor(availableColors[currentColor]);
            break;
        case 1:
            doGradient();
            break;
        case 2:
            doMover();
            break;
    }

    // Check all keys, if a key is pressed, count 25 ticks,
    // then execute buttonAction if no other key has been pressed during the ticks
    if (!isChangingColor && !isChangingBrightness) {
        for (int i = FIRST_BUTTON_PIN; i < (FIRST_BUTTON_PIN + NUM_BUTTONS); i++) {
            if (isNoOtherKeyPressed(i)) {
                if (buttonTicks[i - FIRST_BUTTON_PIN] > 25) {
                    Consumer.write(buttonActions[i - FIRST_BUTTON_PIN]);
                    buttonTicks[i - FIRST_BUTTON_PIN] = -1;

                    applyColor(availableColors[currentColor], i - FIRST_BUTTON_PIN, true);
                    delay(100);
                }
                else {
                    buttonTicks[i - FIRST_BUTTON_PIN]++;
                }
            }
        }
    }

    // Color switching
    if (currentLedMode == 0 && digitalRead(5) == HIGH && digitalRead(6) == HIGH && digitalRead(7) == HIGH) {
        resetButonStates();

        if (updateLedsTimeout == -1) {
            isChangingColor = true;
            currentColor++;

            if (currentColor > 8) {
                currentColor = 0;
            }
            EEPROM.write(STORE_COLOR_ADRESS, currentColor);

            applyColor(availableColors[currentColor]);
            updateLedsTimeout = millis();
        }
        else {
            if (millis() - updateLedsTimeout > LEDS_UPDATE_TIMEOUT) {
                updateLedsTimeout = -1;
            }
        }
    }
    // Brightness switching
    else if (digitalRead(2) == HIGH && digitalRead(3) == HIGH && digitalRead(4) == HIGH) {
        resetButonStates();

        if (updateLedsTimeout == -1) {
            isChangingBrightness = true;
            currentBrightness++;

            if (currentBrightness > 5) {
                currentBrightness = 0;
            }
            EEPROM.write(STORE_BRIGHTNESS_ADRESS, currentBrightness);

            LEDS.setBrightness(availableBrightnesses[currentBrightness]);
            updateLedsTimeout = millis();
        }
        else {
            if (millis() - updateLedsTimeout > LEDS_UPDATE_TIMEOUT) {
                updateLedsTimeout = -1;
            }
        }
    }
    // LED Mode switching
    else if (digitalRead(4) == HIGH && digitalRead(7) == HIGH) {
        resetButonStates();

        if (updateLedsTimeout == -1) {
            isChangingColor = true;
            currentLedMode++;

            if (currentLedMode > 2) {
                currentLedMode = 0;
            }
            EEPROM.write(STORE_MODE_ADRESS, currentLedMode);

            updateLedsTimeout = millis();
        }
        else {
            if (millis() - updateLedsTimeout > LEDS_UPDATE_TIMEOUT) {
                updateLedsTimeout = -1;
            }
        }
    }
    else {
        isChangingColor = false;
        isChangingBrightness = false;
    }
}

// Re-set all button tick counters
void resetButonStates()
{
    for (int i = FIRST_BUTTON_PIN; i < (FIRST_BUTTON_PIN + NUM_BUTTONS); i++) {
        buttonTicks[i - FIRST_BUTTON_PIN] = -1;
    }
}

// Apply hex color ("#ffffff") to led or all leds, optionally invert color
void applyColor(String color, int led = -1, bool invert = false)
{
    if (currentLedMode != 0)
        return;

    long number = strtol(&color[1], NULL, 16);
    number = invert ? ~number : number;

    long r = number >> 16;
    long g = number >> 8 & 0xFF;
    long b = number & 0xFF;

    if (led == -1) {
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (digitalRead(i + FIRST_BUTTON_PIN) == LOW || isChangingColor || isChangingBrightness) {
                leds[i].r = r;
                leds[i].g = g;
                leds[i].b = b;
            }
        }
    }
    else {
        leds[led].r = r;
        leds[led].g = g;
        leds[led].b = b;
    }

    FastLED.show();
}

// Check if no other than onlyKey is pressed
bool isNoOtherKeyPressed(int onlyKey)
{
    for (int i = FIRST_BUTTON_PIN; i < (FIRST_BUTTON_PIN + NUM_BUTTONS); i++) {
        if (i != onlyKey) {
            if (digitalRead(i) == HIGH) {
                return false;
            }
        }
    }
    return (digitalRead(onlyKey) == HIGH);
}

// Gradient animation
void doGradient()
{
    int startHue = beatsin8(3, 0, 255);
    int endHue = beatsin8(5, 0, 255);

    if (startHue < endHue) {
        fill_gradient(leds, NUM_BUTTONS, CHSV(startHue, 255, 255), CHSV(endHue, 255, 255), FORWARD_HUES);
    }
    else {
        fill_gradient(leds, NUM_BUTTONS, CHSV(startHue, 255, 255), CHSV(endHue, 255, 255), BACKWARD_HUES);
    }
    FastLED.show();
}

// Mover animation
void doMover()
{
    fadeToBlackBy(leds, NUM_BUTTONS, 12);

    int pos = beatsin16(13, 0, NUM_BUTTONS - 1);

    leds[pos] = CHSV(hueWheelIndex, 255, 255);
    FastLED.show();

    if (pos == 0 || pos == NUM_BUTTONS - 1) {
        if (!hasColorUpdated) {
            hueWheelIndex += 10;
            hasColorUpdated = true;
        }
    }
    else {
        hasColorUpdated = false;
    }
}
