# Lierda AM36 Development Kit L-LRMAM36-FANN4-PK02 User Guide

> Version: Rev1.1  
> Date: 2026-05-27  
> Status: Release

## Legal Notice

By receiving this document from Lierda Science & Technology Group Co., Ltd. ("Lierda"), you agree to the following terms. If you do not agree, stop using this document.

This document is copyrighted by Lierda. All rights not expressly granted are reserved. This document contains proprietary information of Lierda. Without prior written permission from Lierda, no organization or individual may copy, transmit, distribute, use, or disclose this document or any images, tables, data, or other information contained in it.

This product is designed to meet relevant environmental protection and personal safety requirements. Storage, use, and disposal of the product shall comply with the product manual, applicable contracts, and relevant laws and regulations.

Lierda reserves the right to modify and improve the products described in this manual without prior notice, and reserves the right to revise or withdraw this manual at any time.

## Safety Instructions

- Road safety comes first. Do not use handheld mobile terminal equipment while driving unless hands-free operation is available. Park before making a call.
- Turn off mobile terminal equipment before boarding an aircraft. Wireless functions are prohibited on aircraft to prevent interference with aircraft communication systems. Ignoring this notice may affect flight safety and may violate the law.
- In hospitals or healthcare facilities, observe restrictions on the use of mobile terminal equipment. RF interference may cause medical equipment to malfunction.
- Mobile terminal equipment does not guarantee a valid connection in all conditions, such as when service is unavailable or a SIM is invalid. In an emergency, remember to use emergency calling and keep the device powered on in an area with sufficient signal strength.
- Mobile terminal equipment receives and transmits RF signals when powered on. It may cause RF interference when near TVs, radios, computers, or other electronic devices.
- Keep mobile terminal equipment away from flammable gas. Turn off the equipment near gas stations, oil depots, chemical plants, or blasting areas. Operating electronic equipment in potentially explosive environments may be hazardous.

## Revision History

| Version | Date | Author | Reviewer | Changes |
| --- | --- | --- | --- | --- |
| Rev1.0 | 2026-05-15 | NXL | LXY | Initial version |
| Rev1.1 | 2026-05-27 | GCJ | LXQ | Readability optimization |

## Introduction

This document describes the product contents, interface definitions, development environment setup, firmware build and flashing process, and UART debugging method for the AM36 development kit L-LRMAM36-FANN4-PK02. It helps users power on the board, verify the sample project, and prepare for secondary development.

L-LRMAM36-FANN4-PK02 is an evaluation development board based on the Lierda AM36 module (L-LRMAM36-FANN4). The AM36 module is mounted before shipment. The module is designed with ESP32-S3 and LR2021, and supports Wi-Fi, BLE, Generation 4 LoRa, FLRC, and 2-FSK/4-FSK wireless capabilities. It can be used for LoRa RF performance evaluation and application development.

## Related Documents

The following files in `docs/` provide board-level and module-level reference material:

| File | Description |
| --- | --- |
| `L-LRMAM36-FANN4-PK02_SCH_V01.pdf` | Development board schematic. Use this file to understand the electrical connections and signal routing on the AM36 development board. |
| `L-LRMAM36-FANN4-PK02_layout_V01.pdf` | Development board layout file. Use this file to review the PCB placement, routing, and board-level implementation details. |
| `L-LRMAM36-FANN4_V01.step` | Module 3D mechanical model. Use this file for enclosure fitting, mechanical checking, and installation reference. |
| `Lierda L-LRMAM36-FANN4 Hardware Design Manual_EN_Rev1.0.pdf` | English module hardware design manual. Use this file for the module specification, pin definitions, electrical characteristics, and integration guidance. |
| `Lierda L-LRMAM36-FANN4 Hardware Design Manual_CN_Rev1.0.pdf` | Chinese module hardware design manual. Use this file when a Chinese-language hardware integration reference is preferred. |

The development board appearance is shown in Figure 1.1:

![Development board front](assets/lierda_am36_pico_user_guide_rev1_1/figure_01.png)

![Development board back](assets/lierda_am36_pico_user_guide_rev1_1/figure_02.png)

*Figure 1.1 Development board*

### Factory Configuration and Packing List

The development kit is shipped with the AM36 module (L-LRMAM36-FANN4) already mounted, and includes the standard accessories required for development verification. Users do not need to solder the module. The board can be connected by USB directly for power-on, firmware flashing, UART debugging, and sample project verification.

