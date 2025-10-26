#include "AXS15231B.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include "pins_config.h"
#include "fontM.h"
#include "fontH.h"
#include "fontS.h"
#include "fontT.h"
#include "yt.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <Preferences.h>
// For bitmap encoding: use Image2lcd, 16bit true colour, MSB First, RGB565, don't include head data, be sure to set max image size, save as .h file.

USBHIDKeyboard Keyboard;
Preferences preferences;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
uint8_t ALS_ADDRESS = 0x3B;
uint8_t read_touchpad_cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x8};
int tx=0;
int ty=0;
int cx=-1;
int cy=-1;
int xpos[] = {4,92,180,268};
int ypos[] = {4,43,82,121};
 String btns[4][3] = {{"1","2","3"},{"4","5","6"},{"7","8","9"},{"CLR","0","OK"}};
String pin_buffer="";
String new_pin_buffer = "";
bool touch_held=false;
#define time_out_reset 30000
uint16_t touch_timeout=0;
uint16_t cnt=0;

enum AppState {
    STATE_PIN_ENTRY,
    STATE_PIN_SETUP_NEW,
    STATE_PIN_SETUP_CONFIRM
};
AppState currentState;

//colors
unsigned short col1=0x39C7;
unsigned short col2=0x2945;
unsigned short col3=TFT_ORANGE;
unsigned short col4=TFT_SILVER;
unsigned short cls[4][3] = {{col1,col1,col1},{col1,col1,col1},{col1,col1,col1},{col2,col1,col2}};
unsigned short tcls[4][3] = {{col4,col4,col4},{col4,col4,col4},{col4,col4,col4},{col3,col4,col3}};

void draw()
{
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextFont(1);
    sprite.setTextSize(2);
    sprite.setTextColor(TFT_SILVER, TFT_BLACK);
    switch (currentState) {
        case STATE_PIN_ENTRY:
            sprite.drawString("Enter PIN", 350, 10);
            break;
        case STATE_PIN_SETUP_NEW:
            sprite.drawString("Enter New PIN", 350, 10);
            break;
        case STATE_PIN_SETUP_CONFIRM:
            sprite.drawString("Confirm New PIN", 350, 10);
            break;
    }

    // Draw PIN buffer
    sprite.drawRect(350, 40, 260, 40, TFT_WHITE);
    sprite.setTextFont(1);
    sprite.setTextSize(3);
    sprite.drawString(pin_buffer, 360, 50);

    // Draw keypad
    sprite.setTextFont(1);
    sprite.setTextSize(1);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 3; x++) {
            int x_pos = xpos[x];
            int y_pos = ypos[y];
            sprite.fillRoundRect(x_pos, y_pos, 80, 35, 8, cls[y][x]);
            sprite.setTextColor(tcls[y][x], cls[y][x]);
            sprite.drawCentreString(btns[y][x], x_pos + 40, y_pos + 17, 2);
        }
    }
    sprite.setTextSize(2);

    if (currentState == STATE_PIN_ENTRY) {
        sprite.fillRoundRect(350, 121, 120, 35, 8, col2);
        sprite.setTextColor(col3, col2);
        sprite.setTextSize(1);
        sprite.drawCentreString("CLEAR ALL", 350 + 60, 121 + 17, 2);
        sprite.setTextSize(2);
    }

    lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t*)sprite.getPointer());
}


void IRAM_ATTR handleTouchInterrupt() {
    touch_held = true;
    touch_timeout = 0;
}

void setup() {
    pinMode(TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_INT), handleTouchInterrupt, FALLING);
    sprite.createSprite(640, 180);
    sprite.setSwapBytes(1);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    pinMode(TOUCH_RES, OUTPUT);
    digitalWrite(TOUCH_RES, HIGH);delay(2);
    digitalWrite(TOUCH_RES, LOW);delay(10);
    digitalWrite(TOUCH_RES, HIGH);delay(2);
    Wire.begin(TOUCH_IICSDA, TOUCH_IICSCL);

    preferences.begin("pin-storage", false);
    if (preferences.getBool("setup_pin", false) || !preferences.isKey("pin")) {
        currentState = STATE_PIN_SETUP_NEW;
        preferences.putBool("setup_pin", false); // consume flag
    } else {
        currentState = STATE_PIN_ENTRY;
    }

    axs15231_init();
    draw();

    Keyboard.begin();
    USB.begin();
}

void getTouch()
{
    uint8_t buff[8] = {0};
    Wire.beginTransmission(ALS_ADDRESS);
    Wire.write(read_touchpad_cmd, 8);
    Wire.endTransmission();
    Wire.requestFrom(ALS_ADDRESS, 8);
    if (Wire.available()) {
        Wire.readBytes(buff, sizeof(buff));
    }

    int pointX=-1;
    int pointY=-1;
    int type = 0;

    type = AXS_GET_GESTURE_TYPE(buff);
    pointX = AXS_GET_POINT_X(buff,0);
    pointY = AXS_GET_POINT_Y(buff,0);

    if (pointX > 0 || pointY > 0) {
        tx = map(pointX, 627, 10, 0, 640);
        ty = map(pointY, 180, 0, 0, 180);

        // Check keypad buttons
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 3; x++) {
                int x_pos = xpos[x];
                int y_pos = ypos[y];
                if (tx >= x_pos && tx <= x_pos + 80 && ty >= y_pos && ty <= y_pos + 35) {
                    String key = btns[y][x];
                    if (key == "CLR") {
                        pin_buffer = "";
                    } else if (key == "OK") {
                        switch (currentState) {
                            case STATE_PIN_ENTRY:
                                {
                                    String stored_pin = preferences.getString("pin", "");
                                    if (pin_buffer == stored_pin) {
                                        String pw = "P@ssword123!";
                                        Keyboard.print(pw);
                                    }
                                    pin_buffer = "";
                                }
                                break;
                            case STATE_PIN_SETUP_NEW:
                                if(pin_buffer.length() > 0){
                                    new_pin_buffer = pin_buffer;
                                    pin_buffer = "";
                                    currentState = STATE_PIN_SETUP_CONFIRM;
                                }
                                break;
                            case STATE_PIN_SETUP_CONFIRM:
                                if (pin_buffer == new_pin_buffer) {
                                    preferences.putString("pin", pin_buffer);
                                    currentState = STATE_PIN_ENTRY;
                                    pin_buffer = "";
                                    new_pin_buffer = "";
                                } else {
                                    pin_buffer = "";
                                }
                                draw();
                                break;
                        }
                    } else {
                        pin_buffer += key;
                    }
                    draw();
                    return;
                }
            }
        }
        
        if (currentState == STATE_PIN_ENTRY && tx >= 350 && tx <= 470 && ty >= 121 && ty <= 156) { // CLEAR ALL button
            preferences.putBool("setup_pin", true);
            ESP.restart();
            return;
        }
    }
}


void loop()
{
  if(touch_held)
    {
      getTouch();
      touch_held = false;
      delay(100);
    }
}