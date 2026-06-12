# Lierda AM36 开发套件 L-LRMAM36-FANN4-PK02 使用说明书

> 版本：Rev1.1  
> 日期：2026-05-27  
> 状态：Release

## 法律声明

若接收利尔达科技集团股份有限公司(以下称为“利尔达”)的此份文档，即表示您已经同意以下条款。若不同意以下条款，请停止使用本文档。

本文档版权归利尔达科技集团股份有限公司所有，保留任何未在本文档中明示授予的权利。文档中涉及利尔达的专有信息。未经利尔达事先书面许可，任何单位和个人不得复制、传递、分发、使用和泄漏该文档以及该文档包含的任何图片、表格、数据及其他信息。

本产品符合有关环境保护和人身安全方面的设计要求，产品的存放、使用和弃置应遵照产品手册、相关合同或者相关法律、法规的要求进行。

本公司保留在不预先通知的情况下，对此手册中描述的产品进行修改和改进的权利；同时保留随时修订或收回本手册的权利。

## 安全须知

用户有责任遵循其他国家关于无线通信模组及设备的相关规定和具体的使用环境法规。通过遵循以下安全原则，可确保个人安全并有助于保护产品和工作环境免遭潜在损坏。我司不承担因客户未能遵循这些规定导致的相关损失。

- 道路行驶安全第一！当您开车时，请勿使用手持移动终端设备，除非其有免提功能。请停车，再打电话！
- 登机前请关闭移动终端设备。移动终端的无线功能在飞机上禁止开启用以防止对飞机通讯系统的干扰。忽略该提示项可能会导致飞行安全，甚至触犯法律。
- 当在医院或健康看护场所，注意是否有移动终端设备使用限制。RF干扰会导致医疗设备运行失常，因此可能需要关闭移动终端设备。
- 移动终端设备并不保障任何情况下都能进行有效连接，例如在移动终端设备没有话费或SIM无效。当您在紧急情况下遇见以上情况，请记住使用紧急呼叫，同时保证您的设备开机并且处于信号强度足够的区域。
- 您的移动终端设备在开机时会接收和发射射频信号，当靠近电视，收音机电脑或者其它电子设备时都会产生射频干扰。
- 请将移动终端设备远离易燃气体。当您靠近加油站，油库，化工厂或爆炸作业场所，请关闭移动终端设备。在任何有潜在爆炸危险场所操作电子设备都有安全隐患。

## 文件修订历史

| 文档版本 | 变更日期 | 修订人 | 审核人 | 变更内容 |
| --- | --- | --- | --- | --- |
| Rev1.0 | 26-05-15 | NXL | LXY | 初始版本 |
| Rev1.1 | 26-05-27 | GCJ | LXQ | 可读性优化 |

## 引言

本文档用于说明 AM36 开发套件 L-LRMAM36-FANN4-PK02 的产品组成、接口定义、开发环境搭建、固件编译烧录及串口调试方法，帮助用户完成开发板上电、示例工程验证和二次开发准备。

L-LRMAM36-FANN4-PK02 是基于利尔达 AM36 模组（L-LRMAM36-FANN4）的评估开发板。开发板出厂已贴装 AM36 模组，模组基于 ESP32-S3 与 LR2021 设计，支持 Wi-Fi、BLE、Generation 4 LoRa、FLRC、2-FSK/4-FSK 等无线能力，可用于 LoRa 射频性能评估及应用开发。

## Related Documents

The following files in `docs/` provide board-level and module-level reference material:

| File | Description |
| --- | --- |
| `L-LRMAM36-FANN4-PK02_SCH_V01.pdf` | Development board schematic. Use this file to understand the electrical connections and signal routing on the AM36 development board. |
| `L-LRMAM36-FANN4-PK02_layout_V01.pdf` | Development board layout file. Use this file to review the PCB placement, routing, and board-level implementation details. |
| `L-LRMAM36-FANN4_V01.step` | Module 3D mechanical model. Use this file for enclosure fitting, mechanical checking, and installation reference. |
| `Lierda L-LRMAM36-FANN4 Hardware Design Manual_EN_Rev1.0.pdf` | English module hardware design manual. Use this file for the module specification, pin definitions, electrical characteristics, and integration guidance. |
| `Lierda L-LRMAM36-FANN4 Hardware Design Manual_CN_Rev1.0.pdf` | Chinese module hardware design manual. Use this file when a Chinese-language hardware integration reference is preferred. |

开发板外观示意如图 1.1 所示：

![Figure](assets/lierda_am36_pico_user_guide_rev1_1/figure_01.png)
![Figure](assets/lierda_am36_pico_user_guide_rev1_1/figure_02.png)

