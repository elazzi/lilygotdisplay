import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import queue

class PasswordVaultApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Device Password Setup Tool")

        self.serial_port = None
        self.read_queue = queue.Queue()

        # --- Port Selection ---
        self.port_frame = ttk.LabelFrame(root, text="COM Port")
        self.port_frame.pack(padx=10, pady=5, fill="x")

        self.port_label = ttk.Label(self.port_frame, text="Select Port:")
        self.port_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.port_combobox = ttk.Combobox(self.port_frame)
        self.port_combobox.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.connect_button = ttk.Button(self.port_frame, text="Connect", command=self.connect_serial)
        self.connect_button.pack(side=tk.LEFT, padx=5, pady=5)

        # --- Password Management ---
        self.password_frame = ttk.LabelFrame(root, text="Set Device Password")
        self.password_frame.pack(padx=10, pady=5, fill="x")

        self.password_label = ttk.Label(self.password_frame, text="Password:")
        self.password_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.password_entry = ttk.Entry(self.password_frame)
        self.password_entry.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.send_password_button = ttk.Button(self.password_frame, text="Set Password", command=self.set_password)
        self.send_password_button.pack(side=tk.LEFT, padx=5, pady=5)

        # --- Status Area ---
        self.status_frame = ttk.LabelFrame(root, text="Device Status")
        self.status_frame.pack(padx=10, pady=10, fill="both", expand=True)

        self.status_text = tk.Text(self.status_frame, height=10, state="disabled")
        self.status_text.pack(padx=5, pady=5, fill="both", expand=True)

        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.scan_ports() # Start the port scanning loop

    def scan_ports(self):
        """Scans for available serial ports and updates the combobox."""
        # Don't update the list if the user has the dropdown open
        # Check if the combobox has focus, which indicates user interaction.
        if self.root.focus_get() == self.port_combobox:
            return

        current_selection = self.port_combobox.get()
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combobox['values'] = ports

        if current_selection in ports:
            self.port_combobox.set(current_selection)
        elif ports:
            self.port_combobox.set(ports[0])
        else:
            self.port_combobox.set('')
        
        self.root.after(5000, self.scan_ports) # Schedule the next scan

    def connect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.connect_button.config(text="Connect")
            self.log_status("Disconnected.")
            return

        selected_port = self.port_combobox.get()
        if not selected_port:
            messagebox.showerror("Error", "No serial port selected.")
            return

        try:
            self.serial_port = serial.Serial(selected_port, 115200, timeout=1)
            self.log_status(f"Connected to {selected_port}")
            self.connect_button.config(text="Disconnect")

            # Start a thread to read from the serial port
            self.read_thread = threading.Thread(target=self.read_from_port, daemon=True)
            self.read_thread.start()
            self.process_serial_queue()
        except serial.SerialException as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {e}")
            self.log_status(f"Failed to connect to {selected_port}")

    def read_from_port(self):
        while self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    self.read_queue.put(line)
            except (serial.SerialException, TypeError):
                break # Port was closed
        self.read_queue.put(None) # Signal that the port is closed

    def process_serial_queue(self):
        try:
            while True:
                line = self.read_queue.get_nowait()
                if line is None:
                    self.connect_button.config(text="Connect")
                    self.log_status("Disconnected.")
                    break
                self.log_status(f"Device: {line}")
        except queue.Empty:
            pass
        self.root.after(100, self.process_serial_queue)

    def send_command(self, command):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write((command + '\n').encode('utf-8'))
            self.log_status(f"Sent: {command}")
        else:
            messagebox.showwarning("Not Connected", "Please connect to a serial port first.")

    def set_password(self):
        password = self.password_entry.get()
        if password:
            # Send the raw password string, which is what the firmware expects
            self.send_command(password)
            self.password_entry.delete(0, tk.END)
        else:
            messagebox.showwarning("Input Error", "Password cannot be empty.")

    def log_status(self, message):
        self.status_text.config(state="normal")
        self.status_text.insert(tk.END, message + "\n")
        self.status_text.see(tk.END)
        self.status_text.config(state="disabled")

    def on_closing(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = PasswordVaultApp(root)
    root.mainloop()