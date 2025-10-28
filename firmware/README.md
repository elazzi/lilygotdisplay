Project Brief: LilyGO T-Display S3 Long - USB Password Device
1. Core Objective
To create firmware for the LilyGO T-Display S3 Long that turns it into a secure, single-password USB input device. The device will store one password, securely encrypted, which can be "typed" into a host computer by emulating a USB keyboard. All interaction, including unlock and setup, will be handled via the device's touch screen.
2. Hardware Target
•	LilyGO T-Display S3 Long
3. Core Features
•	PIN-Based Unlock: The device is secured by a user-defined PIN entered on the touch screen.
•	Secure Storage: A single password (e.g., a complex master password) is stored on the device's non-volatile memory.
•	AES Encryption: The stored password must be encrypted using AES. The user's PIN must be used to derive the AES encryption key (e.g., via a KDF). The PIN itself should be stored as a hash for verification, not in plain text.
•	USB Keyboard (HID) Emulation: When unlocked, the device connects as a USB keyboard and types the stored password.
•	On-Screen QWERTY Keyboard: A touch-screen QWERTY keyboard is required for the initial setup to enter the password that will be stored.
•	Secure Data Wipe: A dedicated function to erase all user data (PIN and encrypted password), forcing the device back into setup mode.
4. User Flow & Screen Logic
This device has three distinct operational flows:
Flow 1: First-Time Setup (or Post-Wipe)
This flow is triggered when no PIN is found in memory.
1.	Boot: Device starts.
2.	Screen 1: Set PIN: Display a numeric-style keypad. Prompt the user: "Set a new PIN."
3.	Screen 2: Confirm PIN: Prompt the user: "Confirm your PIN."
o	On Mismatch: Show "PINs do not match. Try again." and return to Screen 1.
o	On Match: Proceed to Screen 3.
4.	Screen 3: Set Password: Display a full QWERTY touch keyboard. Prompt the user: "Enter the password to store."
o	The user taps out the password character by character.
o	The user taps an "Enter" or "OK" key on the on-screen keyboard.
5.	Action: Save & Encrypt:
o	The device hashes the new PIN for verification and stores the hash.
o	The device uses the new PIN to derive an AES key.
o	The device encrypts the password entered on the QWERTY keyboard using the derived key.
o	The device saves the encrypted password to non-volatile memory.
6.	Action: Reboot: Once the password is saved, the device automatically reboots.
Constraint: During this entire setup flow, USB keyboard (HID) emulation must be completely disabled.
Flow 2: Standard Operation (Default Mode)
This is the default mode after the device has been set up.
1.	Boot: Device starts and immediately displays Screen 4: PIN Unlock.
2.	Screen 4: PIN Unlock:
o	Displays a numeric keypad for PIN entry.
o	Displays a "Clear All Data" button (see Flow 3).
3.	User Action: Enters PIN.
o	On Correct PIN:
1.	The device unlocks.
2.	It emulates a USB keyboard.
3.	It types the single, decrypted password one time.
4.	(Recommendation: After typing, the device should immediately re-lock itself and return to Screen 4).
o	On Incorrect PIN:
1.	Display "Incorrect PIN."
2.	Remain on Screen 4, waiting for the correct PIN.
Flow 3: Secure Data Wipe
This flow is initiated by the user from the unlock screen.
1.	Boot: Device is in "Standard Operation" (Flow 2) at Screen 4: PIN Unlock.
2.	User Action: The user presses the "Clear All Data" button on the PIN screen.
o	(Recommendation: Add a confirmation step, e.g., "Hold for 3 seconds to wipe" or "Tap again to confirm wipe.")
3.	Action: Wipe: The device securely erases all user-defined content from non-volatile memory (the PIN hash and the encrypted password).
4.	Action: Reboot: The device immediately reboots.
5.	Result: Upon rebooting, the device finds no PIN and automatically enters Flow 1: First-Time Setup.



A companion desktop application allows for managing the password on the device.



## How to Set Up and Use (for Windows Beginners)

This guide will walk you through setting up both the device firmware and the desktop application on a Windows PC.

### Part 1: Flashing the Firmware to the Device

The firmware is the code that runs on your LilyGO T-Display S3 Long. We will use Visual Studio Code with the PlatformIO extension to build and upload it.

#### Prerequisites:
1.  **Visual Studio Code:** Download and install it from [code.visualstudio.com](https://code.visualstudio.com/).
2.  **PlatformIO IDE Extension:**
    *   Open Visual Studio Code.
    *   Click on the Extensions icon on the left sidebar (it looks like four squares).
    *   Search for `PlatformIO IDE` and click "Install".
    *   Restart VS Code after installation.

#### Steps:
1.  **Open the Firmware Project:**
    *   In VS Code, go to `File > Open Folder...`.
    *   Navigate to and select the `firmware` folder from this project.
    *   PlatformIO may take a few minutes to initialize and download the necessary tools and libraries automatically. You can see the progress in the bottom status bar.

2.  **(IMPORTANT) Change the Encryption Key:**
    *   In the VS Code file explorer, open the `main.cpp` file.
    *   Find the `aes_key` line  and change the hexadecimal values to your own secret key.

3.  **Upload the Firmware:**
    *   Connect your LilyGO T-Display S3 Long to your PC with a USB-C cable.
    *   The device needs to be in "bootloader" mode to receive the new firmware. To do this:
        a. Press and **hold** the "BOOT" button on the device.
        b. While still holding it, press and release the "RST" (reset) button.
        c. You can now release the "BOOT" button.
    *   In VS Code, find the PlatformIO toolbar at the bottom of the screen. Click the **Upload** button (it looks like a right-facing arrow `→`).
    *   PlatformIO will now compile the code and upload it to your device. You can see the progress in the terminal window. If it's successful, the device will restart.

4.  **Initial Setup on Device:**
    *   The first time you run the device, it will ask you to set a 4-digit PIN. Use the on-screen keypad to enter a PIN and press "OK".
    *   The device is now ready.

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

### Part 3: Using the Application
1.  **Connect to the Device:**
    *   With the LilyGO device plugged into your PC, open the desktop app.
    *   Select the correct COM port for your device from the dropdown menu. (If you're unsure, you can find it in the Windows Device Manager under "Ports (COM & LPT)").
    *   Click **Connect**. The status area should show "Connected".
2.  **Update Password or PIN:**
    *   Enter a new password or a new 4-digit PIN in the respective fields.
    *   Click the "Update" button next to the field you want to change.
    *   The status area will show a confirmation message from the device.

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