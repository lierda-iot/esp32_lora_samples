# LRMAM36Pico 开发板上手指南

## 概述

**L-LRMAM36-FANN4-PK02**（LRMAM36Pico）是基于利尔达 AM36 模组（L-LRMAM36-FANN4）的评估开发板，板载 ESP32-S3 主控和 LR2021 射频芯片，可用于快速评估 LoRa 射频性能及二次开发。

<!-- TODO: 插入开发板正面实物图 -->
![开发板正面](images/board_front.png)

<!-- TODO: 插入开发板背面实物图 -->
![开发板背面](images/board_back.png)

## 硬件介绍

### 板载资源

| 资源 | 说明 |
|------|------|
| 主控芯片 | ESP32-S3 |
| 射频芯片 | LR2021（LR20xx 系列） |
| Flash | 2 MB |
| 供电方式 | USB Type-C（5V） |
| 调试/烧录接口 | USB Type-C |
| 串口 | UART1 排针（TX: GPIO47，RX: GPIO48） |

### 接口说明

<!-- TODO: 插入开发板接口标注图 -->
![接口标注](images/board_interfaces.png)

| 序号 | 接口 | 说明 |
|------|------|------|
| 1 | USB Type-C | 供电、固件烧录、ESP 日志输出（UART0，115200 波特率） |
| 2 | UART1 排针 | 外接串口工具进行通信调试（921600 波特率） |
| 3 | BOOT 按键 | 按住后上电或复位，进入下载模式 |
| 4 | RESET 按键 | 系统复位 |

### 射频引脚映射

| 功能 | GPIO |
|------|------|
| SPI MOSI | 41 |
| SPI MISO | 42 |
| SPI CLK | 40 |
| SPI CS (NSS) | 39 |
| Radio NRST | 38 |
| Radio BUSY | 17 |
| Radio DIO7 | 21 |

### UART1 排针定义

| 引脚 | GPIO | 说明 |
|------|------|------|
| TX | GPIO 47 | 串口发送 |
| RX | GPIO 48 | 串口接收 |
| GND | — | 接地 |

## 开发环境搭建

本项目基于 **ESP-IDF v5.0 或更高版本** 构建。

### 第一步：安装 ESP-IDF

根据操作系统选择对应的安装方式：

- **Windows**：推荐使用 [ESP-IDF Windows 离线安装包](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/windows-setup.html)，安装完成后会在桌面生成 **ESP-IDF Command Prompt** 快捷方式。
- **macOS / Linux**：参考 [ESP-IDF Linux/macOS 安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/linux-macos-setup.html)。

### 第二步：验证安装

打开 ESP-IDF 命令行终端，执行以下命令确认环境配置成功：

```bash
idf.py --version
```

如果正确输出版本号（如 `v5.x.x`），说明环境已就绪。

## 编译

### 第一步：获取示例代码

```bash
git clone <仓库地址>
cd idf_lr2021_rf_test-main
```

### 第二步：设置目标芯片

```bash
idf.py set-target esp32s3
```

### 第三步：编译项目

```bash
idf.py build
```

> 首次编译时，构建系统会自动从 [ESP 组件仓库](https://components.espressif.com/) 下载 `lierda-iot/esp_lora_driver` 驱动组件，请确保网络畅通。

编译成功后终端会显示：

```
Project build complete. To flash, run:
 idf.py flash
```

## 烧录

### 第一步：连接开发板

使用 USB Type-C 数据线将开发板连接至电脑。

> **注意**：请确保使用的是数据线而非充电线（充电线不带数据通道，无法识别设备）。

连接成功后，系统会自动识别出一个串口设备：
- **Windows**：在设备管理器中查看 COM 端口号（如 `COM3`）
- **Linux**：通常为 `/dev/ttyUSB0` 或 `/dev/ttyACM0`
- **macOS**：通常为 `/dev/cu.usbserial-xxxx`

### 第二步：烧录固件

```bash
idf.py flash
```

如果电脑连接了多个串口设备，需手动指定端口：

```bash
# Windows
idf.py -p COM3 flash

# Linux
idf.py -p /dev/ttyUSB0 flash

# macOS
idf.py -p /dev/cu.usbserial-xxxx flash
```

### 无法烧录时的处理

如果烧录失败，尝试手动进入下载模式：

1. **按住 BOOT 按键不松**
2. **按一下 RESET 按键**
3. **松开 BOOT 按键**
4. 重新执行 `idf.py flash`

## 串口调试

开发板有两路串口输出，用途不同：

| 串口 | 接口 | 波特率 | 用途 |
|------|------|--------|------|
| UART0 | USB Type-C | 115200 | ESP-IDF 系统日志（`ESP_LOG` 输出） |
| UART1 | 外接排针 | 921600 | 用户应用通信/调试 |

### 查看系统日志（UART0）

系统日志通过 USB Type-C 接口直接输出，无需额外接线：

```bash
idf.py monitor
```

按 `Ctrl+]` 退出。也可以编译、烧录、查看日志一步完成：

```bash
idf.py flash monitor
```

### 使用 UART1 调试（外接串口）

UART1 需要通过外接 USB 转串口适配器（如 CP2102、CH340、FT232）连接。

#### 接线方式

| USB 转串口适配器 | 开发板排针 |
|------------------|-----------|
| TXD | GPIO 48（UART1 RX） |
| RXD | GPIO 47（UART1 TX） |
| GND | GND |

> **注意**：TX 接 RX，RX 接 TX，交叉连接。

#### 串口终端配置

| 参数 | 值 |
|------|------|
| 波特率 | 921600 |
| 数据位 | 8 |
| 校验位 | 无 |
| 停止位 | 1 |
| 流控 | 无 |

推荐的串口终端工具：
- **Windows**：PuTTY、MobaXterm、Tera Term
- **macOS / Linux**：minicom、picocom、screen

Linux 示例：

```bash
picocom -b 921600 /dev/ttyUSB0
```

## 常见问题

### 编译时提示找不到组件

确保首次编译时网络畅通。`lierda-iot/esp_lora_driver` 组件会从 ESP 组件仓库自动下载。如在代理环境下，需配置代理。

### USB 连接后设备管理器中无串口

- 检查数据线是否支持数据传输
- 尝试更换 USB 端口
- 检查是否需要安装 USB 驱动（部分系统需手动安装 CP210x 或 CH34x 驱动）

### 烧录失败

- 手动进入下载模式（参考上方 BOOT + RESET 操作）
- 确认选择了正确的串口号
- 关闭其他占用该串口的程序

### UART1 无输出

- 确认 TX/RX 交叉接线正确
- 确认波特率设为 **921600**
- 确认固件已正确烧录

## 更多资源

- [ESP-IDF 编程指南（中文）](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/)
- [ESP 组件仓库 — esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)
