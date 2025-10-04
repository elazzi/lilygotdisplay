import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import queue

class PasswordVaultApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Password Vault Manager")

        self.serial_port = None
        self.read_queue = queue.Queue()

        # --- Port Selection ---
        self.port_frame = ttk.LabelFrame(root, text="COM Port")
        self.port_frame.pack(padx=10, pady=5, fill="x")

        self.port_label = ttk.Label(self.port_frame, text="Select Port:")
        self.port_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.port_combobox = ttk.Combobox(self.port_frame)
        self.port_combobox.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")
        self.populate_ports()

        self.connect_button = ttk.Button(self.port_frame, text="Connect", command=self.connect_serial)
        self.connect_button.pack(side=tk.LEFT, padx=5, pady=5)

        # --- Password Management ---
        self.password_frame = ttk.LabelFrame(root, text="Update Password")
        self.password_frame.pack(padx=10, pady=5, fill="x")

        self.password_label = ttk.Label(self.password_frame, text="New Password:")
        self.password_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.password_entry = ttk.Entry(self.password_frame, show="*")
        self.password_entry.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.send_password_button = ttk.Button(self.password_frame, text="Update Password", command=self.update_password)
        self.send_password_button.pack(side=tk.LEFT, padx=5, pady=5)

        # --- PIN Management ---
        self.pin_frame = ttk.LabelFrame(root, text="Update PIN")
        self.pin_frame.pack(padx=10, pady=5, fill="x")

        self.pin_label = ttk.Label(self.pin_frame, text="New PIN (4 digits):")
        self.pin_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.pin_entry = ttk.Entry(self.pin_frame, show="*")
        self.pin_entry.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.send_pin_button = ttk.Button(self.pin_frame, text="Update PIN", command=self.update_pin)
        self.send_pin_button.pack(side=tk.LEFT, padx=5, pady=5)

        # --- Status Area ---
        self.status_frame = ttk.LabelFrame(root, text="Device Status")
        self.status_frame.pack(padx=10, pady=10, fill="both", expand=True)

        self.status_text = tk.Text(self.status_frame, height=10, state="disabled")
        self.status_text.pack(padx=5, pady=5, fill="both", expand=True)

        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

    def populate_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combobox['values'] = ports
        if ports:
            self.port_combobox.set(ports[0])

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

    def update_password(self):
        password = self.password_entry.get()
        if password:
            self.send_command(f"UPDATE_PASS:{password}")
            self.password_entry.delete(0, tk.END)
        else:
            messagebox.showwarning("Input Error", "Password cannot be empty.")

    def update_pin(self):
        pin = self.pin_entry.get()
        if len(pin) == 4 and pin.isdigit():
            self.send_command(f"UPDATE_PIN:{pin}")
            self.pin_entry.delete(0, tk.END)
        else:
            messagebox.showwarning("Input Error", "PIN must be 4 digits.")

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