*图 1.1 开发板示意图*

### 出厂状态与包装清单

开发套件出厂已贴装 AM36 模组（L-LRMAM36-FANN4），并随附开发验证所需的标配物料。用户无需自行焊接模组，可直接连接 USB 进行上电、固件烧录、串口调试和示例工程验证。

**表 1-1 开发套件包装清单**

| 序号 | 名称 | 数量 | 说明 |
| --- | --- | --- | --- |
| 1 | AM36 开发板（L-LRMAM36-FANN4-PK02） | 1 块 | 出厂已贴装 AM36 模组（L-LRMAM36-FANN4）。 |
| 2 | LoRa 天线 | 1 根 | 标配物料，用于 LoRa 射频通信及测试。 |
| 3 | USB Type-C 数据线 | 1 根 | 标配物料，支持数据传输，可用于 USB 烧录或 UART 串口调试。 |
| 1 | AM36 开发板（L-LRMAM36-FANN4-PK02） | 1 块 | 出厂已贴装 AM36 模组（L-LRMAM36-FANN4）。 |

### 主要特性

AM36 开发套件的主要硬件特性如下。模组相关参数来源于 L-LRMAM36-FANN4 硬件设计手册。

**表 1-2 开发套件主要特性**

| 项目 | 说明 |
| --- | --- |
| 开发板型号 | L-LRMAM36-FANN4-PK02 |
| 搭载模组 | L-LRMAM36-FANN4，出厂已贴装。 |
| 主控与射频芯片 | ESP32-S3 + LR2021。 |
| 无线能力 | Wi-Fi、BLE、Generation 4 LoRa、FLRC、2-FSK/4-FSK。 |
| 工作频段 | LoRa：863 MHz ~ 930 MHz；Wi-Fi/BLE：2400 MHz ~ 2500 MHz。 |
| 模组尺寸 | 20 mm × 20 mm × 2.5 mm。 |
| 内部存储 | 8 MB Flash + 8 MB PSRAM。 |
| USB 转串口 | 板载 CP2105 双通道 USB to UART 芯片。 |

## 组件描述

![Figure](assets/lierda_am36_pico_user_guide_rev1_1/figure_03.png)

*图 2.1 开发板接口示意图*

**表 2-1 开发板组件介绍**

| 主要组件 | 描述 |
| --- | --- |
| LoRa天线接口 | SMA接口用于连接标配 LoRa 天线 |
| WiFi板载天线 | 板载PCB天线 |
| USB | Type-C 接口，连接 AM36 模组 USB 接口，用于固件烧录、USB-JTAG/Serial 调试 |
| UART | Type-C 接口，板载 CP2105 双通道 USB 转 UART 芯片。<br>连接 AM36 模组 ESP32-S3 两路串口（UART0：GPIO43/GPIO44，UART1：GPIO47/GPIO48）。<br>连接 PC 后显示 Enhanced/Standard 两路 COM 端口。 |
| GPIO | AM36 模组 GPIO 引出接口，可根据软件配置复用。详细引脚功能请参考 L-LRMAM36-FANN4 硬件设计手册。 |
| RST | 复位按键，低电平有效 |
| BOOT | BOOT 按键，用于进入下载模式。 |
| KEY | 用户自定义按键，具体功能由用户应用定义。 |

## 开发套件使用说明

### 开发环境搭建

本项目基于 ESP-IDF v5.0 或更高版本 构建。

