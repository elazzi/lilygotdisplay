# Password Vault on LilyGO T-Display S3 Long

This project turns a LilyGO T-Display S3 Long into a USB password vault. 
It allows you to  store passwords on the device in an AES vault that uses the pin as salt and type them into a computer by emulating a USB keyboard.
 The device is unlocked with a PIN entered on the touch screen.
 in the screen to input the pin you have a clear pin putton this will erase all stored contents
 after clearing content the device restarts   it will not emulate a keybord untill next reboot and will prompt you the first time only for new pin once pin is set 
  it will accept on console as input the new password once password is saved in an AES hash using the pin as salt
  device reboots and goes into default functioning mode where it waits for pin

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