import tkinter as tk
from tkinter import messagebox
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.animation as animation
import numpy as np
from collections import deque
import threading
import time
import sys
import os
import serial
from serial.tools import list_ports
import matplotlib.image as mpimg
import logging
import struct
import matplotlib.patches
from matplotlib.widgets import Button
from matplotlib.gridspec import GridSpec
from PIL import Image, ImageTk
if getattr(sys, 'frozen', False):
    import pyi_splash

# ===========================
# Logging Configuration
# ===========================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] [%(module)s] %(message)s',
    handlers=[
        logging.FileHandler("app_debug.log"),
        logging.StreamHandler(sys.stdout)
    ]
)

HEADER_FOOTER = {
    b'FUT0': b'END0',     # TOF
    b'FUT1': b'END1',     # THERMAL
}

# ===========================
# Common Helper Functions
# ===========================
def get_resource_path(relative_path):
    """ Get absolute path to resource, works for dev and for PyInstaller """
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

def read_header_from_port(ser, header_to_check, header_length, max_bytes=200):
    """ Generic function to detect a specific header from a serial port. """
    other_header = next(h for h in HEADER_FOOTER if h != header_to_check)
    sync_buffer = bytearray()
    try:
        for _ in range(max_bytes):
            new_byte = ser.read(1)
            if not new_byte:
                return False  # Timeout
            sync_buffer.append(new_byte[0])
            if len(sync_buffer) > header_length:
                sync_buffer.pop(0)
            if sync_buffer == header_to_check:
                logging.info(f"Header {header_to_check.hex()} detected on {ser.port}")
                return "DESIRED"
            elif sync_buffer == other_header:
                send_uart_data(other_header, 0xA8, [0x01], HEADER_FOOTER[other_header], ser)
                return "SWAPPED"
        return False
    except Exception as e:
        logging.error(f"Error reading header on {ser.port}: {e}")
        return False

def detect_serial_port(baud_rate, header_to_check):
    """ Scans and returns the first serial port that responds with the correct header. """
    def scan_once():
        header_length = len(header_to_check)
        available_ports = list(list_ports.comports())
        logging.info(f"Scanning for device with header {header_to_check.hex()} at {baud_rate} baud...")
        for port in available_ports:
            logging.info(f"Trying port: {port.device}")
            try:
                with serial.Serial(port.device, baud_rate, timeout=0.5) as ser_test:
                    res = read_header_from_port(ser_test, header_to_check, header_length)
                    if res:
                        return res, port
            except (serial.SerialException, OSError):
                continue    
        return None, None
    
    res, port = scan_once()
    if res == "DESIRED":
        logging.info(f"Matching port found: {port.device}")
        return port.device
    elif res == "SWAPPED":
        time.sleep(3)
        res, port = scan_once()
        if res == "DESIRED":
            return port.device
    logging.error("No suitable port found.")
    return None

def send_uart_data(HEADER, TYPE, DATA, FOOTER, ser):
    try:
        byte_length = len(DATA)
        checksum = TYPE ^ byte_length
        for byte in DATA:
            checksum ^= byte
        packet = bytearray(HEADER)
        packet.append(TYPE)
        packet.append(byte_length)
        packet.extend(DATA)
        packet.append(checksum)
        packet.extend(FOOTER)
        if ser and ser.is_open:
            ser.write(packet)
            logging.info(f"TOF App: Sent packet: {packet.hex()}")
    except Exception as e:
        logging.error(f"TOF App: Error sending data: {e}")

