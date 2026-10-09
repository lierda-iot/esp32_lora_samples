# Lierda AM36 Pico LoRa Example

Language: [中文](README.md) | English

This is an ESP-IDF based LR2021 LoRa example for the Lierda AM36 Pico development board L-LRMAM36-FANN4-PK02. It demonstrates LoRa point-to-point communication, FLRC packet streaming, RF parameter configuration, and an interactive command line (CLI) over the board `USB` port.

L-LRMAM36-FANN4-PK02 is an evaluation development board based on the Lierda AM36 module L-LRMAM36-FANN4. The AM36 module is mounted before shipment. The module is designed with ESP32-S3 and LR2021, and supports Wi-Fi, BLE, Generation 4 LoRa, FLRC, and 2-FSK/4-FSK wireless capabilities. It can be used for LoRa RF performance evaluation and application development.

**High-speed FLRC**: the [esp_lora_driver](https://github.com/lierda-iot/esp32_lora_driver) component used by this example lets `LR2021` send and receive FLRC at a raw bit rate of 2.6 Mbit/s (`RAL_FLRC_RAW_BIT_RATE_2_600_MBPS`). Without coding (`RAL_FLRC_CR_1_1`), the measured application payload rate is close to 2.2 Mbit/s, not counting protocol overhead such as retransmissions and gaps between packets. The `flrc_burst_tx` / `flrc_burst_rx` commands of this example stream FLRC packets back to back and print the throughput every second.

## Table of Contents

- [Board Overview](#board-overview)
- [Interface Overview](#interface-overview)
- [Hardware Documents](#hardware-documents)
- [Development Environment](#development-environment)
- [Quick Start](#quick-start)
- [Flashing](#flashing)
- [CLI Debugging](#cli-debugging)
- [RF Parameters and CLI](#rf-parameters-and-cli)
- [Project Structure](#project-structure)
- [FAQ](#faq)
- [Safety Notes](#safety-notes)
- [Additional Resources](#additional-resources)

## Board Overview

| Front | Back |
| --- | --- |
| <img src="docs/pic/Front.png" alt="Development board front" height="420"> | <img src="docs/pic/Back.png" alt="Development board back" height="420"> |

| Item | Description |
| --- | --- |
| Recommended board | Lierda AM36 Pico L-LRMAM36-FANN4-PK02 |
| Mounted module | L-LRMAM36-FANN4, mounted before shipment |
| MCU and RF chip | ESP32-S3 + LR2021 |
| Wireless capabilities | Wi-Fi, BLE, Generation 4 LoRa, FLRC, and 2-FSK/4-FSK |
| Operating bands | LoRa: 863 MHz to 930 MHz; Wi-Fi/BLE: 2400 MHz to 2500 MHz |
| Module dimensions | 20 mm x 20 mm x 2.5 mm |
| Internal memory | 8 MB Flash + 8 MB PSRAM |
| USB-to-UART | On-board CP2105 dual-channel USB-to-UART bridge |

The development kit usually includes:

| No. | Item | Qty | Description |
| --- | --- | --- | --- |
| 1 | AM36 Pico development board L-LRMAM36-FANN4-PK02 | 1 pc | The AM36 module L-LRMAM36-FANN4 is mounted before shipment. |
| 2 | LoRa antenna | 1 pc | Used for LoRa RF communication and testing. |
| 3 | USB Type-C data cable | 1 pc | Used for USB flashing and CLI debugging. Use a cable that supports data transfer. |

## Interface Overview

<img src="docs/pic/Interface.png" alt="Development board interfaces" width="760">

The board provides two Type-C connectors. The `USB` port is connected to the AM36 module USB interface and is used for firmware flashing, USB-JTAG/Serial debugging, and the CLI of this example. The `UART` port uses the on-board CP2105 dual-channel USB-to-UART bridge to expose two UART channels for user applications; the CLI of this example does not use it.

| Interface / Component | Description |
| --- | --- |
| LoRa antenna connector | SMA connector for the standard LoRa antenna. |
| On-board Wi-Fi antenna | On-board PCB antenna. |
| USB | Type-C connector connected to the AM36 module USB interface, used for firmware flashing, USB-JTAG/Serial debugging, and the CLI of this example. |
| UART | Type-C connector with an on-board CP2105 dual-channel USB-to-UART bridge; after connection to a PC, Enhanced and Standard COM ports are enumerated. Reserved for user applications. |
| GPIO | AM36 module GPIO expansion interface. Functions can be multiplexed by software. For detailed pin functions, refer to the module hardware design manual. |
| RST | Reset button, active low. |
| BOOT | Download-mode button used with RESET to enter firmware download mode. |
| KEY | User-defined button. The function is defined by the user application. |

## Hardware Documents

The `docs/` directory provides board-level and module-level reference material:

| File | Description |
| --- | --- |
| [L-LRMAM36-FANN4-PK02_SCH_V01.pdf](docs/L-LRMAM36-FANN4-PK02_SCH_V01.pdf) | Development board schematic. Use this file to understand electrical connections and signal routing on the AM36 Pico development board. |
| [L-LRMAM36-FANN4-PK02_layout_V01.pdf](docs/L-LRMAM36-FANN4-PK02_layout_V01.pdf) | Development board layout file. Use this file to review PCB placement, routing, and board-level implementation details. |
| [L-LRMAM36-FANN4_V01.step](docs/L-LRMAM36-FANN4_V01.step) | AM36 module 3D mechanical model for enclosure fitting, mechanical checking, and installation reference. |
| [Lierda L-LRMAM36-FANN4 Hardware Design Manual_EN_Rev1.0.pdf](docs/Lierda%20L-LRMAM36-FANN4%20Hardware%20Design%20Manual_EN_Rev1.0.pdf) | English module hardware design manual for module specifications, pin definitions, electrical characteristics, and integration guidance. |
| [Lierda L-LRMAM36-FANN4 Hardware Design Manual_CN_Rev1.0.pdf](docs/Lierda%20L-LRMAM36-FANN4%20Hardware%20Design%20Manual_CN_Rev1.0.pdf) | Chinese module hardware design manual for Chinese-language hardware integration reference. |

## Development Environment

- ESP-IDF v5.0 or later (including v6.0)
- [lierda-iot/esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver) 1.0.0 or a later 1.x version (`^1.0.0` in [main/idf_component.yml](main/idf_component.yml))

`esp_lora_driver` is fetched automatically by ESP-IDF Component Manager according to [main/idf_component.yml](main/idf_component.yml). During the first build, make sure the network can access the [ESP Component Registry](https://components.espressif.com/).

## Quick Start

### 1. Install ESP-IDF

- Windows: Use the [ESP-IDF Windows setup guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html).
- macOS / Linux: Use the [ESP-IDF Linux/macOS setup guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html).

After installation, open an ESP-IDF command-line terminal and run:

```bash
idf.py --version
```

If v5.x.x or later is printed, the environment is ready.

### 2. Get the Code

```bash
git clone https://github.com/lierda-iot/esp32_lora_samples.git
cd esp32_lora_samples
```

### 3. Build the Project

```bash
idf.py set-target esp32s3
idf.py build
```

After a successful build, the terminal prints:

```bash
Project build complete. To flash, run:
idf.py flash
```

## Flashing

Connect the development board `USB` Type-C port to the computer. This port is connected to the AM36 module USB interface and is used for firmware flashing, USB-JTAG/Serial debugging, and the CLI of this example.

Use a USB Type-C cable that supports data transfer. If a charge-only cable is used, the PC cannot recognize the device.

```bash
idf.py flash
```

If multiple USB devices are connected to the computer, specify the port manually:

```bash
# Windows
idf.py -p COM3 flash

# Linux
idf.py -p /dev/ttyACM0 flash

# macOS
idf.py -p /dev/cu.usbmodem-xxxx flash
```

If flashing fails, manually enter download mode and flash again:

1. Press and hold the `BOOT` button.
2. Press the `RESET` button once.
3. Release the `BOOT` button.
4. Run `idf.py flash` again.

## CLI Debugging

The interactive CLI of this example runs on the ESP32-S3 built-in USB-Serial-JTAG interface, that is, the same `USB` Type-C port used for flashing. No extra USB-to-UART driver is needed.

| Item | Description |
| --- | --- |
| Port | Development board `USB` Type-C port |
| PC-side device name | Windows: `USB Serial Device (COMx)`; Linux: `/dev/ttyACMx`; macOS: `/dev/cu.usbmodem-xxxx` |
| Serial settings | USB-Serial-JTAG ignores the baud rate; any value set in the serial terminal works |

Debugging steps:

1. Connect the development board `USB` Type-C port to the computer.
2. Confirm the port name in Device Manager or the system serial port list.
3. Open the port with a serial terminal such as SSCOM, PuTTY, Tera Term, or minicom.
4. Press Enter, then enter `help` in the serial terminal.

Notes:

- The CLI runs a command when it receives CR (`\r`), which is what the Enter key sends in most serial terminals. In tools that send typed text without a line ending by default, such as SSCOM, enable the option that appends CR or CRLF.
- Resetting the development board also resets its USB connection. If the serial terminal does not reconnect automatically, close and reopen the port.
- ESP-IDF boot messages may also appear on this port after a reset. After startup, the example only lets ESP-IDF error logs through so that they do not mix with the CLI output.
- The `UART` Type-C port (CP2105 Enhanced COM Port: UART1, GPIO47/GPIO48; Standard COM Port: UART0, GPIO43/GPIO44) is not used by the CLI and is left for user applications.

## RF Parameters and CLI

### Default RF Parameters

The example starts in LoRa mode with the following parameters:

| Parameter | Default Value |
| --- | --- |
| Frequency | 869 MHz |
| TX Power | 14 dBm |
| Modulation | LoRa |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble | 8 symbols |
| Sync Word | 0x12, private network |
| Header Type | Explicit |
| CRC | Enabled |

After `modem flrc`, the FLRC parameters below are used. `freq` and `power` apply to the currently selected modem, and FLRC also defaults to 869 MHz and 14 dBm.

| Parameter | Default Value |
| --- | --- |
| Bit Rate | 2.6 Mbit/s |
| Coding Rate | 1/1, no coding |
| Pulse Shape | BT 0.5 |
| Preamble | 16 bits |
| Sync Word | 4 bytes, 0x90563412 |
| Payload Length | Variable |
| CRC | 2 bytes |

### CLI Command Reference

Open the `USB` port in a serial terminal as described in [CLI Debugging](#cli-debugging). Type `help` to see all commands.

#### General Commands

| Command | Description |
| --- | --- |
| `help` | Show command list and usage |
| `clear` | Clear terminal screen |
| `list` | List commands |
| `info` | Show product and firmware information |

#### RF Commands

| Command | Usage | Description |
| --- | --- | --- |
| `show` | `show` | Show current RF configuration |
| `modem` | `modem <lora\|gfsk\|flrc>` | Get or set RF modem type |
| `freq` | `freq <frequency_in_hz>` | Get or set RF frequency |
| `power` | `power <dBm>` | Get or set RF TX power |
| `syncword` | `syncword <hex_value>` | Get or set RF sync word |
| `tx` | `tx <hex byte> [hex byte] ...` | Send RF packet |
| `rx` | `rx` | Start RF receive |
| `standby` | `standby` | Put RF in standby mode |
| `sleep` | `sleep <0\|1>` | Put RF in sleep mode. `1` keeps configuration on wakeup. |
| `wakeup` | `wakeup` | Wake up RF from sleep mode |
| `reset` | `reset` | Reset the radio |
| `per` | `per <0\|1>` | Enable or disable PER statistics |
| `xosc` | `xosc <xta> <xtb> <wait_time_us>` | Get or set the LR2021 crystal oscillator configuration: XTA/XTB trim values and wait time |

#### LoRa Parameter Commands

| Command | Usage | Description |
| --- | --- | --- |
| `lora_sf` | `lora_sf <5~12>` | Get or set LoRa spreading factor |
| `lora_bw` | `lora_bw <31\|62\|125\|250\|500\|1000>` | Get or set LoRa bandwidth in kHz |
| `lora_cr` | `lora_cr <4/5\|4/6\|4/7\|4/8\|li4/5\|li4/6\|li4/8>` | Get or set LoRa coding rate |
| `lora_preamble` | `lora_preamble <length>` | Get or set LoRa preamble length |
| `lora_syncword` | `lora_syncword <0x12\|0x34>` | Get or set LoRa sync word |
| `lora_cad` | `lora_cad <cad_timeout_ms> [rx_timeout_ms]` | Start LoRa CAD detection. When activity is detected, RX starts with `rx_timeout_ms`, which defaults to `cad_timeout_ms`. |

#### FLRC Parameter Commands

| Command | Usage | Description |
| --- | --- | --- |
| `flrc_br` | `flrc_br <260\|325\|520\|650\|1040\|1300\|2080\|2600>` | Get or set FLRC bit rate in kbit/s |
| `flrc_cr` | `flrc_cr <1/2\|3/4\|1/1\|2/3>` | Get or set FLRC coding rate |
| `flrc_preamble` | `flrc_preamble <4\|8\|12\|16\|20\|24\|28\|32>` | Get or set FLRC preamble length in bits |
| `flrc_crc` | `flrc_crc <off\|2\|3\|4>` | Get or set FLRC CRC length in bytes |

#### FLRC Burst Streaming Commands

| Command | Usage | Description |
| --- | --- | --- |
| `flrc_burst_tx` | `flrc_burst_tx [1/2\|3/4\|1/1\|2/3]` | Start back-to-back transmission of 511-byte FLRC packets, optionally setting the coding rate. Prints packets per second and Mbit/s every second. |
| `flrc_burst_rx` | `flrc_burst_rx [1/2\|3/4\|1/1\|2/3]` | Start continuous FLRC reception, optionally setting the coding rate. Prints packets per second, Mbit/s, RSSI, and CRC errors every second. |
| `flrc_burst_cr` | `flrc_burst_cr <1/2\|3/4\|1/1\|2/3>` | Get or set the burst coding rate, 3/4 by default. Takes effect on the next `flrc_burst_tx` or `flrc_burst_rx`. |
| `flrc_burst_stop` | `flrc_burst_stop` | Stop burst TX or RX |

Burst streaming uses its own FLRC settings: 2.6 Mbit/s, 32-bit preamble, 2-byte CRC. The transmitter and the receiver must use the same coding rate. These settings stay in effect for later FLRC commands; reset the development board to return to the default FLRC parameters.

#### Test Mode Commands

| Command | Description |
| --- | --- |
| `tx_cw` | Transmit continuous wave |
| `tx_preamble` | Transmit infinite preamble |

### LoRa Point-to-Point Test Example

Prepare two AM36 Pico development boards, flash the same firmware, and open the CLI of each board on its `USB` port.

Receiver:

```text
freq 869000000
rx
```

Transmitter:

```text
freq 869000000
tx 01 02 03 04
```

### FLRC Burst Streaming Example

Receiver:

```text
modem flrc
freq 869000000
flrc_burst_rx 1/1
```

Transmitter:

```text
modem flrc
freq 869000000
flrc_burst_tx 1/1
```

Enter `flrc_burst_stop` on either board to stop.

## Project Structure

```text
.
|-- CMakeLists.txt
|-- README.md
|-- README_EN.md
|-- sdkconfig.defaults
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
    |-- LiotUsbJtag.c
    `-- LiotUsbJtag.h
```

## FAQ

### Component Not Found During Build

During the first build, `lierda-iot/esp_lora_driver` is downloaded automatically from the [ESP Component Registry](https://components.espressif.com/). Make sure the PC can access the registry. If you are using a proxy network, configure the proxy first.

### USB Port Is Not Recognized

- Confirm that the USB Type-C cable supports data transfer.
- Try another USB port on the PC.
- Systems earlier than Windows 10 may require an appropriate USB driver.

### Flashing Fails

- Manually enter download mode and flash again.
- Confirm that the correct flashing port is selected.
- Close any other program that is using the port.

### No CLI Output

- Confirm that the development board `USB` Type-C port is connected, not the `UART` Type-C port.
- Confirm that the board's USB-Serial-JTAG port is selected, not a CP2105 COM port.
- Close any other program that is using the port, such as `idf.py monitor`.
- After a reset, close and reopen the port if the serial terminal does not reconnect automatically.
- Confirm that the firmware has been flashed correctly and that the development board is running, then press Enter.
- If typed characters are echoed but commands do not run, make sure the serial tool sends a line ending (CR or CRLF). See [CLI Debugging](#cli-debugging).

## Safety Notes

- Follow the radio regulations of the country or region where the wireless device is used.
- Do not operate wireless devices in environments where they may cause safety risks, such as aircraft, restricted hospital areas, gas stations, chemical plants, or blasting areas.
- When using LoRa RF test functions, select legal frequency bands and TX power according to local regulations.
- `tx_cw`, `tx_preamble`, and `flrc_burst_tx` transmit continuously. Check the local duty cycle and bandwidth limits before using them, or use them in a shielded test environment.

## Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/)
- [ESP Component Registry](https://components.espressif.com/)
- [esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver)

## Legal Notice

This document and related materials are provided by Lierda Science & Technology Group Co., Ltd. Lierda reserves the right to modify and improve the products, documents, and related materials without prior notice. When using this project and development board, comply with applicable laws and regulations, product manuals, and safety requirements.
