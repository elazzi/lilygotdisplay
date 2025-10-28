Project Brief: LilyGO T-Display S3 Long - USB Password Device

1. Core Objective
To create firmware for the LilyGO T-Display S3 Long that turns it into a secure, multi-password USB input device. The device will store up to four passwords, securely encrypted, which can be "typed" into a host computer by emulating a USB keyboard. All interaction, including unlock and setup, will be handled via the device's touch screen.

2. Hardware Target
•	LilyGO T-Display S3 Long

3. Core Features
•	PIN-Based Unlock: The device is secured by a user-defined PIN entered on the touch screen.
•	Secure Storage: Up to four passwords (e.g., complex master passwords) are stored on the device's non-volatile memory.
•	**AES-256 Encryption:** The stored password is encrypted using AES-256. The user's PIN is used to derive the encryption key via PBKDF2. The PIN itself is stored as a SHA-256 hash for verification, not in plain text.
•	**USB Keyboard (HID) Emulation:** When unlocked, the user can select one of the stored passwords from a touch screen menu, which the device then types as a USB keyboard.
•	**Serial Password Input:** For maximum security and to support complex passwords, the password to be stored is entered via a standard serial monitor during setup, not on the screen.
•	Secure Data Wipe: A dedicated function to erase all user data (PIN and encrypted password), forcing the device back into setup mode.

4. User Flow & Screen Logic
This device has three distinct operational flows:

Flow 1: First-Time Setup (or Post-Wipe)
This flow is triggered when no PIN is found in memory.
1.	Boot: Device starts.
2.	**Screen 1: Set PIN:** Display a numeric keypad. Prompt the user: "Enter New PIN."
3.	**Screen 2: Confirm PIN:** Prompt the user: "Confirm New PIN."
    -	On Mismatch: Show "PINs do not match" and return to Screen 1.
    -	On Match: Proceed to the next screen.
4.	**Screen 3: Set Passwords via Serial:** Display instructions: "Enter Password 1 in Serial Monitor and press Enter."
    -	The user opens a serial monitor (like the one in Arduino IDE or PlatformIO).
    -	The user types or pastes the first password and presses Enter. The screen will then prompt for "Password 2", and so on, up to four passwords.
    -	To finish setup before adding all four passwords, the user can submit an empty password (just press Enter).
5.	**Action: Save & Encrypt:**
    -	The device hashes the new PIN for verification and stores the hash.
    -	For each password received via serial, the device uses the new PIN to derive an AES key, encrypts the password, and saves the encrypted result to non-volatile memory.
6.	**Action: Reboot:** Once the user has finished entering passwords, the device automatically reboots into standard operation mode.

*Constraint: During this entire setup flow, USB keyboard (HID) emulation is disabled.*

Flow 2: Standard Operation (Default Mode)
This is the default mode after the device has been set up.
1.	Boot: Device starts and immediately displays Screen 4: PIN Unlock.
2.	**Screen 4: PIN Unlock:**
    -	Displays a numeric keypad for PIN entry.
    -	Displays a "CLEAR ALL" button (see Flow 3).
3.	**User Action:** Enters PIN and taps "OK".
    -	**On Correct PIN:** The device unlocks and proceeds to **Screen 5: Password Selection**.
4.	**Screen 5: Password Selection:**
    -	Displays four buttons, labeled "Password 1" through "Password 4".
5.	**User Action:** Taps one of the password buttons.
    -	The device emulates a USB keyboard and types the corresponding decrypted password one time.
    -	For security, the device immediately re-locks and returns to the PIN unlock screen.
    -	**On Incorrect PIN:**
        1. Display "Incorrect PIN" and the number of attempts remaining.
        2. After 5 failed attempts, the device enters a 30-second lockout.

Flow 3: Secure Data Wipe
This flow is initiated by the user from the unlock screen.
1.	**Start:** Device is at the "PIN Unlock" screen.
2.	**User Action:** The user taps the "CLEAR ALL" button.
3.	**Screen 6: Wipe Confirmation:** Display "WIPE ALL DATA? Long touch to confirm".
4.	**User Action:** The user touches and holds the screen for 3 seconds.
5.	**Action: Wipe:** The device securely erases all user-defined content from non-volatile memory (the PIN hash and all encrypted passwords).
6.	**Action: Reboot:** The device immediately reboots.
7.	**Result:** Upon rebooting, the device finds no PIN and automatically enters Flow 1: First-Time Setup.