# ========================================================================================
#
#       TOF PEOPLE DETECTION APPLICATION
#
# ========================================================================================
class TofApp:
    def __init__(self, master, port):
        self.master = master
        self.port = port
        self.ser = None
        self.running = True
        
        # Configuration
        self.BAUD_RATE = 115200
        self.SERIAL_TIMEOUT = 0.5
        self.BUFFER_SIZE = 1000
        self.UPDATE_INTERVAL = 120
        self.HEADER = b'FUT0'
        self.FOOTER = b'END0'
        self.HEADER_LENGTH = len(self.HEADER)
        self.FOOTER_LENGTH = len(self.FOOTER)
        self.CALIBRATION_DONE_FRAMES = 5

        # State Variables
        self.frame_buffer = deque(maxlen=self.BUFFER_SIZE)
        self.buffer_lock = threading.Lock()
        self.last_update_time = time.time()
        self.calibration_dots = 0
        self.calibration_done_counter = 0
        self.view_background_enabled = False
        self.STATE_MAPPING = {
            0: "None", 1: "Lying floor", 2: "Sitting", 3: "Standing", 4: "Falling Down!"
        }

        self.setup_ui()
        self.master.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.rotate_state = 0  # 0°, 90°, 180°, 270°
        self.mirror_state = False  # True = mirrored horizontally

    def start(self):
        """Connects to serial and starts background threads and UI animation."""
        try:
            self.ser = serial.Serial(
                self.port, self.BAUD_RATE, timeout=self.SERIAL_TIMEOUT,
                bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE
            )
            logging.info(f"TOF App: Serial connection established on {self.port}.")
        except Exception as e:
            logging.critical(f"TOF App: Failed to open serial port {self.port}: {e}")
            messagebox.showerror("Serial Port Error", f"Failed to open serial port {self.port}: {e}", parent=self.master)
            self.on_closing()
            return

        self.sthread = threading.Thread(target=self.serial_reader, daemon=True)
        self.sthread.start()
        
        # self.wthread = threading.Thread(target=self.watchdog_thread, args=(5,), daemon=True)
        # self.wthread.start()

        self.ani = animation.FuncAnimation(self.fig, self.update_plot, interval=self.UPDATE_INTERVAL, blit=False)
        logging.info("TOF App: Animation started.")

    def setup_ui(self):
        """Creates all the UI widgets for the TOF application."""
        # Set Demo Window Icon
        icon_path = get_resource_path("images.ico")
        if os.path.exists(icon_path):
            try:
                self.master.iconbitmap(icon_path)
            except tk.TclError:
                logging.warning("ToF App: Could not load icon 'images.ico'.")

        # --- Main container frames ---
        # The plot takes up all available space at the top
        plot_frame = tk.Frame(self.master)
        plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # The button frame is packed at the very bottom
        btn_frame = tk.Frame(self.master)
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=(5, 10))
        
        # The message frame is packed above the buttons
        msg_frame = tk.Frame(self.master)
        msg_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=5)
        
        # --- Message Label ---
        message_container = tk.Frame(msg_frame, height=80, bg="white")
        message_container.pack_propagate(False) # Prevents shrinking
        message_container.pack(side=tk.TOP, fill=tk.X, padx=10)
        self.message_text = tk.StringVar(value="Waiting for frames...")
        message_label = tk.Label(
            message_container, textvariable=self.message_text, font=("Helvetica", 16),
            wraplength=1000, bg="white", justify=tk.CENTER
        )
        message_label.pack(fill="both", expand=True)
        
        # --- Buttons ---
        # Use expand=True to make buttons share space instead of fixed padding
        # --- Buttons centered via grid in btn_frame ---
        # reserve four columns; outer two will absorb extra space
        for col in (0,1,2,3,4,7):
            btn_frame.columnconfigure(col, weight=1)
        for col in (5,6,8,9):
            btn_frame.columnconfigure(col,weight=0)

        self.recalibrate_button = tk.Button(
            btn_frame, text="Recalibrate Background",
            command=lambda: send_uart_data(self.HEADER, 0xA1, [0x01], self.FOOTER, self.ser),
            height=2, font=("Helvetica", 14, "bold")
        )
        self.recalibrate_button.grid(row=0, column=5, padx=20, pady=5)

        self.view_background_button = tk.Button(
            btn_frame, text="View Background",
            command=self.toggle_background,
            height=2, font=("Helvetica", 14, "bold")
        )
        self.view_background_button.grid(row=0, column=6, padx=20, pady=5)

        self.flip_lr_button = tk.Button(
            btn_frame,
            text="Mirror LR",
            command=self.toggle_flip_lr,
            height=2, font=("Helvetica", 14, "bold")
        )
        self.flip_lr_button.grid(row=0, column=8, padx=20, pady=5)
        # ——— Flip Up/Down ———        
        self.flip_ud_button = tk.Button(
            btn_frame,
            text="Rotate 90°",
            command=self.toggle_rotate,
            height=2, font=("Helvetica", 14, "bold")
        )
        self.flip_ud_button.grid(row=0, column=9, padx=20, pady=5)


        # --- Matplotlib Figure ---
        # Use a more standard rectangular figure size
        self.fig, self.ax = plt.subplots(figsize=(8, 6))
        # Use standard padding values that prevent cutoff
        self.fig.subplots_adjust(left= -0.1, right=0.8, top=0.9, bottom=0.15)

        icon_path = get_resource_path("images.ico")
        logo_path = get_resource_path("future_logo.png")

        if os.path.exists(logo_path):
            logo_img = mpimg.imread(logo_path)
            logo_ax = self.fig.add_axes([0.0, 0.9, 0.2, 0.1], anchor='NW', zorder=1)
            logo_ax.imshow(logo_img)
            logo_ax.axis('off')

        if os.path.exists(icon_path):
            self.master.iconbitmap(icon_path)

        # Create the plot and colorbar
        heatmap_init = np.zeros((8,8))
        self.cax = self.ax.imshow(
            heatmap_init, cmap="viridis", vmin=0, vmax=3000,
            extent=(0, 8, 8, 0),  # left, right, bottom, top
            origin='upper'
        )
        plt.colorbar(self.cax, ax=self.ax, label="Distance (mm)", fraction=0.046, pad=0.04)
        self.ax.set_title("Live 8x8 ToF Heatmap", fontsize=16)
        
        # --- Final Canvas Setup ---
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    def on_closing(self):
        """Handles graceful shutdown of the application."""
        logging.info("TOF App: Closing application.")
        self.running = False
        time.sleep(0.1) # Give threads a moment to see the flag
        if hasattr(self, "ani"):
            self.ani.event_source.stop()      # stop Tk timer
            plt.close(self.fig)               # destroy the figure
        try:
            if hasattr(self, 'sthread') and self.sthread.is_alive():
                self.sthread.join(timeout=1)
            if hasattr(self, 'wthread') and self.wthread.is_alive():
                self.wthread.join(timeout=1)
        except Exception as e:
            logging.error(f"TOF App: Error stopping threads: {e}")

        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
                logging.info("TOF App: Serial connection closed.")
        except Exception as e:
            logging.error(f"TOF App: Error closing serial connection: {e}")

        self.master.destroy()
        logging.info("TOF App: Window destroyed.")

    def serial_reader(self):
        """Continuously reads and parses data from the serial port."""
        while self.running:
            try:
                packet_type, data_bytes = self.read_packet(self.ser)
                if packet_type is None: continue

                frame_data_list = []
                if packet_type == 0xAF:
                    frame_data_list = self.parse_bundle_payload(data_bytes)
                else:
                    frame_data = None
                    if packet_type == 0xA3:
                        distances = self.parse_distance_data(data_bytes)
                        if distances:
                            frame_data = {"packet_type": "distance", "distances": distances}
                    elif packet_type == 0xA4:
                        people_in, people_out = self.parse_inout_data(data_bytes)
                        frame_data = {"packet_type": "inout", "people_in": people_in, "people_out": people_out}
                    elif packet_type == 0xA5:
                        people_info = self.parse_person_info(data_bytes)
                        frame_data = {"packet_type": "person_info", "people_info": people_info}
                    elif packet_type == 0xA6:
                        cal_data = self.parse_background_calibration(data_bytes)
                        frame_data = {"packet_type": "calibration", "calibrating": cal_data["calibrating"]}
                    if frame_data:
                        frame_data_list.append(frame_data)

                if frame_data_list:
                    with self.buffer_lock:
                        self.frame_buffer.extend(frame_data_list)
                    self.last_update_time = time.time()
            except Exception as e:
                if self.running: logging.error(f"TOF App: Serial reading error: {e}")
            finally:
                time.sleep(0.01)

    def update_plot(self, frame):
        """Updates the UI with new data from the buffer. Called by FuncAnimation."""
        with self.buffer_lock:
            frames_to_process = list(self.frame_buffer)
            self.frame_buffer.clear()

        if not frames_to_process: return [self.cax]

        distances, inout, people_info, calibrating = None, None, [], False
        for fdata in frames_to_process:
            ptype = fdata.get("packet_type")
            if ptype == "distance": distances = fdata.get("distances", [])
            elif ptype == "inout": inout = (fdata.get('people_in', 0), fdata.get('people_out', 0))
            elif ptype == "person_info": people_info = fdata.get("people_info", [])
            elif ptype == "calibration": calibrating = fdata.get("calibrating", False)
        
        arr = np.zeros((8,8))
        if distances is not None and len(distances) == 64:
            arr = np.array(distances).reshape((8,8))
            if self.rotate_state == 90:
                arr = np.rot90(arr, k=3)
            elif self.rotate_state == 180:
                arr = np.rot90(arr, k=2)
            elif self.rotate_state == 270:
                arr = np.rot90(arr, k=1)
            if not self.view_background_enabled:
                self.view_background_enabled = True
                self.master.after(0, lambda: self.view_background_button.config(text="Disable Background"))
        else:
            if self.view_background_enabled:
                self.view_background_enabled = False
                self.master.after(0, lambda: self.view_background_button.config(text="View Background"))
        self.cax.set_array(arr)
        
        number_of_person = len(people_info)
        msg_lines = []
        if inout: msg_lines.append(f"{number_of_person} People In: {inout[0]}  Out: {inout[1]}")
        if people_info:
            lines = []
            for idx, p in enumerate(people_info[:2]):
                state_str = self.STATE_MAPPING.get(p.get("State", 0), f"State:{p.get('State', 0)}")
                h, rem = divmod(p.get("Seconds", 0), 3600)
                m, s = divmod(rem, 60)
                dur_str = f"{h}h {m}m {s}s" if h > 0 else f"{m}m {s}s"
                lines.append(f"P{idx+1}, {state_str}, Loc:({p.get('X',0)}, {p.get('Y',0)}), Dur: {dur_str}")
            if lines: msg_lines.append("\n".join(lines))
        
        new_message = ""
        if calibrating:
            self.calibration_dots = (self.calibration_dots + 1) % 4
            new_message = f"Background initializing{'.' * self.calibration_dots}{' ' * (3 - self.calibration_dots)}\n\n"
            self.recalibrate_button.config(state='disabled')
            self.view_background_button.config(state='disabled')
            self.calibration_done_counter = self.CALIBRATION_DONE_FRAMES
        else:
            self.recalibrate_button.config(state='normal')
            self.view_background_button.config(state='normal')
            if self.calibration_done_counter > 0:
                new_message = "Background Calibration Done!\n\n"
                self.calibration_done_counter -= 1
            else:
                new_message = "\n".join(msg_lines) if msg_lines else ""
        
        current_lines = new_message.count('\n') + 1 if new_message else 0
        if current_lines < 3: new_message += '\n' * (3 - current_lines)
        
        if self.message_text.get() != new_message: self.message_text.set(new_message)
        
        self.canvas.draw_idle()
        return [self.cax]

    # --- Other methods specific to TOF App (parsing, sending data, etc.) ---
    def watchdog_thread(self, timeout=5):
        while self.running:
            if time.time() - self.last_update_time > timeout:
                logging.warning(f"TOF App: No data received for {timeout} seconds. Closing.")
                self.master.after(0, self.on_closing)
                break
            time.sleep(1)

    def read_packet(self, ser_conn):
        sync_buffer = bytearray()
        while self.running:
            new_byte = ser_conn.read(1)
            if not new_byte: return (None, None)
            sync_buffer.extend(new_byte)
            if len(sync_buffer) > self.HEADER_LENGTH: sync_buffer.pop(0)

            if sync_buffer == self.HEADER:
                try:
                    type_byte = ser_conn.read(1)
                    if not type_byte: continue
                    packet_type = type_byte[0]
                    if packet_type not in (0xA3, 0xA4, 0xA5, 0xA6, 0xAF): continue

                    length_byte = ser_conn.read(1)
                    if not length_byte: continue
                    length_val = length_byte[0]

                    data_bytes = ser_conn.read(length_val)
                    if len(data_bytes) < length_val: continue
                    
                    checksum_byte = ser_conn.read(1)
                    if not checksum_byte: continue
                    checksum_val = checksum_byte[0]
                    
                    footer_bytes = ser_conn.read(self.FOOTER_LENGTH)
                    if footer_bytes != self.FOOTER: continue

                    expected_checksum = packet_type ^ length_val
                    for b in data_bytes:
                        expected_checksum ^= b
                    if checksum_val != expected_checksum:
                        logging.warning("TOF App: packet checksum mismatch")
                        continue

                    return (packet_type, data_bytes)
                except Exception as e:
                    logging.warning(f"TOF App: Error reading packet structure: {e}")
                    continue
        return (None, None)

    def parse_bundle_payload(self, data_bytes):
        frames = []
        offset = 0

        while offset + 3 <= len(data_bytes):
            section_type = data_bytes[offset]
            section_len = data_bytes[offset + 1]
            data_start = offset + 2
            data_end = data_start + section_len

            if data_end >= len(data_bytes):
                break

            section_end = data_bytes[data_end]
            if section_end != section_type:
                logging.warning(f"TOF App: section marker mismatch type=0x{section_type:02X}")
                break

            section_data = data_bytes[data_start:data_end]
            frame_data = None

            if section_type == 0xA3:
                distances = self.parse_distance_data(section_data)
                if distances:
                    frame_data = {"packet_type": "distance", "distances": distances}
            elif section_type == 0xA4:
                people_in, people_out = self.parse_inout_data(section_data)
                frame_data = {"packet_type": "inout", "people_in": people_in, "people_out": people_out}
            elif section_type == 0xA5:
                people_info = self.parse_person_info(section_data)
                frame_data = {"packet_type": "person_info", "people_info": people_info}
            elif section_type == 0xA6:
                cal_data = self.parse_background_calibration(section_data)
                frame_data = {"packet_type": "calibration", "calibrating": cal_data["calibrating"]}

            if frame_data:
                frames.append(frame_data)

            offset = data_end + 1

        return frames
        
    def parse_distance_data(self, data_bytes):
        if len(data_bytes) != 128: return []
        return [(data_bytes[i] << 8) | data_bytes[i+1] for i in range(0, 128, 2)]

    def parse_inout_data(self, data_bytes):
        if len(data_bytes) < 4: return (0, 0)
        return ((data_bytes[0] << 8) | data_bytes[1], (data_bytes[2] << 8) | data_bytes[3])

    def parse_person_info(self, data_bytes):
        if not data_bytes: return []
        person_number = data_bytes[0]
        if person_number == 0: return []
        
        people_info = []
        expected_len = 1 + person_number * 5
        if len(data_bytes) < expected_len: return []
        
        offset = 1
        for i in range(person_number):
            people_info.append({
                "id": i + 1, "State": data_bytes[offset], "X": data_bytes[offset + 1],
                "Y": data_bytes[offset + 2], "Seconds": (data_bytes[offset + 3] << 8) | data_bytes[offset + 4]
            })
            offset += 5
        return people_info

    def parse_background_calibration(self, data_bytes):
        return {"calibrating": data_bytes[0] == 0x01 if data_bytes else False}
        

    def toggle_background(self):
        if self.view_background_enabled:
            send_uart_data(self.HEADER, 0xA2, [0x02], self.FOOTER, self.ser)
        else:
            send_uart_data(self.HEADER, 0xA2, [0x01], self.FOOTER, self.ser)

    def toggle_flip_lr(self):
        """Mirror the heatmap horizontally."""
        self.mirror_state = not self.mirror_state
        if self.mirror_state:
            self.ax.set_xlim(8, 0)
        else:
            self.ax.set_xlim(0, 8)

    def toggle_rotate(self):
        """Rotates the view in 90° increments (clockwise)."""
        self.rotate_state = (self.rotate_state + 90) % 360

