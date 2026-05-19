# ESP LR2021 LoRa Example

A minimal ESP-IDF example demonstrating how to drive the LR2021 radio module for LoRa point-to-point communication. Provides an interactive CLI over UART for configuring and testing RF parameters.

## Hardware Requirements

- ESP32-S3 based board with LR2021 radio module (e.g., LRMAM36Pico)
- USB-to-UART adapter connected to UART1 (TX: GPIO47, RX: GPIO48)

## Software Requirements

- ESP-IDF v5.0 or later
- [lierda-iot/esp_lora_driver](https://components.espressif.com/components/lierda-iot/esp_lora_driver) (auto-fetched by IDF Component Manager)

## Build and Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## UART Configuration

| Parameter | Value    |
|-----------|----------|
| Port      | UART1    |
| TX Pin    | GPIO 47  |
| RX Pin    | GPIO 48  |
| Baud Rate | 921600   |
| Data Bits | 8        |
| Parity    | None     |
| Stop Bits | 1        |

## Default RF Parameters

| Parameter  | Value       |
|------------|-------------|
| Frequency  | 868 MHz     |
| TX Power   | 14 dBm      |
| Modulation | LoRa        |
| SF         | 7           |
| BW         | 125 kHz     |
| CR         | 4/5         |
| Preamble   | 8 symbols   |
| Sync Word  | 0x12 (Private) |

## CLI Command Reference

Connect to the UART port at 921600 baud. Type `help` to see all commands.

### General Commands

| Command   | Description                          |
|-----------|--------------------------------------|
| `help`    | Show command list and usage           |
| `clear`   | Clear terminal screen                 |
| `list`    | List all commands                     |
| `info`    | Show product and firmware information |

### RF Commands

| Command    | Usage                          | Description                     |
|------------|--------------------------------|---------------------------------|
| `show`     | `show`                         | Show current RF configuration   |
| `modem`    | `modem <lora\|gfsk\|flrc>`    | Get/Set RF modem type           |
| `freq`     | `freq <frequency_in_hz>`       | Get/Set RF frequency            |
| `power`    | `power <dBm>`                  | Get/Set RF TX power             |
| `syncword` | `syncword <hex_value>`         | Get/Set RF sync word            |
| `tx`       | `tx <hex byte> [hex byte] ...` | Send RF packet                  |
| `rx`       | `rx`                           | Start RF receive (continuous)   |
| `standby`  | `standby`                      | Put RF in standby mode          |
| `sleep`    | `sleep <0\|1>`                 | Put RF in sleep mode            |
| `wakeup`   | `wakeup`                       | Wake up RF from sleep           |
| `reset`    | `reset`                        | Reset radio                     |
| `per`      | `per <0\|1>`                   | Enable/disable PER measurement  |
| `xosc`     | `xosc <xta> <xtb> <wait_us>`  | Configure LR20xx XOSC           |

### LoRa Parameter Commands

| Command         | Usage                                       | Description                |
|-----------------|---------------------------------------------|----------------------------|
| `lora_sf`       | `lora_sf <5~12>`                            | Get/Set spreading factor   |
| `lora_bw`       | `lora_bw <31\|62\|125\|250\|500\|1000>`     | Get/Set bandwidth (kHz)    |
| `lora_cr`       | `lora_cr <4/5\|4/6\|4/7\|4/8\|li4/5\|...>` | Get/Set coding rate        |
| `lora_preamble` | `lora_preamble <length>`                    | Get/Set preamble length    |
| `lora_syncword` | `lora_syncword <0x12\|0x34>`                | Get/Set LoRa sync word     |
| `lora_cad`      | `lora_cad <timeout_ms> [rx_timeout_ms]`     | Start LoRa CAD detection   |

### Test Mode Commands

| Command       | Description                  |
|---------------|------------------------------|
| `tx_cw`       | Transmit continuous wave     |
| `tx_preamble` | Transmit infinite preamble   |

## Quick Start

> **New to this board?** See the full [Getting Started Guide](docs/GETTING_STARTED.md) for step-by-step setup instructions.

1. Flash the firmware and open a serial terminal at 921600 baud.
2. Set frequency: `freq 868000000`
3. Send a packet: `tx 01 02 03 04`
4. On another device, start receiving: `rx`

## Project Structure

```
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   ├── main.c              # Application entry point
│   ├── LiotRfCmd.c/.h      # RF CLI command handler
│   └── LiotUart.c/.h       # UART driver
├── components/
│   └── microrl/            # Command line library
└── README.md
```
