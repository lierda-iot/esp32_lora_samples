# Lierda AM36 Pico LoRa Example

Language: 中文 | [English](README_EN.md)

这是一个基于 ESP-IDF 的 LR2021 LoRa 示例工程，用于在 Lierda AM36 Pico 开发板 L-LRMAM36-FANN4-PK02 上验证 LoRa 点对点通信、射频参数配置和串口交互调试。

L-LRMAM36-FANN4-PK02 是基于 Lierda AM36 模组 L-LRMAM36-FANN4 的评估开发板。开发板出厂已贴装 AM36 模组，模组基于 ESP32-S3 与 LR2021 设计，支持 Wi-Fi、BLE、Generation 4 LoRa、FLRC、2-FSK/4-FSK 等无线能力，可用于 LoRa 射频性能评估和应用开发。

## 目录

- [开发板概览](#开发板概览)
- [接口说明](#接口说明)
- [硬件资料](#硬件资料)
- [开发环境](#开发环境)
- [快速开始](#快速开始)
- [烧录说明](#烧录说明)
- [UART 调试](#uart-调试)
- [RF 参数与 CLI](#rf-参数与-cli)
- [项目结构](#项目结构)
- [常见问题](#常见问题)
- [安全须知](#安全须知)
- [更多资源](#更多资源)

## 开发板概览

| 正面 | 背面 |
| --- | --- |
| <img src="docs/pic/Front.png" alt="开发板正面" height="420"> | <img src="docs/pic/Back.png" alt="开发板背面" height="420"> |

| 项目 | 说明 |
| --- | --- |
| 推荐开发板 | Lierda AM36 Pico L-LRMAM36-FANN4-PK02 |
| 搭载模组 | L-LRMAM36-FANN4，出厂已贴装 |
| 主控与射频芯片 | ESP32-S3 + LR2021 |
| 无线能力 | Wi-Fi、BLE、Generation 4 LoRa、FLRC、2-FSK/4-FSK |
| 工作频段 | LoRa: 863 MHz 至 930 MHz；Wi-Fi/BLE: 2400 MHz 至 2500 MHz |
| 模组尺寸 | 20 mm x 20 mm x 2.5 mm |
| 内部存储 | 8 MB Flash + 8 MB PSRAM |
| USB 转串口 | 板载 CP2105 双通道 USB-to-UART 芯片 |

开发套件通常包含：

| 序号 | 名称 | 数量 | 说明 |
| --- | --- | --- | --- |
| 1 | AM36 Pico 开发板 L-LRMAM36-FANN4-PK02 | 1 块 | 出厂已贴装 AM36 模组 L-LRMAM36-FANN4。 |
| 2 | LoRa 天线 | 1 根 | 用于 LoRa 射频通信和测试。 |
| 3 | USB Type-C 数据线 | 1 根 | 用于 USB 烧录或 UART 串口调试，请使用支持数据传输的线缆。 |

## 接口说明

<img src="docs/pic/Interface.png" alt="开发板接口示意图" width="760">

开发板提供两个 Type-C 接口：`USB` 口连接 AM36 模组 USB，用于固件烧录和 USB-JTAG/Serial 调试；`UART` 口通过板载 CP2105 双通道 USB-to-UART 芯片引出两路串口，用于示例工程 CLI 调试和用户应用串口扩展。

| 接口/组件 | 说明 |
| --- | --- |
| LoRa 天线接口 | SMA 接口，用于连接标配 LoRa 天线。 |
| Wi-Fi 板载天线 | 板载 PCB 天线。 |
| USB | Type-C 接口，连接 AM36 模组 USB，用于固件烧录和 USB-JTAG/Serial 调试。 |
| UART | Type-C 接口，板载 CP2105 双通道 USB-to-UART 芯片；连接 PC 后枚举 Enhanced COM Port 和 Standard COM Port。 |
| GPIO | AM36 模组 GPIO 引出接口，可按软件配置复用；详细引脚功能请参考模组硬件设计手册。 |
| RST | 复位按键，低电平有效。 |
| BOOT | 下载模式按键，用于配合 RESET 进入烧录下载模式。 |
| KEY | 用户自定义按键，具体功能由用户应用定义。 |

## 硬件资料

`docs/` 目录提供开发板级和模组级参考资料：

| 文件 | 说明 |
| --- | --- |
| [L-LRMAM36-FANN4-PK02_SCH_V01.pdf](docs/L-LRMAM36-FANN4-PK02_SCH_V01.pdf) | 开发板原理图，用于查看 AM36 Pico 开发板的电气连接关系和信号走线。 |
| [L-LRMAM36-FANN4-PK02_layout_V01.pdf](docs/L-LRMAM36-FANN4-PK02_layout_V01.pdf) | 开发板 layout 文件，用于查看 PCB 器件布局、走线和板级实现细节。 |
| [L-LRMAM36-FANN4_V01.step](docs/L-LRMAM36-FANN4_V01.step) | AM36 模组 3D 结构模型，用于外壳适配、结构检查和安装参考。 |
| [Lierda L-LRMAM36-FANN4 Hardware Design Manual_CN_Rev1.0.pdf](docs/Lierda%20L-LRMAM36-FANN4%20Hardware%20Design%20Manual_CN_Rev1.0.pdf) | 中文版模组硬件设计手册，用于查看模组规格、引脚定义、电气特性和集成设计说明。 |
| [Lierda L-LRMAM36-FANN4 Hardware Design Manual_EN_Rev1.0.pdf](docs/Lierda%20L-LRMAM36-FANN4%20Hardware%20Design%20Manual_EN_Rev1.0.pdf) | 英文版模组硬件设计手册，适合英文环境下的硬件集成参考。 |

## 开发环境

- ESP-IDF v5.0 或更高版本
- [lierda-iot/esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)

`esp_lora_driver` 由 ESP-IDF Component Manager 根据 [main/idf_component.yml](main/idf_component.yml) 自动获取。首次编译时请确保网络可以访问 [ESP Component Registry](https://components.espressif.com/)。

## 快速开始

### 1. 安装 ESP-IDF

- Windows: 推荐使用 [ESP-IDF Windows 安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/windows-setup.html)。
- macOS / Linux: 参考 [ESP-IDF Linux/macOS 安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/linux-macos-setup.html)。

安装完成后，在 ESP-IDF 命令行终端中执行：

```bash
idf.py --version
```

如果输出 v5.x.x 或更高版本，说明环境已就绪。

### 2. 获取代码

```bash
git clone https://github.com/lierda-iot/esp32_lora_samples.git
cd esp32_lora_samples
```

### 3. 编译工程

```bash
idf.py set-target esp32s3
idf.py build
```

编译成功后，终端会提示：

```bash
Project build complete. To flash, run:
idf.py flash
```

## 烧录说明

使用 USB Type-C 数据线将开发板的 `USB` 口连接至电脑。该接口连接 AM36 模组 USB，用于固件烧录和 USB-JTAG/Serial 调试。

请注意：不要使用仅支持充电的 Type-C 线缆，否则电脑无法识别设备。

```bash
idf.py flash
```

如果电脑连接了多个 USB 设备，请手动指定端口：

```bash
# Windows
idf.py -p COM3 flash

# Linux
idf.py -p /dev/ttyACM0 flash

# macOS
idf.py -p /dev/cu.usbmodem-xxxx flash
```

如果烧录失败，可手动进入下载模式后重新烧录：

1. 按住 `BOOT` 按键。
2. 按一下 `RESET` 按键。
3. 松开 `BOOT` 按键。
4. 重新执行 `idf.py flash`。

## UART 调试

本示例的交互式 CLI 通过开发板 `UART` Type-C 口输出。该接口通过板载 CP2105 双通道 USB-to-UART 芯片连接 AM36 模组 ESP32-S3 的两路 UART。示例工程默认使用 `Enhanced COM Port` 对应的 UART1。

| PC 端显示名称 | 对应串口 | 模组引脚 | 调试参数 | 用途 |
| --- | --- | --- | --- | --- |
| Silicon Labs Dual CP2105 USB to UART Bridge: Enhanced COM Port | UART1 | GPIO47/GPIO48 | 921600, 8N1 | 示例工程默认应用通信和 CLI 调试串口 |
| Silicon Labs Dual CP2105 USB to UART Bridge: Standard COM Port | UART0 | GPIO43/GPIO44 | 由用户应用配置 | 第二路串口，可由用户应用自定义 |

调试步骤：

1. 安装 CP210x USB-to-UART 驱动。
2. 使用 Type-C 数据线将开发板 `UART` 口连接至电脑。
3. 在设备管理器或系统串口列表中确认 `Enhanced COM Port` 的端口号。
4. 使用 SSCOM、PuTTY、Tera Term、minicom 等串口工具打开该端口。
5. 设置串口参数为 `921600, 8N1`。
6. 复位开发板后，在串口终端输入 `help` 查看命令。

## RF 参数与 CLI

### 默认 RF 参数

| 参数 | 默认值 |
| --- | --- |
| Frequency | 868 MHz |
| TX Power | 14 dBm |
| Modulation | LoRa |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble | 8 symbols |
| Sync Word | 0x12, private network |
| Header Type | Explicit |
| CRC | Enabled |

### CLI 命令参考

连接 UART1 Enhanced COM Port，波特率设置为 921600，然后输入 `help` 查看完整命令列表。

#### 通用命令

| 命令 | 说明 |
| --- | --- |
| `help` | 显示命令列表和用法 |
| `clear` | 清空终端显示 |
| `list` | 列出命令 |
| `info` | 显示产品和固件信息 |

#### RF 命令

| 命令 | 用法 | 说明 |
| --- | --- | --- |
| `show` | `show` | 显示当前 RF 配置 |
| `modem` | `modem <lora\|gfsk\|flrc>` | 获取或设置 RF modem 类型 |
| `freq` | `freq <frequency_in_hz>` | 获取或设置 RF 频率 |
| `power` | `power <dBm>` | 获取或设置 RF 发射功率 |
| `syncword` | `syncword <hex_value>` | 获取或设置 RF sync word |
| `tx` | `tx <hex byte> [hex byte] ...` | 发送 RF 数据包 |
| `rx` | `rx` | 启动 RF 接收 |
| `standby` | `standby` | 进入 standby 模式 |
| `sleep` | `sleep <0\|1>` | 进入 sleep 模式，`1` 表示唤醒时保留配置 |
| `wakeup` | `wakeup` | 从 sleep 模式唤醒 |
| `reset` | `reset` | 复位射频芯片 |
| `per` | `per <0\|1>` | 开启或关闭 PER 测试统计 |

#### LoRa 参数命令

| 命令 | 用法 | 说明 |
| --- | --- | --- |
| `lora_sf` | `lora_sf <5~12>` | 获取或设置 LoRa spreading factor |
| `lora_bw` | `lora_bw <31\|62\|125\|250\|500\|1000>` | 获取或设置 LoRa 带宽，单位 kHz |
| `lora_cr` | `lora_cr <4/5\|4/6\|4/7\|4/8\|li4/5\|li4/6\|li4/8>` | 获取或设置 LoRa coding rate |
| `lora_preamble` | `lora_preamble <length>` | 获取或设置 LoRa preamble 长度 |
| `lora_syncword` | `lora_syncword <0x12\|0x34>` | 获取或设置 LoRa sync word |
| `lora_cad` | `lora_cad [rx_timeout_ms]` | 启动 LoRa CAD 检测，可选设置检测到活动后的 RX 超时时间 |

#### 测试模式命令

| 命令 | 说明 |
| --- | --- |
| `tx_cw` | 发射连续波 |
| `tx_preamble` | 发射无限 preamble |

### LoRa 点对点测试示例

准备两块 AM36 Pico 开发板，烧录相同固件并连接各自的 UART1 Enhanced COM Port。

接收端：

```text
freq 868000000
rx
```

发送端：

```text
freq 868000000
tx 01 02 03 04
```

## 项目结构

```text
.
|-- CMakeLists.txt
|-- README.md
|-- README_EN.md
|-- dependencies.lock
|-- components/
|   `-- microrl/
|-- docs/
|   |-- pic/
|   |-- L-LRMAM36-FANN4-PK02_SCH_V01.pdf
|   |-- L-LRMAM36-FANN4-PK02_layout_V01.pdf
|   |-- L-LRMAM36-FANN4_V01.step
|   |-- Lierda L-LRMAM36-FANN4 Hardware Design Manual_CN_Rev1.0.pdf
|   `-- Lierda L-LRMAM36-FANN4 Hardware Design Manual_EN_Rev1.0.pdf
`-- main/
    |-- CMakeLists.txt
    |-- idf_component.yml
    |-- Kconfig.projbuild
    |-- main.c
    |-- LiotRfCmd.c
    |-- LiotRfCmd.h
    |-- LiotUart.c
    `-- LiotUart.h
```

## 常见问题

### 编译时提示找不到组件

首次编译会从 [ESP Component Registry](https://components.espressif.com/) 自动下载 `lierda-iot/esp_lora_driver`。请确认网络可以访问组件仓库；如果处于代理网络环境，请先完成代理配置。

### USB 口连接后无法识别设备

- 确认使用支持数据传输的 USB Type-C 线缆。
- 尝试更换电脑端 USB 接口。
- Windows 10 以下系统可能需要安装相应 USB 驱动。

### 烧录失败

- 手动进入下载模式后重新烧录。
- 确认选择了正确的烧录端口。
- 关闭其他占用该端口的程序。

### UART 无输出

- 确认 CP210x 驱动已安装。
- 确认连接的是开发板 `UART` Type-C 口，而不是 `USB` Type-C 口。
- 确认选择的是 CP2105 `Enhanced COM Port`。
- 确认串口参数为 `921600, 8N1`。
- 确认固件已正确烧录，开发板已复位并运行。

## 安全须知

- 使用无线通信设备时，请遵守所在国家或地区的无线电法规。
- 请勿在可能产生安全风险的环境中操作无线设备，例如飞机、医院限制区域、加油站、化工厂或爆破作业区域。
- 使用 LoRa 射频测试功能时，请根据当地法规选择合法频段和发射功率。

## 更多资源

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/)
- [ESP Component Registry](https://components.espressif.com/)
- [esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)

## 法律声明

本文档和相关资料由 Lierda Science & Technology Group Co., Ltd. 提供。Lierda 保留在不预先通知的情况下修改和改进产品、文档及相关资料的权利。使用本项目和开发板时，请遵守适用的法律法规、产品手册和安全要求。