# ========================================================================================
#
#       THERMAL HUMAN DETECTION APPLICATION
#
# ========================================================================================
class ThermalApp:
    def __init__(self, master, port):
        self.master = master
        self.port = port
        self.ser = None
        self.running = True

        # Configuration
        self.BAUD_RATE = 921600
        self.SERIAL_TIMEOUT = 0.5
        self.ROWS, self.COLS = 24, 32
        self.HEADER, self.FOOTER = b'FUT1', b'END1'
        self.HEADER_LENGTH, self.FOOTER_LENGTH = len(self.HEADER), len(self.FOOTER)
        
        # State Variables
        self.buffer_lock = threading.Lock()
        self.line_buffer = deque(maxlen=20)
        self.calibration_dots = 0
        self.bed_frame = False
        self.rect_start_point = None
        self.STATE_LABELS = ["none", "lying bed", "sitting bed", "1 person", "lying floor", "multi-person", "noise"]
        self.STATE_COLORS = {0:"red", 1:"orange", 2:"blue", 3:"purple", 4:"red", 5:"black", 6:"chocolate"}
        self.MAX_DISPLAYABLE_TRACKS = 20
        self.rotate_state = 0  # 0°, 90°, 180°, 270°
        self.mirror_state = False  # True = mirrored horizontally
        self.setup_ui()
        self._last_shape = (self.ROWS, self.COLS)
        self.master.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.data_rows, self.data_cols = 24, 32
    def start(self):
        """Connects to serial and starts background threads and UI animation."""
        try:
            self.ser = serial.Serial(
                self.port, self.BAUD_RATE, timeout=self.SERIAL_TIMEOUT,
                bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE
            )
            logging.info(f"Thermal App: Serial connection established on {self.port}.")
        except Exception as e:
            logging.critical(f"Thermal App: Failed to open serial port {self.port}: {e}")
            messagebox.showerror("Serial Port Error", f"Failed to open serial port {self.port}: {e}", parent=self.master)
            self.on_closing()
            return
            
        self.serial_thread = threading.Thread(target=self.serial_reader, daemon=True)
        self.serial_thread.start()

        self.ani = animation.FuncAnimation(self.fig, self.update_state, init_func=self.init_plot, interval=50, blit=True)
        logging.info("Thermal App: Animation started.")

    def setup_ui(self):
        """Creates all the UI widgets for the Thermal application."""
        # Start with a figure that's a bit wider to prevent crowding
        self.fig = plt.figure(figsize=(10, 10), dpi=100)
        icon_path = get_resource_path("images.ico")
        logo_path = get_resource_path("future_logo.png")

        if os.path.exists(logo_path):
            logo_img = mpimg.imread(logo_path)
            logo_ax = self.fig.add_axes([0.0, 0.9, 0.2, 0.1], anchor='NW', zorder=1)
            logo_ax.imshow(logo_img)
            logo_ax.axis('off')

        if os.path.exists(icon_path):
            self.master.iconbitmap(icon_path)

        # 1. Use subplots_adjust to set tight padding on the sides.
        # The right side is slightly larger to make room for the colorbar's label.
        self.fig.subplots_adjust(left=0.07, right=0.87, top=0.92, bottom=0.15)
        
        # 2. Use a simpler 2-row GridSpec. The footer will contain the text.
        gs = GridSpec(nrows=2, ncols=3, 
                      height_ratios=[8, 1], 
                      width_ratios=[0.15, 0.7, 0.15], 
                      figure=self.fig)
        self.ax_pred = self.fig.add_subplot(gs[0,1])
        ax_footer = self.fig.add_subplot(gs[1,:])
        ax_footer.axis("off")
        # --- Main Heatmap Plot ---
        self.heatmap_pred = self.ax_pred.imshow(
            np.zeros((self.ROWS, self.COLS)), origin= 'upper', cmap='viridis', vmin=15, vmax=32
        )
        self.ax_pred.set_aspect(self.COLS/self.ROWS, adjustable='box')
        self.heatmap_pred.set_extent((0, self.COLS, self.ROWS, 0))
        cbar = self.fig.colorbar(self.heatmap_pred, ax=self.ax_pred, fraction=0.046, pad=0.04)
        cbar.set_label("Temperature (°C)")
        self.ax_pred.set_title("Inference Output", fontsize=18)
        
        # --- Bed and Tracking Visuals ---
        self.bed_rect = matplotlib.patches.Rectangle((0,0), 0, 0, lw=2, ec='maroon', fc='none', label='Bed')
        self.ax_pred.add_patch(self.bed_rect)
        self.bed_text = self.ax_pred.text(0,0,"",visible=False,color='white',bbox=dict(fc='maroon',alpha=0.7),fontsize=12)
        self.setup_track_rectangles()

        # --- Footer Text (placed in the GridSpec footer) ---
        self.people_text = ax_footer.text(0.5, 0.6, "", ha="center", va="center", fontsize=15, animated=True)
        
        # 3. Manually place the buttons on the FIGURE, not in the GridSpec.
        # These coordinates are [left, bottom, width, height] as fractions of the window.
        # They are calculated to center the two buttons.
        button_bed_ax = self.fig.add_axes([0.28, 0.05, 0.2, 0.075])
        self.send_bed_button = Button(button_bed_ax, "Enable Bed Frame")
        self.send_bed_button.on_clicked(self.toggle_bed_frame)
        
        button_calib_ax = self.fig.add_axes([0.52, 0.05, 0.2, 0.075])
        self.send_button = Button(button_calib_ax, "Background Calibration")
        self.send_button.on_clicked(lambda event: send_uart_data(self.HEADER, 0xA1, [0x01], self.FOOTER, self.ser))

        button_flip_lr = self.fig.add_axes([0.82, 0.05, 0.05, 0.075])
        self.flip_lr_button = Button(button_flip_lr, "Mirror LR")
        self.flip_lr_button.label.set_fontsize(8)
        self.flip_lr_button.on_clicked(self.toggle_flip_lr)

        button_flip_ud = self.fig.add_axes([0.88, 0.05, 0.05, 0.075])
        self.flip_up_button = Button(button_flip_ud, "Rotate 90°")
        self.flip_up_button.label.set_fontsize(8)
        self.flip_up_button.on_clicked(self.toggle_rotate)
        # --- Final Canvas Setup ---
        plot_frame = tk.Frame(self.master)
        plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        self.fig.canvas.mpl_connect('motion_notify_event', self.on_mouse_move)
        self.fig.canvas.mpl_connect('button_press_event', self.on_mouse_press)
        self.fig.canvas.mpl_connect('button_release_event', self.on_mouse_release)

    def on_closing(self):
        """Handles graceful shutdown of the application."""
        logging.info("Thermal App: Closing application.")
        self.running = False
        time.sleep(0.1)
        if hasattr(self, "ani"):
            self.ani.event_source.stop()      # stop Tk timer
            plt.close(self.fig)               # destroy the figure
        try:
            if hasattr(self, 'serial_thread') and self.serial_thread.is_alive():
                self.serial_thread.join(timeout=1)
        except Exception as e:
            logging.error(f"Thermal App: Error stopping thread: {e}")

        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
                logging.info("Thermal App: Serial connection closed.")
        except Exception as e:
            logging.error(f"Thermal App: Error closing serial connection: {e}")
            
        self.master.destroy()
        logging.info("Thermal App: Window destroyed.")

    def init_plot(self):
        """Initial drawing function for blitting."""
        return [self.heatmap_pred, self.people_text, self.bed_rect, self.bed_text] + self.track_rect_patches + self.track_label_artists

    def _map_point(self, x, y):
            """Maps a point from the original sensor grid to the rotated display grid."""
            
            # ALWAYS use the original sensor dimensions (e.g., 24, 32) for calculations.
            
            if self.rotate_state == 90:   # 90° clockwise
                # The new x-coordinate is calculated from the original number of ROWS and the original y.
                # The new y-coordinate is the original x.
                return self.ROWS - 1 - y, x
            
            elif self.rotate_state == 180:
                # The new x is calculated from original COLS and x.
                # The new y is calculated from original ROWS and y.
                return self.COLS - 1 - x, self.ROWS - 1 - y
                
            elif self.rotate_state == 270: # 270° clockwise
                # The new x-coordinate is the original y.
                # The new y is calculated from original COLS and x.
                return y, self.COLS - 1 - x
                
            # Default 0° rotation
            return x, y


    def _map_box(self, box):
        """
        box  = dict or list with keys/entries x1,y1,x2,y2
        returns (x1,y1,x2,y2) mapped to screen space  (ordered!)
        """
        x1,y1 = self._map_point(box["x1"], box["y1"])
        x2,y2 = self._map_point(box["x2"], box["y2"])
        # after transformations the first corner might be > second → reorder
        x_lo, x_hi = sorted((x1, x2))
        y_lo, y_hi = sorted((y1, y2))
        return x_lo, y_lo, x_hi, y_hi

    def update_state(self, frame):
        """Updates the UI with new data from the buffer."""
        with self.buffer_lock:
            frames_to_process = list(self.line_buffer)
            self.line_buffer.clear()

        for fdata in frames_to_process:
            ptype = fdata.get("packet_type")
            if ptype == "state":
                people_data = fdata.get("people_data")
                self.people_text.set_text(f"Total people: {people_data[0]}")
                self.send_button.set_active(True); self.send_button.ax.patch.set_facecolor('1')
            elif ptype == "raw":
                data_array = fdata.get("temp").reshape((self.ROWS, self.COLS))
                if self.rotate_state == 90:
                    data_array = np.rot90(data_array, k=3)
                    # data_array = np.flipud(data_array)
                elif self.rotate_state == 180:
                    data_array = np.rot90(data_array, k=2)
                    # data_array = np.flipud(data_array)
                elif self.rotate_state == 270:
                    data_array = np.rot90(data_array, k=1)

                self.heatmap_pred.set_data(data_array)
                rows, cols = data_array.shape
                if (rows, cols) != self._last_shape:
                    self._last_shape = (rows, cols)  # cache the new shape
                    self.data_rows, self.data_cols = rows, cols 
                    self.heatmap_pred.set_extent((0, cols, rows, 0))
                    self.ax_pred.set_xlim(0, cols)
                    self.ax_pred.set_ylim(rows, 0)
                    self.ax_pred.set_aspect(cols/rows, adjustable='box')  # <-- key bit
                    self.ax_pred.figure.canvas.draw_idle()

            elif ptype == "bg":
                self.calibration_dots = (self.calibration_dots + 1) % 4
                self.people_text.set_text(f"Background initializing{'.' * self.calibration_dots}{' ' * (3 - self.calibration_dots)}")
                self.send_button.set_active(False); self.send_button.ax.patch.set_facecolor('0.8')
            elif ptype == "bed":
                bbox = fdata.get("bed_location")
                x1,y1,x2,y2 = self._map_box({"x1": bbox[0], "y1": bbox[2], "x2": bbox[1], "y2": bbox[3]})
                self.bed_rect.set_bounds(x1, y1, x2-x1, y2-y1)
                # self.bed_rect.set_bounds(bbox[0], bbox[2], bbox[1] - bbox[0], bbox[3] - bbox[2])
                # self.bed_text.set_text("Bed"); self.bed_text.set_position((bbox[0], bbox[2] + (bbox[3] - bbox[2] + 0.5))); 
                self.bed_text.set_position((x1, y1 + (y2-y1) + .5))
                self.bed_text.set_visible(True)
            elif ptype == "active_tracks":
                self.update_track_visuals(fdata.get("boxes"))

        return self.init_plot()

    def serial_reader(self):
        """Continuously reads and parses data from the serial port."""
        while self.running:
            try:
                packet_type, data_bytes = self.read_packet()
                if packet_type is None: continue
                
                frame_data = None
                if packet_type == 0xA2:
                    TOTAL_FLOATS = self.ROWS * self.COLS
                    float_array = np.array(struct.unpack(f'{TOTAL_FLOATS}f', data_bytes))
                    frame_data = {"packet_type": "raw", "temp": float_array}
                elif packet_type == 0xA4:
                    frame_data = {"packet_type": "state", "people_data": data_bytes}
                elif packet_type == 0xA6:
                    frame_data = {"packet_type": "bg"}
                elif packet_type == 0xA5:
                    frame_data = {"packet_type": "bed", "bed_location": data_bytes}
                elif packet_type == 0xA7:
                    num_boxes = data_bytes[0]
                    expected_len = 1 + num_boxes * 5
                    if len(data_bytes) == expected_len:
                        boxes = []
                        offset = 1
                        for _ in range(num_boxes):
                            box = struct.unpack('<BBBBB', data_bytes[offset:offset+5])
                            boxes.append({"x1":box[0],"y1":box[1],"x2":box[2],"y2":box[3],"state":box[4]})
                            offset += 5
                        frame_data = {"packet_type": "active_tracks", "boxes": boxes}
                
                if frame_data:
                    with self.buffer_lock:
                        self.line_buffer.append(frame_data)
            except Exception as e:
                if self.running: logging.error(f"Thermal App: Serial reading error: {e}")
        
    def read_packet(self):
        sync_buffer = bytearray()
        while self.running:
            try:
                new_byte = self.ser.read(1)
                if not new_byte: return None, None
                sync_buffer.append(new_byte[0])
                if len(sync_buffer) > self.HEADER_LENGTH: sync_buffer.pop(0)

                if sync_buffer == self.HEADER:
                    type_byte = self.ser.read(1)
                    if not type_byte: continue
                    packet_type = type_byte[0]
                    if packet_type not in (0xA2, 0xA4, 0xA5, 0xA6, 0xA7): continue

                    len_bytes = self.ser.read(2)
                    if len(len_bytes) < 2: continue
                    length_val = (len_bytes[0] << 8) | len_bytes[1]
                    if not (0 < length_val < 4000): continue
                    
                    data_bytes = self.ser.read(length_val)
                    if len(data_bytes) < length_val: continue
                    
                    self.ser.read(1) # Skip checksum
                    
                    footer_bytes = self.ser.read(self.FOOTER_LENGTH)
                    if footer_bytes == self.FOOTER:
                        return packet_type, data_bytes
            except Exception:
                continue
        return None, None

    # --- Other methods specific to Thermal App ---
    def setup_track_rectangles(self):
        self.track_rect_patches = []
        self.track_label_artists = []
        for _ in range(self.MAX_DISPLAYABLE_TRACKS):
            rect = matplotlib.patches.Rectangle((0,0),0,0,lw=2,ec='purple',fc='none',visible=False)
            self.ax_pred.add_patch(rect)
            label = self.ax_pred.text(0,0,"",visible=False,color='white',bbox=dict(fc='purple',alpha=0.7),fontsize=12)
            self.track_rect_patches.append(rect)
            self.track_label_artists.append(label)

    def update_track_visuals(self, received_boxes):
        for i in range(self.MAX_DISPLAYABLE_TRACKS):
            if i < len(received_boxes):
                box = received_boxes[i]
                box["x1"], box["y1"], box["x2"], box["y2"] = self._map_box(box)
                state = box["state"] if self.bed_frame and box["state"] != 4 else 3 if box["state"] in [1,2,4] else box["state"]
                w, h = box["x2"]-box["x1"], box["y2"]-box["y1"]
                if w > 0 and h > 0:
                    self.track_rect_patches[i].set_bounds(box["x1"], box["y1"], w, h)
                    self.track_label_artists[i].set_position((box["x1"], box["y1"] + h + 0.5))
                    self.track_label_artists[i].set_text(self.STATE_LABELS[state])
                    self.track_label_artists[i].get_bbox_patch().set_facecolor(self.STATE_COLORS[state])
                    self.track_rect_patches[i].set_edgecolor(self.STATE_COLORS[state])
                    self.track_rect_patches[i].set_visible(True)
                    self.track_label_artists[i].set_visible(True)
                else:
                    self.track_rect_patches[i].set_visible(False)
                    self.track_label_artists[i].set_visible(False)
            else:
                self.track_rect_patches[i].set_visible(False)
                self.track_label_artists[i].set_visible(False)
                                        
    def toggle_bed_frame(self, event):
        self.bed_frame = not self.bed_frame
        self.send_bed_button.label.set_text("Disable Bed" if self.bed_frame else "Enable Bed")
        if self.bed_frame: send_uart_data(self.HEADER, 0xA3, [0x01], self.FOOTER, self.ser)
        else: self.bed_rect.set_bounds(0,0,0,0); self.bed_text.set_visible(False)

    def on_mouse_press(self, event):
        if event.inaxes == self.ax_pred and event.button == 1:
            self.rect_start_point = (int(round(event.xdata)), int(round(event.ydata)))
            
    def on_mouse_release(self, event):
        if event.button == 1 and self.rect_start_point is not None:
            self.rect_start_point = None
            Bbox = self.bed_rect.get_bbox()
            coords = [int(c) for c in Bbox.extents]
            if messagebox.askyesno("Confirm Bed", f"Send Bed Coords?\nx1={coords[0]},y1={coords[1]},x2={coords[2]},y2={coords[3]}", parent=self.master):
                send_uart_data(self.HEADER, 0xA2, [coords[0], coords[2], coords[1], coords[3]], self.FOOTER, self.ser)
                if not self.bed_frame: 
                    self.bed_frame = not self.bed_frame
            else:
                self.bed_rect.set_bounds(0,0,0,0); self.bed_text.set_visible(False)

    def on_mouse_move(self, event):
        if event.inaxes == self.ax_pred and event.button == 1 and self.rect_start_point is not None:
            x_s, y_s = self.rect_start_point
            x_e, y_e = (int(round(event.xdata)), int(round(event.ydata)))
            left, bottom = min(x_s, x_e), min(y_s, y_e)
            width, height = abs(x_e - x_s), abs(y_e - y_s)
            self.bed_rect.set_bounds(left, bottom, width, height)
            self.bed_text.set_text("Bed"); self.bed_text.set_position((left, bottom + height + 0.5)); self.bed_text.set_visible(True)

    def toggle_flip_lr(self, event):
        """Mirror the heatmap horizontally."""
        self.mirror_state = not self.mirror_state
        self.ax_pred.invert_xaxis()

    def toggle_rotate(self, event):
        """Rotates the view in 90° increments (clockwise)."""
        self.rotate_state = (self.rotate_state + 90) % 360