**Table 1-1 Packing list**

| No. | Item | Qty | Description |
| --- | --- | --- | --- |
| 1 | AM36 development board (L-LRMAM36-FANN4-PK02) | 1 pc | The AM36 module (L-LRMAM36-FANN4) is mounted before shipment. |
| 2 | LoRa antenna | 1 pc | Standard accessory for LoRa RF communication and testing. |
| 3 | USB Type-C data cable | 1 pc | Standard accessory with data transfer support, used for USB flashing or UART debugging. |
| 1 | AM36 development board (L-LRMAM36-FANN4-PK02) | 1 pc | The AM36 module (L-LRMAM36-FANN4) is mounted before shipment. |

### Key Features

The key hardware features of the AM36 development kit are listed below. Module-related parameters come from the L-LRMAM36-FANN4 Hardware Design Manual.

**Table 1-2 Key features**

| Item | Description |
| --- | --- |
| Development board model | L-LRMAM36-FANN4-PK02 |
| Mounted module | L-LRMAM36-FANN4, mounted before shipment. |
| MCU and RF chip | ESP32-S3 + LR2021. |
| Wireless capabilities | Wi-Fi, BLE, Generation 4 LoRa, FLRC, and 2-FSK/4-FSK. |
| Operating bands | LoRa: 863 MHz to 930 MHz; Wi-Fi/BLE: 2400 MHz to 2500 MHz. |
| Module dimensions | 20 mm x 20 mm x 2.5 mm. |
| Internal memory | 8 MB Flash + 8 MB PSRAM. |
| USB-to-UART | On-board CP2105 dual-channel USB-to-UART bridge. |

## Component Description

![Development board interfaces](assets/lierda_am36_pico_user_guide_rev1_1/figure_03.png)

*Figure 2.1 Development board interface diagram*

**Table 2-1 Development board components**

| Component | Description |
| --- | --- |
| LoRa antenna connector | SMA connector for the standard LoRa antenna. |
| On-board Wi-Fi antenna | On-board PCB antenna. |
| USB | Type-C connector connected to the AM36 module USB interface, used for firmware flashing and USB-JTAG/Serial debugging. |
| UART | Type-C connector with an on-board CP2105 dual-channel USB-to-UART bridge.<br>It connects to two ESP32-S3 UARTs on the AM36 module (UART0: GPIO43/GPIO44, UART1: GPIO47/GPIO48).<br>After connection to a PC, Enhanced and Standard COM ports are enumerated. |
| GPIO | AM36 module GPIO expansion interface. Functions can be multiplexed by software. For detailed pin functions, refer to the L-LRMAM36-FANN4 Hardware Design Manual. |
| RST | Reset button, active low. |
| BOOT | BOOT button for entering download mode. |
| KEY | User-defined button. The function is defined by the user application. |

## Using the Development Kit

### Setting Up the Development Environment

This project is built with ESP-IDF v5.0 or later.

