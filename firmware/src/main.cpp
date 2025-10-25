#include "AXS15231B.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include "pins_config.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/aes.h"

// Encryption - IMPORTANT: This is a default key. For any real-world use,
// you MUST generate a new, random 16-byte key and keep it secret.
// Exposing this key compromises all stored data.
byte aes_key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
byte aes_iv[16]; // Initialization Vector

// PIN and password storage
#define STORAGE_NAMESPACE "vault"
const char* pin_key = "pin";
const char* password_key = "password";

// State management
enum AppState {
  LOCKED,
  UNLOCKED,
  PIN_SETUP,
  PASSWORD_SETUP
};
AppState currentState = LOCKED;

// PIN entry
String enteredPin = "";
const int pinLength = 4;

USBHIDKeyboard Keyboard;

TFT_eSPI tft = TFT_eSPI();

// Forward declarations
void setupPin();
void checkPin();
void typePassword();
bool touch_held = false;
uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x8};

void IRAM_ATTR handleTouchInterrupt() {
    touch_held = true;
}
void encrypt(const String& plainText, byte* output, byte* iv);
void displayMessage(const String& message, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color);
    tft.setTextSize(2);
    tft.setCursor(20, 50);
    tft.print(message);
}

void drawKeypad() {
    tft.fillScreen(TFT_BLACK);
    int buttonWidth = 80;
    int buttonHeight = 50;
    int startX = 40;
    int startY = 60;
    int spacingX = 15;
    int spacingY = 15;

    // Draw PIN entry field
    tft.drawRect(startX, 10, 3 * buttonWidth + 2 * spacingX, 40, TFT_WHITE);
    tft.setTextSize(3);
    tft.setCursor(startX + 10, 20);
    tft.print(enteredPin);


    // Draw number buttons 1-9
    for (int i = 1; i <= 9; i++) {
        int col = (i - 1) % 3;
        int row = (i - 1) / 3;
        int x = startX + col * (buttonWidth + spacingX);
        int y = startY + row * (buttonHeight + spacingY);
        tft.fillRoundRect(x, y, buttonWidth, buttonHeight, 8, TFT_BLUE);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(x + 35, y + 15);
        tft.print(i);
    }

    int y_row4 = startY + 3 * (buttonHeight + spacingY);
    // Clear button
    tft.fillRoundRect(startX, y_row4, buttonWidth, buttonHeight, 8, TFT_RED);
    tft.setCursor(startX + 20, y_row4 + 15);
    tft.print("CLR");

    // 0 button
    int x_zero = startX + buttonWidth + spacingX;
    tft.fillRoundRect(x_zero, y_row4, buttonWidth, buttonHeight, 8, TFT_BLUE);
    tft.setCursor(x_zero + 35, y_row4 + 15);
    tft.print("0");

    // Enter button
    int x_ok = startX + 2 * (buttonWidth + spacingX);
    tft.fillRoundRect(x_ok, y_row4, buttonWidth, buttonHeight, 8, TFT_GREEN);
    tft.setCursor(x_ok + 25, y_row4 + 15);
    tft.print("OK");
}
String decrypt(byte* cipherText, size_t len, byte* iv);
void displayMessage(const String& message, uint16_t color);
void drawKeypad();
void handleTouch();


void setup() {
    pinMode(TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_INT), handleTouchInterrupt, FALLING);
    
    // Initialize display
    Wire.begin(TOUCH_IICSDA, TOUCH_IICSCL);
    axs15231_init();
    tft.init();
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // USB HID setup
    Keyboard.begin();
    USB.begin();

  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Check if PIN is set
  nvs_handle_t my_handle;
  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    displayMessage("NVS Error!", TFT_RED);
    return;
  }

  size_t required_size = 0;
  err = nvs_get_blob(my_handle, pin_key, NULL, &required_size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    currentState = PIN_SETUP;
    displayMessage("Setup a new PIN", TFT_CYAN);
    drawKeypad();
  } else {
    displayMessage("Enter PIN", TFT_CYAN);
    drawKeypad();
  }
  nvs_close(my_handle);
}

