import glob
import sys
import os
import subprocess
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton,
    QLabel, QTextEdit, QComboBox, QFileDialog, QMessageBox, QGroupBox, QHBoxLayout, QProgressBar
)
from PyQt5.QtCore import QThread, pyqtSignal, QTimer
from PyQt5.QtGui import QFont
import serial.tools.list_ports
import serial
import time

class FlashThread(QThread):
    log_output = pyqtSignal(str)
    finished = pyqtSignal(bool, str)
    update_progress = pyqtSignal(int)  # 新增信号，用于更新进度条

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

                    # 检查是否包含进度信息
                    if "Writing at" in line and "%" in line:
                        # 提取进度百分比
                        percentage = int(line.split('(')[1].split('%')[0].strip())
                        self.update_progress.emit(percentage)  # 更新进度条

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

class EraseFlashThread(QThread):
    log_output = pyqtSignal(str)
    finished = pyqtSignal(bool, str)

    def __init__(self, port, chip_type):
        super().__init__()
        self.port = port
        self.chip_type = chip_type

    def run(self):
        try:
            erase_command = [
                'python', '-m', 'esptool', '--chip', self.chip_type,
                '--port', self.port, 'erase_flash'
            ]

            process = subprocess.Popen(
                erase_command,
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
                self.finished.emit(True, "擦除成功！")
            else:
                stderr_output = process.stderr.read()
                self.finished.emit(False, f"擦除失败: {stderr_output.strip()}")
        except Exception as e:
            self.finished.emit(False, f"擦除异常: {str(e)}")

class SerialMonitorThread(QThread):
    log_output = pyqtSignal(str)
    camera_error = pyqtSignal()  # 新增信号，用于通知主线程弹出错误对话框

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

                    # 检查是否包含摄像头初始化失败的错误信息
                    if "MAIN: Camera initialization failed!" in data:
                        self.camera_error.emit()  # 发送信号通知主线程

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

        # 创建按钮的组
        self.create_button_group()

        # 进度条
        self.progress_bar = QProgressBar(self)
        self.progress_bar.setRange(0, 100)
        self.layout.addWidget(self.progress_bar)

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

        # 用户选择的 app.bin 文件路径
        self.app_firmware = None

        # 串口监听线程
        self.serial_monitor_thread = None

        # 调整字体和按钮样式
        self.adjust_ui()

    def adjust_ui(self):
        # 设置字体大小
        font = QFont("Arial", 12)
        self.status_label.setFont(font)
        self.port_combobox.setFont(font)
        self.refresh_button.setFont(font)
        self.usb_button.setFont(font)
        self.wifi_button.setFont(font)
        self.select_app_button.setFont(font)
        self.erase_button.setFont(font)
        self.open_serial_button.setFont(font)
        self.restart_button.setFont(font)

        # 设置按钮的样式
        button_style = """
        QPushButton {
            background-color: #D3D3D3;  /* 使用柔和的灰色 */
            color: black;
            font-size: 14px;
            border-radius: 10px;
            padding: 10px;
            min-width: 150px;
        }
        QPushButton:hover {
            background-color: #A9A9A9;  /* 悬停时变为较深灰色 */
        }
        """
        self.refresh_button.setStyleSheet(button_style)
        self.usb_button.setStyleSheet(button_style)
        self.wifi_button.setStyleSheet(button_style)
        self.select_app_button.setStyleSheet(button_style)
        self.erase_button.setStyleSheet(button_style)
        self.open_serial_button.setStyleSheet(button_style)
        self.restart_button.setStyleSheet(button_style)

    def create_button_group(self):
        # 创建一个新的 QGroupBox，用来组织按钮
        button_group_box = QGroupBox("操作按钮")
        button_layout = QVBoxLayout()

        self.usb_button = QPushButton("烧录有线面捕固件")
        self.usb_button.clicked.connect(lambda: self.start_flash("USB"))
        button_layout.addWidget(self.usb_button)

        self.wifi_button = QPushButton("烧录无线面捕固件")
        self.wifi_button.clicked.connect(lambda: self.start_flash("WIFI"))
        button_layout.addWidget(self.wifi_button)

        self.select_app_button = QPushButton("选择定制固件")
        self.select_app_button.clicked.connect(self.select_app_file)
        button_layout.addWidget(self.select_app_button)

        self.erase_button = QPushButton("清空所有数据")
        self.erase_button.clicked.connect(self.erase_flash)
        button_layout.addWidget(self.erase_button)

        self.open_serial_button = QPushButton("打开串口")
        self.open_serial_button.clicked.connect(self.open_serial)
        button_layout.addWidget(self.open_serial_button)

        self.restart_button = QPushButton("重启设备")
        self.restart_button.clicked.connect(self.restart_device)
        button_layout.addWidget(self.restart_button)

        button_group_box.setLayout(button_layout)
        self.layout.addWidget(button_group_box)

    def get_resource_path(self, relative_path):
        if hasattr(sys, '_MEIPASS'):
            return os.path.join(sys._MEIPASS, relative_path)
        return os.path.join(os.path.abspath("."), relative_path)

    def select_app_file(self):
        # 打开文件对话框让用户选择 app.bin 文件
        options = QFileDialog.Options()
        file, _ = QFileDialog.getOpenFileName(self, "选择 app.bin 文件", "", "Bin Files (*.bin);;All Files (*)", options=options)
        if file:
            self.app_firmware = file
            self.update_log(f"选择的 app.bin 文件: {file}")

    def populate_serial_ports(self):
        self.port_combobox.clear()  # 清空现有的串口选项

        ports = serial.tools.list_ports.comports()
        if not ports:
            self.port_combobox.addItem("未检测到串口")
        else:
            for port in ports:
                port_info = f"{port.device} - {port.description}"  # 显示端口号和描述信息
                self.port_combobox.addItem(port.device)  # 只添加端口号

    def start_flash(self, firmware_type):
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        # 如果用户选择了 app.bin 文件，则使用选择的文件
        firmware_file = self.app_firmware if self.app_firmware else (self.usb_firmware if firmware_type == "USB" else self.wifi_firmware)
        
        if not firmware_file:
            self.update_log("[错误]: 未选择固件文件！")
            return

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
        self.flash_thread.update_progress.connect(self.update_progress_bar)  # 连接进度条更新信号
        self.flash_thread.start()

    def erase_flash(self):
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        chip_type = "esp32s3"

        self.status_label.setText("正在擦除 Flash...")
        self.usb_button.setEnabled(False)
        self.wifi_button.setEnabled(False)
        self.erase_button.setEnabled(False)

        if self.serial_monitor_thread:
            self.serial_monitor_thread.stop()
            self.serial_monitor_thread = None

        self.erase_thread = EraseFlashThread(port, chip_type)
        self.erase_thread.log_output.connect(self.update_log)
        self.erase_thread.finished.connect(self.erase_finished)
        self.erase_thread.start()

    def update_progress_bar(self, percentage):
        # 更新进度条
        self.progress_bar.setValue(percentage)

    def erase_finished(self, success, message):
        self.usb_button.setEnabled(True)
        self.wifi_button.setEnabled(True)
        self.erase_button.setEnabled(True)
        self.status_label.setText(message)

        if success:
            self.update_log("[系统]: 擦除完成。")
        else:
            self.update_log(f"[错误]: {message}")

    def open_serial(self, port=None):
        port = port or self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            self.update_log("[错误]: 请选择有效的串口！")
            return

        if self.serial_monitor_thread:
            self.serial_monitor_thread.stop()

        self.serial_monitor_thread = SerialMonitorThread(port)
        self.serial_monitor_thread.log_output.connect(self.update_log)
        self.serial_monitor_thread.camera_error.connect(self.show_camera_error_dialog)  # 连接信号
        self.serial_monitor_thread.start()

    def show_camera_error_dialog(self):
        # 弹出错误消息框
        msg = QMessageBox()
        msg.setIcon(QMessageBox.Critical)
        msg.setText("摄像头初始化失败！")
        msg.setInformativeText("请检查接线,如果无法解决问题请联系淘宝远程售后")
        msg.setWindowTitle("错误")
        msg.exec_()

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
