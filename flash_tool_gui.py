import sys
import os
import subprocess  # 用于调用 esptool
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton,
    QFileDialog, QComboBox, QLabel, QMessageBox, QTextEdit
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
import serial.tools.list_ports
import time
import re

class FlashThread(QThread):
    progress = pyqtSignal(int)  # 传递烧录进度
    log_output = pyqtSignal(str)  # 实时日志输出
    finished = pyqtSignal(bool, str)  # 通知烧录完成

    def __init__(self, port, firmware_path, chip_type):
        super().__init__()
        self.port = port
        self.firmware_path = firmware_path
        self.chip_type = chip_type  # 芯片类型（esp32 或 esp32s3）
        self.serial_connection = None  # 串口连接对象
        self.keep_reading_serial = True  # 控制串口读取的标志位

    def run(self):
        try:
            if not os.path.exists(self.firmware_path):
                self.finished.emit(False, "固件文件不存在！")
                return

            # 确保串口关闭，释放资源
            self._close_serial()

            # 启动子进程运行 esptool
            process = subprocess.Popen(
                ['python', '-m', 'esptool', '--chip', self.chip_type,
                 '--port', self.port, '--baud', '460800', '--before', 'default_reset',
                 '--after', 'hard_reset', 'write_flash', '-z', '0x10000', self.firmware_path],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True
            )

            # 读取烧录日志
            while True:
                line = process.stdout.readline()
                if line:
                    self.log_output.emit(line.strip())
                if process.poll() is not None:
                    break

            process.wait()

            # 检查烧录结果
            if process.returncode == 0:
                self.finished.emit(True, "烧录成功！")
            else:
                stderr_output = process.stderr.read()
                self.finished.emit(False, f"烧录失败: {stderr_output.strip()}")

            # 重新打开串口监听
            self._reopen_serial()
        except Exception as e:
            self.finished.emit(False, f"烧录异常: {str(e)}")
        finally:
            # 最后确保串口被释放
            self._close_serial()

    def _reopen_serial(self):
        """重新打开串口并开始读取数据"""
        try:
            self.serial_connection = serial.Serial(self.port, baudrate=115200, timeout=1)
            self.log_output.emit(f"[串口]: 烧录后重新打开串口 {self.port}")

            # 自动重启芯片（发送 RTS 信号）
            self.serial_connection.dtr = False  # 拉低 DTR
            time.sleep(0.1)
            self.serial_connection.dtr = True  # 拉高 DTR
            self.log_output.emit("[串口]: 已自动重启芯片")

            # 持续读取串口数据
            buffer = ""  # 用于存储批量读取的数据
            while self.keep_reading_serial:
                if self.serial_connection.in_waiting > 0:
                    # 一次性读取串口所有数据
                    serial_data = self.serial_connection.read_all().decode('utf-8', errors='ignore')
                    buffer += serial_data

                    # 将缓冲区中的数据按行拆分并发送到日志
                    lines = buffer.split("\n")
                    for line in lines[:-1]:  # 处理完整的行
                        self.log_output.emit(f"[串口]: {line.strip()}")
                    buffer = lines[-1]  # 保留不完整的行

                time.sleep(0.01)
        except Exception as e:
            self.log_output.emit(f"[串口]: 重新打开串口失败: {str(e)}")

    def _close_serial(self):
        """确保串口被正确释放"""
        if self.serial_connection:
            try:
                self.serial_connection.close()
                self.log_output.emit(f"[串口]: 串口 {self.port} 已释放")
            except Exception as e:
                self.log_output.emit(f"[串口]: 关闭串口时出错: {str(e)}")
            finally:
                self.serial_connection = None

    def stop_serial_reading(self):
        """停止串口读取"""
        self.keep_reading_serial = False
        self._close_serial()


class FlashToolGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('paper_face_tracker固件烧录')
        self.resize(800, 600)  # 设置初始窗口大小
        self.layout = QVBoxLayout()

        # 串口选择
        self.port_label = QLabel("选择串口:")
        self.layout.addWidget(self.port_label)

        self.port_combobox = QComboBox()
        self.populate_serial_ports()  # 初始化时填充串口列表
        self.layout.addWidget(self.port_combobox)

        # 刷新串口按钮
        self.refresh_button = QPushButton("刷新串口")
        self.refresh_button.clicked.connect(self.populate_serial_ports)
        self.layout.addWidget(self.refresh_button)

        # 芯片类型选择
        self.chip_label = QLabel("选择芯片类型:")
        self.layout.addWidget(self.chip_label)

        self.chip_combobox = QComboBox()
        self.chip_combobox.addItems(["esp32", "esp32s3"])  # 添加芯片类型
        self.chip_combobox.setCurrentText("esp32s3")  # 设置默认芯片类型为 esp32s3
        self.layout.addWidget(self.chip_combobox)

        # 固件选择按钮
        self.firmware_button = QPushButton('选择固件文件')
        self.firmware_button.clicked.connect(self.select_firmware)
        self.layout.addWidget(self.firmware_button)

        # 显示选择的固件文件路径
        self.firmware_label = QLabel("未选择固件文件")
        self.firmware_label.setWordWrap(True)
        self.layout.addWidget(self.firmware_label)

        # 烧录按钮
        self.flash_button = QPushButton('开始烧录')
        self.flash_button.clicked.connect(self.start_flash)
        self.layout.addWidget(self.flash_button)

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
        """填充串口列表"""
        self.port_combobox.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combobox.addItem(port.device)
        if not ports:
            self.port_combobox.addItem("未检测到串口")

    def select_firmware(self):
        """选择固件文件"""
        file_path, _ = QFileDialog.getOpenFileName(self, '选择固件文件', '', 'BIN Files (*.bin)')
        if file_path:
            self.firmware_path = file_path
            display_path = file_path if len(file_path) < 50 else '...' + file_path[-47:]
            self.firmware_label.setText(f"选择的固件文件: {display_path}")

    def start_flash(self):
        """启动烧录线程"""
        port = self.port_combobox.currentText()
        chip_type = self.chip_combobox.currentText()
        if not port or port == "未检测到串口":
            QMessageBox.warning(self, "警告", "请选择有效的串口。")
            return
        if not self.firmware_path:
            QMessageBox.warning(self, "警告", "请选择要烧录的固件文件。")
            return

        self.flash_button.setEnabled(False)
        self.status_label.setText("准备烧录...")

        self.flash_thread = FlashThread(port, self.firmware_path, chip_type)
        self.flash_thread.log_output.connect(self.update_log)
        self.flash_thread.finished.connect(self.flash_finished)
        self.flash_thread.start()

    def update_log(self, message):
        """更新日志"""
        self.log_text_edit.append(message)

    def flash_finished(self, success, message):
        """烧录完成后的操作"""
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