Lierda提供基础示例代码（[https://github.com/lierda-iot/esp32_lora_samples](https://github.com/lierda-iot/esp32_lora_samples)），驱动AM36模组实现基础的LoRa通信及参数配置，下面以该示例工程为例，说明代码获取、编译、固件烧录及串口交互流程。

#### 第一步：安装 ESP-IDF

根据操作系统选择对应的安装方式：

- Windows：推荐使用 [ESP-IDF Windows 离线安装包](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/windows-setup.html)，安装完成后会在桌面生成 ESP-IDF Command Prompt 快捷方式。

- macOS / Linux：参考 [ESP-IDF Linux/macOS 安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/linux-macos-setup.html)。

#### 第二步：验证安装

打开 ESP-IDF 命令行终端，执行以下命令确认环境配置成功：

```bash
idf.py --version
```

如果正确输出版本号（如 v5.x.x），说明环境已就绪。

### 编译

#### 第一步：获取示例代码

在 ESP-IDF 命令行终端中执行：

```bash
git clone https://github.com/lierda-iot/esp32_lora_samples.git
```

```bash
cd esp32_lora_samples
```

#### 第二步：设置目标芯片

```bash
idf.py set-target esp32s3
```

#### 第三步：编译项目

```bash
idf.py build
```

首次编译时，构建系统会自动从 [[ESP 组件仓库](https://components.espressif.com/)](https://components.espressif.com/) 下载 lierda-iot/[[esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)](https://components.espressif.com/components/lierda-iot/[esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)) 驱动组件，请确保网络可访问 [ESP 组件仓库](https://components.espressif.com/)。

编译成功后终端会显示：

```bash
Project build complete. To flash, run:
```

```bash
idf.py flash
```

### 烧录

#### 第一步：连接开发板

使用标配 USB Type-C 数据线将开发板 USB 口（非 UART Type-C 口）连接至电脑。

注意：请使用支持数据传输的 USB Type-C 线缆。若使用仅支持充电的线缆，PC 将无法识别设备。

连接成功后，系统会自动识别出 USB-JTAG/Serial 设备（Windows 10 及以上系统通常无需额外驱动）：

- Windows：在设备管理器 > 端口 中查看 USB-JTAG 对应的 COM 端口号（如 COM3）

- Linux：通常为 /dev/ttyACM0

- macOS：通常为 /dev/cu.usbmodem-xxxx

#### 第二步：烧录固件

```bash
idf.py flash
```

如果电脑连接了多个 USB 设备，需手动指定端口：

```bash
# Windows
```

```bash
idf.py -p COM3 flash
```

```bash
# Linux
```

```bash
idf.py -p /dev/ttyACM0 flash
```

```bash
# macOS
```

```bash
idf.py -p /dev/cu.usbmodem-xxxx flash
```

#### 无法烧录时的处理

如烧录失败，可按以下步骤手动进入下载模式后重新烧录：

1、按住 BOOT 按键不松

2、按一下 RESET 按键

3、松开 BOOT 按键

4、重新执行 idf.py flash

### 串口调试

安装CP210x USB转串口芯片驱动。使用标配 USB Type-C 数据线将开发板 UART Type-C 口连接至 PC。UART Type-C 口通过板载 CP2105 双通道 USB 转 UART 芯片引出 AM36 模组 ESP32-S3 的两路 UART，PC 端会枚举出 Enhanced COM Port 和 Standard COM Port 两个串口。示例工程默认使用 Enhanced COM Port 进行应用通信和调试。

**表 3-2 UART Type-C 串口识别说明**

| PC端显示名称 | 对应串口 | 模组引脚 | 调试参数 | 用途说明 |
| --- | --- | --- | --- | --- |
| Silicon Labs Dual CP2105 USB to UART Bridge: Enhanced COM Port | UART1 | GPIO47/GPIO48 | 921600，8N1（示例工程） | 示例工程默认使用的应用通信/调试串口。 |
| Silicon Labs Dual CP2105 USB to UART Bridge: Standard COM Port | UART0 | GPIO43/GPIO44 | 按用户程序配置 | 第二路串口，可根据用户应用进行配置。 |

#### 调试步骤

1. 将开发板 UART Type-C 口连接至 PC，在设备管理器中确认 Enhanced COM Port 和 Standard COM Port 对应的 COM 号。

2. 打开串口调试助手（如 SSCOM、PuTTY），选择 Enhanced COM Port 对应的 COM 号。

3. 设置串口参数：波特率 921600，数据位 8，停止位 1，无校验。

4. 打开串口后复位开发板，即可接收应用层调试输出。示例工程发送 help 后可返回命令帮助信息，详细指令请参考示例工程 README。

### 常见问题

#### 编译时提示找不到组件

确保首次编译时 PC 可以访问 [ESP 组件仓库](https://components.espressif.com/)。lierda-iot/[esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver) 组件会从 [ESP 组件仓库](https://components.espressif.com/)自动下载；如处于代理网络环境，请先完成代理配置。

#### USB 口连接后无法识别设备

- 确认使用支持数据传输的 USB Type-C 线缆。

- 尝试更换 PC 端 USB 接口。

- Windows 10 以下系统可能需要根据 PC 环境安装相应 USB 驱动。

#### 烧录失败

- 手动进入下载模式（参考上方 BOOT + RESET 操作）。

- 确认选择了正确的烧录端口。

- 关闭其他占用该端口的程序。

#### UART 无输出

- 确认CP210x驱动已安装，且UART Type-C 口连接 PC 后，设备管理器已识别出 CP2105 Enhanced COM Port 和 Standard COM Port。

- 示例工程默认选择 Enhanced COM Port，串口波特率设为 921600。

- 确认固件已正确烧录，开发板已复位并运行。

### 更多资源

- [ESP-IDF 编程指南（中文）](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/)
- [ESP 组件仓库](https://components.espressif.com/)
- [esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)
