import sys
import os
import subprocess  # 用于调用 esptool
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton,
    QFileDialog, QComboBox, QLabel, QMessageBox, QTextEdit, QLineEdit
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
import serial.tools.list_ports
import serial  # 用于串口通信


class FlashThread(QThread):
    progress = pyqtSignal(int)  # 传递烧录进度
    log_output = pyqtSignal(str)  # 实时日志输出
    finished = pyqtSignal(bool, str)  # 通知烧录完成

    def __init__(self, port, firmware_path):
        super().__init__()
        self.port = port
        self.firmware_path = firmware_path

    def run(self):
        try:
            # 检查固件文件是否存在
            if not os.path.exists(self.firmware_path):
                self.finished.emit(False, "固件文件不存在！")
                return

            # 启动子进程运行 esptool
            process = subprocess.Popen(
                ['python', '-m', 'esptool', '--chip', 'esp32',
                 '--port', self.port, '--baud', '460800', '--before', 'default_reset',
                 '--after', 'hard_reset', 'write_flash', '-z', '0x1000', self.firmware_path],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True
            )

            # 解析子进程的输出
            for line in process.stdout:
                self.log_output.emit(line.strip())  # 实时发送日志到主线程
                # 解析烧录进度
                if "%" in line:
                    try:
                        progress = int(line.strip().split('%')[0].split()[-1])
                        self.progress.emit(progress)  # 更新进度条
                    except ValueError:
                        continue

            process.wait()

            # 根据返回码判断烧录是否成功
            if process.returncode == 0:
                self.finished.emit(True, "烧录成功！")
            else:
                stderr_output = process.stderr.read()
                self.finished.emit(False, f"烧录失败: {stderr_output.strip()}")
        except Exception as e:
            self.finished.emit(False, f"烧录异常: {str(e)}")


class FlashToolGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('ESP32 Flash Tool with WiFi Configuration')
        self.setFixedSize(600, 500)  # 调整窗口大小
        self.layout = QVBoxLayout()

        # 串口选择
        self.port_label = QLabel("选择串口:")
        self.layout.addWidget(self.port_label)

        self.port_combobox = QComboBox()
        self.populate_serial_ports()
        self.layout.addWidget(self.port_combobox)

        # 烧录部分
        self.firmware_button = QPushButton('选择固件文件')
        self.firmware_button.clicked.connect(self.select_firmware)
        self.layout.addWidget(self.firmware_button)

        self.firmware_label = QLabel("未选择固件文件")
        self.firmware_label.setWordWrap(True)
        self.layout.addWidget(self.firmware_label)

        self.flash_button = QPushButton('开始烧录')
        self.flash_button.clicked.connect(self.start_flash)
        self.layout.addWidget(self.flash_button)

        # WiFi 配置部分
        self.wifi_label = QLabel("配置 WiFi:")
        self.layout.addWidget(self.wifi_label)

        self.ssid_input = QLineEdit()
        self.ssid_input.setPlaceholderText("输入 WiFi 名称 (SSID)")
        self.layout.addWidget(self.ssid_input)

        self.password_input = QLineEdit()
        self.password_input.setPlaceholderText("输入 WiFi 密码")
        self.password_input.setEchoMode(QLineEdit.Password)  # 设置为密码隐藏模式
        self.layout.addWidget(self.password_input)

        self.wifi_button = QPushButton("配置 WiFi")
        self.wifi_button.clicked.connect(self.configure_wifi)
        self.layout.addWidget(self.wifi_button)

        # 日志输出窗口
        self.log_text_edit = QTextEdit()
        self.log_text_edit.setReadOnly(True)  # 设置为只读
        self.layout.addWidget(self.log_text_edit)

        # 状态标签
        self.status_label = QLabel("")
        self.layout.addWidget(self.status_label)

        self.setLayout(self.layout)
        self.firmware_path = ""

    def populate_serial_ports(self):
        self.port_combobox.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combobox.addItem(port.device)
        if not ports:
            self.port_combobox.addItem("未检测到串口")

    def configure_wifi(self):
        """通过串口发送 WiFi 配置"""
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            QMessageBox.warning(self, "警告", "请选择有效的串口。")
            return

        ssid = self.ssid_input.text().strip()
        password = self.password_input.text().strip()

        if not ssid or not password:
            QMessageBox.warning(self, "警告", "WiFi 名称和密码不能为空。")
            return

        try:
            # 打开串口
            ser = serial.Serial(port, baudrate=115200, timeout=5)
            wifi_config = f"SSID:{ssid};PWD:{password}\n"
            ser.write(wifi_config.encode('utf-8'))  # 发送 WiFi 配置
            ser.close()
            QMessageBox.information(self, "成功", "WiFi 配置已发送！请等待设备连接。")
        except Exception as e:
            QMessageBox.critical(self, "错误", f"无法发送 WiFi 配置：{str(e)}")

    def select_firmware(self):
        file_path, _ = QFileDialog.getOpenFileName(self, '选择固件文件', '', 'BIN Files (*.bin)')
        if file_path:
            self.firmware_path = file_path
            display_path = file_path if len(file_path) < 50 else '...' + file_path[-47:]
            self.firmware_label.setText(f"选择的固件文件: {display_path}")

    def start_flash(self):
        port = self.port_combobox.currentText()
        if not port or port == "未检测到串口":
            QMessageBox.warning(self, "警告", "请选择有效的串口。")
            return
        if not self.firmware_path:
            QMessageBox.warning(self, "警告", "请选择要烧录的固件文件。")
            return

        self.flash_button.setEnabled(False)
        self.status_label.setText("准备烧录...")

        # 启动烧录线程
        self.flash_thread = FlashThread(port, self.firmware_path)
        self.flash_thread.log_output.connect(self.update_log)  # 连接日志输出信号
        self.flash_thread.finished.connect(self.flash_finished)
        self.flash_thread.start()

    def update_log(self, message):
        self.log_text_edit.append(message)  # 将日志追加到文本框

    def flash_finished(self, success, message):
        self.flash_button.setEnabled(True)
        if success:
            QMessageBox.information(self, "成功", message)
        else:
            QMessageBox.critical(self, "失败", message)
        self.status_label.setText(message)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FlashToolGUI()
    window.show()
    sys.exit(app.exec_())