## How to Set Up and Use (for Windows Beginners)

This guide will walk you through setting up the device firmware on a Windows PC.

### Part 1: Flashing the Firmware to the Device

The firmware is the code that runs on your LilyGO T-Display S3 Long. We will use Visual Studio Code with the PlatformIO extension to build and upload it.

#### Prerequisites:
1.  **Visual Studio Code:** Download and install from code.visualstudio.com.
2.  **PlatformIO IDE Extension:** In VS Code, go to the Extensions view, search for `PlatformIO IDE`, and click "Install".

#### Steps:
1.  **Open the Firmware Project:**
    *   In VS Code, go to `File > Open Folder...`.
    *   Navigate to and select the `firmware` folder from this project.
    *   PlatformIO will automatically download the necessary tools and libraries.

3.  **Upload the Firmware:**
    *   Connect your LilyGO T-Display S3 Long to your PC with a USB-C cable.
    *   The device needs to be in "bootloader" mode to receive the new firmware. To do this:
        a. Press and **hold** the "BOOT" button on the device.
        b. While still holding it, press and release the "RST" (reset) button.
        c. You can now release the "BOOT" button.
    *   In VS Code, find the PlatformIO toolbar at the bottom of the screen. Click the **Upload** button (it looks like a right-facing arrow `→`).
    *   PlatformIO will now compile the code and upload it to your device. You can see the progress in the terminal window. If it's successful, the device will restart.

4.  **Initial Setup on Device:**
    *   The first time you run the device, it will ask you to set a PIN (at least 4 digits). Use the on-screen keypad to enter a PIN and press "OK".
    *   Confirm the PIN on the next screen.
    *   The device will then ask you to enter your passwords one by one via the serial port. You can add up to four.
    *   To finish adding passwords, simply press Enter on an empty line in the serial monitor.

### Part 2: Running the Desktop Management App

This Python application lets you update the password and PIN on your device from your PC.

#### Prerequisites:
1.  **Python:** Download and install Python from [python.org](https://www.python.org/downloads/windows/).
    *   **Important:** During installation, make sure to check the box that says **"Add Python to PATH"**.

#### Steps:
1.  **Open Command Prompt:**
    *   Press the `Win` key, type `cmd`, and press Enter.

2.  **Navigate to the Project Folder:**
    *   In the command prompt, use the `cd` command to navigate to where you saved this project. For example:
      ```
      cd C:\Users\YourUser\Documents\lilygo-password-vault\desktop_app
      ```

3.  **Install Dependencies:**
    *   Run the following command to install the necessary Python libraries:
      ```
      pip install -r requirements.txt
      ```

4.  **Run the Application:**
    *   Once the libraries are installed, run the application with this command:
      ```
      python main.py
      ```

### Part 2a (Optional): Building a Standalone Executable

If you want to create a single `.exe` file that can be run on any Windows computer without needing to install Python or any libraries, you can build the application using `PyInstaller`.

1.  **Navigate to the Desktop App Folder:**
    *   Open a Command Prompt and navigate to the `desktop_app` directory as shown in the previous step.

2.  **Install PyInstaller:**
    *   If you haven't already, install the required libraries, including `PyInstaller`:
      ```
      pip install -r requirements.txt
      ```

3.  **Build the Executable:**
    *   Run the following command. This will bundle the application and all its dependencies into a single executable file.
      ```
      pyinstaller --onefile --windowed main.py
      ```

4.  **Find the Executable:**
    *   After the process completes, you will find a new `dist` folder inside the `desktop_app` directory.
    *   Your standalone application, `main.exe`, will be inside this `dist` folder. You can now run this file directly or share it with others.


### Part 3: Using the Application
1.  **Connect to the Device:**
    *   With the LilyGO device plugged into your PC, open the desktop app.
    *   Select the correct COM port for your device from the dropdown menu. (If you're unsure, you can find it in the Windows Device Manager under "Ports (COM & LPT)").
    *   Click **Connect**. The status area should show "Connected".

---

## Project Structure

```
.
├── desktop_app/      # Python-based desktop application
│   ├── main.py
│   └── requirements.txt
├── firmware/         # PlatformIO code for the ESP32-S3
├── .gitignore
└── README.md
```