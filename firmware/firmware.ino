#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <CST816S.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Crypto.h>
#include <AES.h>
#include <nvs_flash.h>
#include <nvs.h>

// Display
TFT_eSPI tft = TFT_eSPI();

// Touch
#define I2C_SDA 6
#define I2C_SCL 5
#define RST_N_PIN 7
#define INT_N_PIN 8
CST816S touch(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);

// USB Keyboard
USBHIDKeyboard keyboard;

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

// Forward declarations
void drawKeypad();
void handleTouch();
void setupPin();
void checkPin();
void typePassword();
void encrypt(const String& plainText, byte* output, byte* iv);
String decrypt(byte* cipherText, size_t len, byte* iv);
void displayMessage(const String& message, uint16_t color);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Password Vault...");

  // Initialize display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  // Initialize Touch
  touch.begin();

  // Start Keyboard
  keyboard.begin();
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
  handleTouch();

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

void drawKeypad() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    if(currentState == PIN_SETUP) {
      tft.print("Set New PIN:");
    } else {
      tft.print("Enter PIN:");
    }

    int buttonWidth = 80;
    int buttonHeight = 50;
    int startX = 40;
    int startY = 60;
    int spacingX = 15;
    int spacingY = 15;

    // Draw buttons 1-9
    for (int i = 1; i <= 9; i++) {
        int col = (i - 1) % 3;
        int row = (i - 1) / 3;
        int x = startX + col * (buttonWidth + spacingX);
        int y = startY + row * (buttonHeight + spacingY);
        tft.drawRoundRect(x, y, buttonWidth, buttonHeight, 5, TFT_WHITE);
        tft.setCursor(x + 35, y + 18);
        tft.print(i);
    }

    // Draw 0, Clear, and Enter buttons
    int y_row4 = startY + 3 * (buttonHeight + spacingY);

    // Clear button (C)
    tft.drawRoundRect(startX, y_row4, buttonWidth, buttonHeight, 5, TFT_RED);
    tft.setCursor(startX + 32, y_row4 + 18);
    tft.print("C");

    // 0 button
    int x_zero = startX + buttonWidth + spacingX;
    tft.drawRoundRect(x_zero, y_row4, buttonWidth, buttonHeight, 5, TFT_WHITE);
    tft.setCursor(x_zero + 35, y_row4 + 18);
    tft.print("0");

    // Enter button (OK)
    int x_ok = startX + 2 * (buttonWidth + spacingX);
    tft.drawRoundRect(x_ok, y_row4, buttonWidth, buttonHeight, 5, TFT_GREEN);
    tft.setCursor(x_ok + 25, y_row4 + 18);
    tft.print("OK");
}

void handleTouch() {
    if (touch.available()) {
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
            if (touch.x > x && touch.x < x + buttonWidth && touch.y > y && touch.y < y + buttonHeight) {
                if (enteredPin.length() < pinLength) enteredPin += String(i);
            }
        }

        int y_row4 = startY + 3 * (buttonHeight + spacingY);
        // Clear button
        if (touch.x > startX && touch.x < startX + buttonWidth && touch.y > y_row4 && touch.y < y_row4 + buttonHeight) {
            enteredPin = "";
        }
        // 0 button
        int x_zero = startX + buttonWidth + spacingX;
        if (touch.x > x_zero && touch.x < x_zero + buttonWidth && touch.y > y_row4 && touch.y < y_row4 + buttonHeight) {
            if (enteredPin.length() < pinLength) enteredPin += "0";
        }
        // Enter button
        int x_ok = startX + 2 * (buttonWidth + spacingX);
        if (touch.x > x_ok && touch.x < x_ok + buttonWidth && touch.y > y_row4 && touch.y < y_row4 + buttonHeight) {
            if (currentState == PIN_SETUP) {
                setupPin();
            } else {
                checkPin();
            }
        }

        // Update PIN display
        tft.fillRect(10, 35, 300, 20, TFT_BLACK);
        tft.setCursor(10, 40);
        String maskedPin = "";
        for (unsigned int i = 0; i < enteredPin.length(); i++) {
          maskedPin += "*";
        }
        tft.print(maskedPin);
        delay(200); // Debounce
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
    if (keyboard.isConnected()) {
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
        if (err != ESP_OK) {
            displayMessage("NVS Read Error!", TFT_RED);
            return;
        }

        size_t required_size;
        err = nvs_get_blob(my_handle, password_key, NULL, &required_size);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            keyboard.print("PasswordNotSet");
        } else {
            byte stored_data[required_size];
            nvs_get_blob(my_handle, password_key, stored_data, &required_size);

            byte iv[16];
            memcpy(iv, stored_data, 16);
            String password_decrypted = decrypt(stored_data + 16, required_size - 16, iv);

            keyboard.print(password_decrypted);
        }
        nvs_close(my_handle);
    } else {
        displayMessage("USB not connected!", TFT_RED);
    }
}

void encrypt(const String& plainText, byte* output, byte* iv) {
    AES aes;
    aes.setKey(aes_key, sizeof(aes_key));

    // Generate a random IV
    for(int i=0; i<16; i++) {
        iv[i] = random(256);
    }
    aes.setIV(iv);

    // PKCS7 padding
    int plainTextLen = plainText.length();
    int paddedLen = plainTextLen + (16 - (plainTextLen % 16));
    byte paddedText[paddedLen];
    memcpy(paddedText, plainText.c_str(), plainTextLen);
    for (int i = plainTextLen; i < paddedLen; i++) {
        paddedText[i] = 16 - (plainTextLen % 16);
    }

    aes.encrypt(paddedText, output, paddedLen);
}

String decrypt(byte* cipherText, size_t len, byte* iv) {
    AES aes;
    aes.setKey(aes_key, sizeof(aes_key));
    aes.setIV(iv);

    byte decrypted[len];
    aes.decrypt(cipherText, decrypted, len);

    // Remove PKCS7 padding
    char pad = decrypted[len - 1];
    int plainTextLen = len - pad;

    char plainText[plainTextLen + 1];
    memcpy(plainText, decrypted, plainTextLen);
    plainText[plainTextLen] = '\0';

    return String(plainText);
}

void displayMessage(const String& message, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color);
    tft.setCursor(10, 20);
    tft.print(message);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}