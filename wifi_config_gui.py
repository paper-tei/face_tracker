import tkinter as tk
from tkinter import messagebox, scrolledtext
from serial import Serial, SerialException
from serial.tools import list_ports
import threading
import time
import atexit

# 全局变量，用于串口管理
ser = None
stop_logging = threading.Event()

# 自动检测可用串口
def detect_ports():
    ports = list_ports.comports()
    return [port.device for port in ports]

# 确保程序退出时关闭串口
def close_serial():
    global ser
    if ser and ser.is_open:
        ser.close()
        print("串口已关闭")

atexit.register(close_serial)  # 程序退出时自动关闭串口

# 发送 Wi-Fi 配置到 ESP32
def send_wifi_config():
    global ser
    port = port_entry.get()
    baudrate = baudrate_entry.get()
    ssid = ssid_entry.get()
    password = password_entry.get()

    if not port or not baudrate or not ssid or not password:
        messagebox.showerror("错误", "请完整填写所有字段")
        return

    try:
        ser = Serial(port, int(baudrate), timeout=5)
        config_data = f"SSID={ssid},PASS={password}\n"
        ser.write(config_data.encode())
        time.sleep(2)
        response = ser.readline().decode().strip()
        if response == "OK":
            messagebox.showinfo("成功", "Wi-Fi 配置成功！ESP32 已连接到网络")
        elif response == "ERROR":
            messagebox.showerror("失败", "ESP32 配置失败，请检查输入或设备状态")
        else:
            messagebox.showwarning("警告", f"未识别的响应: {response}")
    except SerialException as e:
        messagebox.showerror("错误", f"无法与设备通信: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()

# 使用线程避免 GUI 卡顿
def send_wifi_config_thread():
    threading.Thread(target=send_wifi_config, daemon=True).start()

# 实时读取串口日志
def read_serial_logs():
    global ser
    port = port_entry.get()
    baudrate = baudrate_entry.get()

    if not port or not baudrate:
        return

    try:
        ser = Serial(port, int(baudrate), timeout=1)
        while not stop_logging.is_set():
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                log_area.insert(tk.END, f"{line}\n")
                log_area.see(tk.END)  # 滚动到最新日志
    except SerialException as e:
        log_area.insert(tk.END, f"串口错误: {e}\n")
        log_area.see(tk.END)
    finally:
        if ser and ser.is_open:
            ser.close()

# 开始日志打印线程
def start_log_thread():
    stop_logging.clear()
    threading.Thread(target=read_serial_logs, daemon=True).start()

# 停止日志打印线程
def stop_log_thread():
    stop_logging.set()

# 创建主窗口
root = tk.Tk()
root.title("ESP32 Wi-Fi 配置工具")

# 自动检测可用串口
port_list = detect_ports()
if not port_list:
    messagebox.showerror("错误", "未检测到可用串口，请检查连接")
    port_list = ["无可用串口"]

# 串口设置
tk.Label(root, text="串口:").grid(row=0, column=0, padx=10, pady=5)
port_entry = tk.StringVar()
port_entry.set(port_list[0])
port_dropdown = tk.OptionMenu(root, port_entry, *port_list)
port_dropdown.grid(row=0, column=1, padx=10, pady=5)

# 波特率设置
tk.Label(root, text="波特率:").grid(row=1, column=0, padx=10, pady=5)
baudrate_entry = tk.Entry(root)
baudrate_entry.insert(0, "115200")
baudrate_entry.grid(row=1, column=1, padx=10, pady=5)

# Wi-Fi SSID 输入
tk.Label(root, text="SSID:").grid(row=2, column=0, padx=10, pady=5)
ssid_entry = tk.Entry(root)
ssid_entry.grid(row=2, column=1, padx=10, pady=5)

# Wi-Fi 密码输入
tk.Label(root, text="密码:").grid(row=3, column=0, padx=10, pady=5)
password_entry = tk.Entry(root, show="*")
password_entry.grid(row=3, column=1, padx=10, pady=5)

# 发送按钮
send_button = tk.Button(root, text="发送 Wi-Fi 配置", command=send_wifi_config_thread)
send_button.grid(row=4, column=0, columnspan=2, pady=10)

# 日志显示区域
tk.Label(root, text="串口日志:").grid(row=5, column=0, padx=10, pady=5, columnspan=2)
log_area = scrolledtext.ScrolledText(root, width=60, height=20, state="normal")
log_area.grid(row=6, column=0, columnspan=2, padx=10, pady=5)

# 日志控制按钮
start_log_button = tk.Button(root, text="开始日志打印", command=start_log_thread)
start_log_button.grid(row=7, column=0, pady=10)

stop_log_button = tk.Button(root, text="停止日志打印", command=stop_log_thread)
stop_log_button.grid(row=7, column=1, pady=10)

# 启动主循环
root.mainloop()