# ========================================================================================
#
#       APPLICATION LAUNCHER
#
# ========================================================================================

class AppLauncher:
    def __init__(self, root):
        self.root = root
        self.root.title("Future Electronics Non-Vision App")
        self.root.geometry("450x500")
        self.root.resizable(False, False)
        self.current_app_instance = None
        self.current_app_window = None
        self.is_scanning = False

        self.setup_selection_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_main_close)

    def setup_selection_ui(self):
        self.selection_frame = tk.Frame(self.root)
        self.selection_frame.pack(pady=10, padx=20, fill="both", expand=True)

        # --- Logo ---
        icon_path = get_resource_path("images.ico")
        logo_path = get_resource_path("future_logo.png")
        if os.path.exists(icon_path):
            try: self.root.iconbitmap(icon_path)
            except tk.TclError: logging.warning("Could not load icon 'images.ico'.")     
        if os.path.exists(logo_path):
            try:
                img = Image.open(logo_path)
                img.thumbnail((800, 400)) # Resize image to fit
                self.logo_photo = ImageTk.PhotoImage(img)
                logo_label = tk.Label(self.selection_frame, image=self.logo_photo)
                logo_label.pack(pady=(20, 30))
            except Exception as e:
                logging.error(f"Could not load logo: {e}")

        tk.Label(self.selection_frame, text="Select an Application", font=("Helvetica", 20, "bold")).pack(pady=10)

        # --- Buttons ---
        self.tof_button = tk.Button(self.selection_frame, text="TOF Human Detection", font=("Helvetica", 16),
                                     command=self.launch_tof_app, height=3, width=30)
        self.tof_button.pack(pady=10)

        self.thermal_button = tk.Button(self.selection_frame, text="Thermal-Array Human Detection", font=("Helvetica", 16),
                                          command=self.launch_thermal_app, height=3, width=30)
        self.thermal_button.pack(pady=10)

        # --- Status Label ---
        self.status_label = tk.Label(self.selection_frame, text="", font=("Helvetica", 14, "italic"))
        self.status_label.pack(pady=20)

    def set_buttons_state(self, state):
        """Helper to set the state of launcher buttons."""
        self.tof_button.config(state=state)
        self.thermal_button.config(state=state)

    def animate_status(self, dot_count=0):
        if not self.is_scanning: return
        dots = '.' * dot_count
        self.status_label.config(text=f"Scanning for device{dots}")
        next_dot_count = (dot_count + 1) % 4
        self.animation_job = self.root.after(300, self.animate_status, next_dot_count)

    def _scan_and_launch(self, app_class, title, baud_rate, header):
        port = detect_serial_port(baud_rate, header)
        self.root.after(0, self._on_scan_complete, port, app_class, title)

    def _on_scan_complete(self, port, app_class, title):
        self.is_scanning = False
        if hasattr(self, 'animation_job'): self.root.after_cancel(self.animation_job)

        if not port:
            messagebox.showerror("Serial Port Error", f"Could not find device for '{title}'.\nPlease check connection.", parent=self.root)
            self.set_buttons_state(tk.NORMAL); self.status_label.config(text="")
            return

        self.status_label.config(text=f"Launching '{title}'...")
        self.current_app_window = tk.Toplevel(self.root)
        self.current_app_window.title(title); self.current_app_window.geometry("1200x800")
        self.current_app_instance = app_class(self.current_app_window, port)
        
        def on_demo_close():
            if self.current_app_instance: self.current_app_instance.on_closing()
            if self.current_app_window: self.current_app_window.destroy()
            self.current_app_window = None; self.current_app_instance = None
            self.set_buttons_state(tk.NORMAL); self.status_label.config(text="")
        
        self.current_app_window.protocol("WM_DELETE_WINDOW", on_demo_close)
        self.current_app_instance.start()
        self.status_label.config(text=f"'{title}' is running.")

    def launch_app(self, app_class, title, baud_rate, header):
        if self.is_scanning: return
        self.set_buttons_state(tk.DISABLED)
        self.is_scanning = True
        self.animate_status()
        threading.Thread(target=self._scan_and_launch, args=(app_class, title, baud_rate, header), daemon=True).start()

    def launch_tof_app(self):
        self.launch_app(TofApp, "TOF People Detection Demo", 921600, b'FUT0')

    def launch_thermal_app(self):
        self.launch_app(ThermalApp, "Thermal Human Detection", 921600, b'FUT1')

    def on_main_close(self):
        logging.info("Main launcher closing.")
        if self.current_app_instance: self.current_app_instance.on_closing()
        if self.current_app_window: self.current_app_window.destroy()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app_launcher = AppLauncher(root)
    if getattr(sys, 'frozen', False):
        pyi_splash.close()
    root.mainloop()