void loop() {
  if(touch_held)
    {
      handleTouch();
      drawKeypad();
      touch_held = false;
      delay(100);
    }

  // Handle serial commands for updating credentials
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("UPDATE_PASS:")) {
      String new_pass = command.substring(12);
      nvs_handle_t my_handle;
      nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);

      byte encrypted_pass[128]; // Max password length
      byte iv[16];
      encrypt(new_pass, encrypted_pass, iv);

      byte pass_to_store[128 + 16];
      memcpy(pass_to_store, iv, 16);
      memcpy(pass_to_store + 16, encrypted_pass, sizeof(encrypted_pass));

      nvs_set_blob(my_handle, password_key, pass_to_store, sizeof(pass_to_store));
      nvs_commit(my_handle);
      nvs_close(my_handle);
      Serial.println("OK: Password updated");
    }
    if (command.startsWith("UPDATE_PIN:")) {
      String new_pin = command.substring(11);
      if (new_pin.length() == pinLength && new_pin.toInt() >= 0) {
        nvs_handle_t my_handle;
        nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);

        byte encrypted_pin[32];
        byte iv[16];
        encrypt(new_pin, encrypted_pin, iv);

        byte pin_to_store[32 + 16];
        memcpy(pin_to_store, iv, 16);
        memcpy(pin_to_store + 16, encrypted_pin, sizeof(encrypted_pin));

        nvs_set_blob(my_handle, pin_key, pin_to_store, sizeof(pin_to_store));
        nvs_commit(my_handle);
        nvs_close(my_handle);
        Serial.println("OK: PIN updated");
      } else {
        Serial.println("ERROR: PIN must be " + String(pinLength) + " digits");
      }
    }
  }

  delay(50); // General delay to prevent busy-waiting
}



void handleTouch() {
    uint8_t buff[20] = {0};
    Wire.beginTransmission(0x3B);
    Wire.write(read_touchpad_cmd, 8);
    Wire.endTransmission();
    Wire.requestFrom(0x3B, 8);
    while (!Wire.available());
    Wire.readBytes(buff, 8);

    int pointX=-1;
    int pointY=-1;

    pointX = AXS_GET_POINT_X(buff,0);
    pointY = AXS_GET_POINT_Y(buff,0);

    if (pointX > 0 || pointY > 0) {
        int tx = map(pointX, 627, 10, 0, 640);
        int ty = map(pointY, 180, 0, 0, 180);

        int buttonWidth = 80;
        int buttonHeight = 50;
        int startX = 40;
        int startY = 60;
        int spacingX = 15;
        int spacingY = 15;

        // Check number buttons 1-9
        for (int i = 1; i <= 9; i++) {
            int col = (i - 1) % 3;
            int row = (i - 1) / 3;
            int x = startX + col * (buttonWidth + spacingX);
            int y = startY + row * (buttonHeight + spacingY);
            if (tx > x && tx < x + buttonWidth && ty > y && ty < y + buttonHeight) {
                if (enteredPin.length() < pinLength) enteredPin += String(i);
                return;
            }
        }

        int y_row4 = startY + 3 * (buttonHeight + spacingY);
        // Clear button
        if (tx > startX && tx < startX + buttonWidth && ty > y_row4 && ty < y_row4 + buttonHeight) {
            enteredPin = "";
            return;
        }
        // 0 button
        int x_zero = startX + buttonWidth + spacingX;
        if (tx > x_zero && tx < x_zero + buttonWidth && ty > y_row4 && ty < y_row4 + buttonHeight) {
            if (enteredPin.length() < pinLength) enteredPin += "0";
            return;
        }
        // Enter button
        int x_ok = startX + 2 * (buttonWidth + spacingX);
        if (tx > x_ok && tx < x_ok + buttonWidth && ty > y_row4 && ty < y_row4 + buttonHeight) {
            if (currentState == PIN_SETUP) {
                setupPin();
            } else {
                checkPin();
            }
            return;
        }
    }
}

