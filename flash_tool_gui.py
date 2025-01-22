import glob
import sys
import os
import subprocess
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton,
    QLabel, QTextEdit, QComboBox
)
from PyQt5.QtCore import QThread, pyqtSignal
import serial.tools.list_ports
import serial
import time

class FlashThread(QThread):
    log_output = pyqtSignal(str)
    finished = pyqtSignal(bool, str)

    def __init__(self, port, chip_type, firmware_file, bootloader_file, partition_table_file):
        super().__init__()
        self.port = port
        self.chip_type = chip_type
        self.firmware_file = firmware_file
        self.bootloader_file = bootloader_file
        self.partition_table_file = partition_table_file

    def run(self):
        try:
            flash_command = [
                'python', '-m', 'esptool', '--chip', self.chip_type,
                '--port', self.port, '--baud', '921600', '--before', 'default_reset',
                '--after', 'hard_reset', 'write_flash',
                '0x0000', self.bootloader_file,
                '0x8000', self.partition_table_file,
                '0x10000', self.firmware_file
            ]

            process = subprocess.Popen(
                flash_command,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True
            )

            while True:
                line = process.stdout.readline()
                if line:
                    self.log_output.emit(line.strip())
                if process.poll() is not None:
                    break

            process.wait()
            if process.returncode == 0:
                self.finished.emit(True, "烧录成功！")
            else:
                stderr_output = process.stderr.read()
                self.finished.emit(False, f"烧录失败: {stderr_output.strip()}")
        except Exception as e:
            self.finished.emit(False, f"烧录异常: {str(e)}")


class SerialMonitorThread(QThread):
    log_output = pyqtSignal(str)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.serial_connection = None
        self.keep_running = True

    def run(self):
        try:
            self.serial_connection = serial.Serial(self.port, baudrate=115200, timeout=1)
            self.log_output.emit(f"[串口]: 打开串口 {self.port}")

            while self.keep_running:
                if self.serial_connection.in_waiting > 0:
                    data = self.serial_connection.read_all().decode('utf-8', errors='ignore')
                    self.log_output.emit(data)
                time.sleep(0.1)
        except Exception as e:
            self.log_output.emit(f"[串口]: 串口异常: {str(e)}")
        finally:
            if self.serial_connection:
                self.serial_connection.close()
                self.log_output.emit(f"[串口]: 关闭串口 {self.port}")

    def stop(self):
        self.keep_running = False
        if self.serial_connection:
            self.serial_connection.close()


class FlashToolGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('固件烧录工具')
        self.resize(800, 600)
        self.layout = QVBoxLayout()

        # 状态标签
        self.status_label = QLabel("")
        self.layout.addWidget(self.status_label)

        # 串口选择
        self.port_combobox = QComboBox()
        self.populate_serial_ports()
        self.layout.addWidget(self.port_combobox)

        # 刷新串口按钮
        self.refresh_button = QPushButton("刷新串口")
        self.refresh_button.clicked.connect(self.populate_serial_ports)
        self.layout.addWidget(self.refresh_button)

        # 烧录 USB 固件按钮
        self.usb_button = QPushButton("烧录 USB 固件")
        self.usb_button.clicked.connect(lambda: self.start_flash("USB"))
        self.layout.addWidget(self.usb_button)

        # 烧录 WIFI 固件按钮
        self.wifi_button = QPushButton("烧录 WIFI 固件")
        self.wifi_button.clicked.connect(lambda: self.start_flash("WIFI"))
        self.layout.addWidget(self.wifi_button)

        # 打开串口按钮
        self.open_serial_button = QPushButton("打开串口")
        self.open_serial_button.clicked.connect(self.open_serial)
        self.layout.addWidget(self.open_serial_button)

        # 重启设备按钮
        self.restart_button = QPushButton("重启设备")
        self.restart_button.clicked.connect(self.restart_device)
        self.layout.addWidget(self.restart_button)

        # 日志窗口
        self.log_text_edit = QTextEdit()
        self.log_text_edit.setReadOnly(True)
        self.layout.addWidget(self.log_text_edit)

        self.setLayout(self.layout)

        # 固定的资源文件路径
        self.resources_dir = self.get_resource_path("resources")
        self.bootloader_file = os.path.join(self.resources_dir, "bootloader.bin")
        self.partition_table_file = os.path.join(self.resources_dir, "partition-table.bin")
        self.usb_firmware = os.path.join(self.resources_dir, "USB.bin")
        self.wifi_firmware = os.path.join(self.resources_dir, "WIFI.bin")

        # 串口监听线程
        self.serial_monitor_thread = None

    def get_resource_path(self, relative_path):
        if hasattr(sys, '_MEIPASS'):
            return os.path.join(sys._MEIPASS, relative_path)
        return os.path.join(os.path.abspath("."), relative_path)

    def populate_serial_ports(self):
        self.port_combobox.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combobox.addItem(port.device)
        if not ports:
            self.port_combobox.addItem("未检测到串口")

    def start_flash(self, firmware_type):
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        firmware_file = self.usb_firmware if firmware_type == "USB" else self.wifi_firmware
        chip_type = "esp32s3"

        if not os.path.exists(self.bootloader_file) or not os.path.exists(self.partition_table_file):
            self.update_log("[错误]: Bootloader 或 Partition Table 文件不存在！")
            return

        self.status_label.setText(f"正在烧录 {firmware_type} 固件...")
        self.usb_button.setEnabled(False)
        self.wifi_button.setEnabled(False)

        if self.serial_monitor_thread:
            self.serial_monitor_thread.stop()
            self.serial_monitor_thread = None

        self.flash_thread = FlashThread(port, chip_type, firmware_file, self.bootloader_file, self.partition_table_file)
        self.flash_thread.log_output.connect(self.update_log)
        self.flash_thread.finished.connect(lambda success, message: self.flash_finished(success, message, port))
        self.flash_thread.start()

    def open_serial(self, port=None):
        port = port or self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        if self.serial_monitor_thread:
            self.serial_monitor_thread.stop()

        self.serial_monitor_thread = SerialMonitorThread(port)
        self.serial_monitor_thread.log_output.connect(self.update_log)
        self.serial_monitor_thread.start()

    def restart_device(self):
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        try:
            if self.serial_monitor_thread:
                self.serial_monitor_thread.stop()

            with serial.Serial(port, baudrate=115200, timeout=1) as ser:
                ser.dtr = False
                time.sleep(0.1)
                ser.dtr = True
                self.update_log("[设备]: 重启成功！")

            self.open_serial(port)
        except Exception as e:
            self.update_log(f"[设备]: 重启失败: {str(e)}")

    def update_log(self, message):
        self.log_text_edit.append(message)

    def flash_finished(self, success, message, port):
        self.usb_button.setEnabled(True)
        self.wifi_button.setEnabled(True)
        self.status_label.setText(message)

        if success:
            self.update_log("[系统]: 烧录完成，自动启动串口监听。")
            self.open_serial(port)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FlashToolGUI()
    window.show()
    sys.exit(app.exec_())