Lierda provides a basic sample project ([https://github.com/lierda-iot/esp32_lora_samples](https://github.com/lierda-iot/esp32_lora_samples)) to drive the AM36 module for basic LoRa communication and parameter configuration. The following sections use this sample project to describe code acquisition, building, firmware flashing, and UART interaction.

#### Step 1: Install ESP-IDF

Select the installation method according to your operating system:

- Windows: It is recommended to use the ESP-IDF Windows offline installer. After installation, an ESP-IDF Command Prompt shortcut is created on the desktop.
- macOS / Linux: Refer to the [[ESP-IDF Linux/macOS installation guide](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/linux-macos-setup.html)](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/linux-macos-setup.html).

#### Step 2: Verify the Installation

Open the ESP-IDF command-line terminal and run:

```bash
idf.py --version
```

If a version number such as v5.x.x is printed, the environment is ready.

### Build

#### Step 1: Get the Sample Code

Run the following commands in the ESP-IDF command-line terminal:

```bash
git clone [https://github.com/lierda-iot/esp32_lora_samples](https://github.com/lierda-iot/esp32_lora_samples).git
```

```bash
cd esp32_lora_samples
```

#### Step 2: Set the Target Chip

```bash
idf.py set-target esp32s3
```

#### Step 3: Build the Project

```bash
idf.py build
```

During the first build, the build system automatically downloads the lierda-iot/[esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver) component from the [ESP Component Registry](https://components.espressif.com/). Make sure the network can access the [ESP Component Registry](https://components.espressif.com/).

After a successful build, the terminal prints:

```bash
Project build complete. To flash, run:
```

```bash
idf.py flash
```

### Flash

#### Step 1: Connect the Development Board

Use the standard USB Type-C data cable to connect the USB port of the development board (not the UART Type-C port) to the computer.

Note: Use a USB Type-C cable that supports data transfer. If a charge-only cable is used, the PC cannot recognize the device.

After connection, the system automatically recognizes a USB-JTAG/Serial device. Windows 10 and later usually do not require an additional driver:

- Windows: Check the COM port number corresponding to USB-JTAG under Device Manager > Ports, such as COM3.
- Linux: Usually /dev/ttyACM0.
- macOS: Usually /dev/cu.usbmodem-xxxx.

#### Step 2: Flash the Firmware

```bash
idf.py flash
```

If multiple USB devices are connected to the computer, specify the port manually:

```bash
# Windows
idf.py -p COM3 flash
```

```bash
# Linux
idf.py -p /dev/ttyACM0 flash
```

```bash
# macOS
idf.py -p /dev/cu.usbmodem-xxxx flash
```

#### If Flashing Fails

If flashing fails, manually enter download mode and flash again:

1. Press and hold the BOOT button.
2. Press the RESET button once.
3. Release the BOOT button.
4. Run `idf.py flash` again.

### UART Debugging

Install the CP210x USB-to-UART bridge driver. Use the standard USB Type-C data cable to connect the UART Type-C port of the development board to the PC. The UART Type-C port uses the on-board CP2105 dual-channel USB-to-UART bridge to expose two ESP32-S3 UARTs from the AM36 module. The PC enumerates an Enhanced COM Port and a Standard COM Port. The sample project uses the Enhanced COM Port by default for application communication and debugging.

**Table 3-2 UART Type-C port identification**

| PC-side device name | UART | Module pins | Debug settings | Purpose |
| --- | --- | --- | --- | --- |
| Silicon Labs Dual CP2105 USB to UART Bridge: Enhanced COM Port | UART1 | GPIO47/GPIO48 | 921600, 8N1 (sample project) | Default application communication/debug UART used by the sample project. |
| Silicon Labs Dual CP2105 USB to UART Bridge: Standard COM Port | UART0 | GPIO43/GPIO44 | Configured by the user application | Second UART, configurable by the user application. |

#### Debugging Steps

1. Connect the UART Type-C port of the development board to the PC, and confirm the COM numbers corresponding to the Enhanced COM Port and Standard COM Port in Device Manager.
2. Open a serial terminal tool such as SSCOM or PuTTY, and select the COM number corresponding to the Enhanced COM Port.
3. Set the serial parameters to 921600 baud, 8 data bits, 1 stop bit, and no parity.
4. Open the serial port and reset the development board to receive application debug output. In the sample project, sending `help` returns the command help information. For detailed commands, refer to the sample project README.

### FAQ

#### Component Not Found During Build

Make sure the PC can access the [ESP Component Registry](https://components.espressif.com/) during the first build. The lierda-iot/[esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver) component is downloaded automatically from the [ESP Component Registry](https://components.espressif.com/). If you are using a proxy network, configure the proxy first.

#### USB Port Is Not Recognized

- Confirm that the USB Type-C cable supports data transfer.
- Try another USB port on the PC.
- Systems earlier than Windows 10 may require an appropriate USB driver for the PC environment.

#### Flashing Fails

- Manually enter download mode by following the BOOT + RESET procedure above.
- Confirm that the correct flashing port is selected.
- Close any other program that is using the port.

#### No UART Output

- Confirm that the CP210x driver is installed and that the CP2105 Enhanced COM Port and Standard COM Port are recognized in Device Manager after connecting the UART Type-C port to the PC.
- The sample project uses the Enhanced COM Port by default, with the serial baud rate set to 921600.
- Confirm that the firmware has been flashed correctly and that the development board has been reset and is running.

### Additional Resources

- [ESP-IDF Programming Guide (Chinese)](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/)
- [ESP Component Registry](https://components.espressif.com/)
- [esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)