void setupPin() {
  if (enteredPin.length() != pinLength) {
    displayMessage("PIN must be " + String(pinLength) + " digits!", TFT_RED);
    enteredPin = "";
    delay(2000);
    drawKeypad();
    return;
  }

  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
      displayMessage("NVS Write Error!", TFT_RED);
      return;
  }

  // Encrypt and store PIN
  byte encrypted_pin[16]; // AES128 encrypts to 16 bytes
  byte iv[16];
  encrypt(enteredPin, encrypted_pin, iv);

  byte pin_to_store[32]; // 16 for IV + 16 for data
  memcpy(pin_to_store, iv, 16);
  memcpy(pin_to_store + 16, encrypted_pin, 16);

  err = nvs_set_blob(my_handle, pin_key, pin_to_store, sizeof(pin_to_store));
  nvs_commit(my_handle);
  nvs_close(my_handle);

  if (err == ESP_OK) {
    displayMessage("PIN Set! Enter PIN to unlock.", TFT_GREEN);
    currentState = LOCKED;
    enteredPin = "";
    delay(2000);
    drawKeypad();
  } else {
    displayMessage("Failed to save PIN!", TFT_RED);
    delay(2000);
    drawKeypad();
  }
}

void checkPin() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        displayMessage("NVS Read Error!", TFT_RED);
        return;
    }

    size_t required_size = 32; // IV + PIN
    byte stored_data[required_size];
    err = nvs_get_blob(my_handle, pin_key, stored_data, &required_size);
    nvs_close(my_handle);

    if (err != ESP_OK) {
        currentState = PIN_SETUP;
        displayMessage("No PIN set. Please set one.", TFT_CYAN);
        delay(2000);
        drawKeypad();
        return;
    }

    byte iv[16];
    memcpy(iv, stored_data, 16);
    String storedPin_decrypted = decrypt(stored_data + 16, 16, iv);

    if (enteredPin.equals(storedPin_decrypted)) {
        currentState = UNLOCKED;
        displayMessage("Unlocked! Typing password...", TFT_GREEN);
        typePassword();
        delay(2000);
        // Relock device
        currentState = LOCKED;
        enteredPin = "";
        drawKeypad();
    } else {
        displayMessage("Incorrect PIN!", TFT_RED);
        enteredPin = "";
        delay(1000);
        tft.fillRect(10, 35, 300, 20, TFT_BLACK); // Clear incorrect pin message
    }
}

void typePassword() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        displayMessage("NVS Read Error!", TFT_RED);
        return;
    }

    size_t required_size = 128 + 16; // IV + Password
    byte stored_data[required_size];
    err = nvs_get_blob(my_handle, password_key, stored_data, &required_size);
    nvs_close(my_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        displayMessage("No password set!", TFT_YELLOW);
        return;
    } else if (err != ESP_OK) {
        displayMessage("Read failed!", TFT_RED);
        return;
    }

    byte iv[16];
    memcpy(iv, stored_data, 16);
    String password = decrypt(stored_data + 16, 128, iv);

    Keyboard.print(password);
}


void encrypt(const String& plainText, byte* output, byte* iv) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Generate a random IV
    for(int i=0; i<16; i++) {
        iv[i] = random(256);
    }

    // PKCS7 padding
    int plainTextLen = plainText.length();
    int paddedLen = plainTextLen + (16 - (plainTextLen % 16));
    byte paddedText[paddedLen];
    memcpy(paddedText, plainText.c_str(), plainTextLen);
    byte padValue = 16 - (plainTextLen % 16);
    for (int i = plainTextLen; i < paddedLen; i++) {
        paddedText[i] = padValue;
    }

    mbedtls_aes_setkey_enc(&aes, aes_key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, paddedText, output);
    mbedtls_aes_free(&aes);
}

String decrypt(byte* cipherText, size_t len, byte* iv) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    byte decrypted[len];

    mbedtls_aes_setkey_dec(&aes, aes_key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, cipherText, decrypted);
    mbedtls_aes_free(&aes);

    // Remove PKCS7 padding
    char pad = decrypted[len - 1];
    if (pad > 16 || pad == 0) {
        // Invalid padding
        return "";
    }
    int plainTextLen = len - pad;

    char plainText[plainTextLen + 1];
    memcpy(plainText, decrypted, plainTextLen);
    plainText[plainTextLen] = '\0';

    return String(plainText);
}
