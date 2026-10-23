# AGENTS.md - AI Assistant Guide

This file is written for AI assistants: chat assistants that were given a link to this repository, and coding agents running in a local clone. It tells you how to guide a user from a new development board to running the LoRa and FLRC tests of this example. Human readers should start with [README_EN.md](README_EN.md) or [README.md](README.md).

## First Reply (Required)

After you read this file, your first reply must tell the user clearly that you found `AGENTS.md`, the setup guide of this repository, and that you can walk them through a quick start. Put this notice at the start of the reply, in the user's language, once per conversation. For example:

> I found AGENTS.md, the setup guide of this repository. I can walk you through a quick start step by step: confirm your board, flash the firmware, and run the LoRa and FLRC tests. Shall we start with your board?

If the user asked a specific question, answer it after the notice and offer the guide instead of starting it. Otherwise, start the guide with section 2.

## 1. How to Guide the User

- Reply in the language the user writes in, even though this file is in English.
- Guide one step at a time. For each step, say what to do and what the user should see, then wait for the user's result before moving on. When a step fails, ask the user to paste the terminal or serial output.
- Ask instead of guessing: operating system, serial port name, number of boards, and the region where the boards are used.
- If you can run commands yourself (for example a coding agent in a clone), you may build without asking, but ask before flashing a board and before any command that transmits.
- Before any transmitting command (`tx`, `flrc_burst_tx`, `tx_cw`, `tx_preamble`), make sure a LoRa antenna is connected to the transmitting board and that the frequency and power are allowed in the user's region. Do not suggest a TX power above the 14 dBm default unless the user confirms it is legal for them. `flrc_burst_tx`, `tx_cw`, and `tx_preamble` transmit continuously.
- Use only commands that exist in this firmware. The full list is in the "CLI Command Reference" section of [README_EN.md](README_EN.md), and the board prints it with `help`. If this file and the source code disagree, trust the source code and tell the user.

Overall flow:

1. Confirm the hardware (section 2).
2. Choose prebuilt firmware or a build from source (section 3).
3. Flash the board (section 4 or 5, port names in section 6).
4. Check the CLI (section 7).
5. Run the tests the user wants (section 8).
6. Use the troubleshooting table when something does not work (section 9).

## 2. Confirm the Hardware

Ask the user to confirm each item:

1. **Board**: Lierda AM36 Pico development board L-LRMAM36-FANN4-PK02, with the AM36 module L-LRMAM36-FANN4 (ESP32-S3 + LR2021). Photos: [docs/pic/Front.png](docs/pic/Front.png), [docs/pic/Back.png](docs/pic/Back.png), and the connector layout [docs/pic/Interface.png](docs/pic/Interface.png). The firmware is made for this board. On other hardware the LR2021 pins (set in the `esp_lora_driver` component configuration, see section 10) may not match.
2. **Number of boards**:
   - 1 board: CLI checks and single-board transmit tests with measuring instruments (section 8, T5).
   - 2 boards: also LoRa point-to-point, FLRC throughput, and CAD tests (section 8, T1 to T3). Both boards run the same firmware.
3. **Antenna**: a LoRa antenna on the SMA connector of every board that transmits.
4. **Cable and connector**: a USB Type-C data cable (not charge-only) in the connector labeled `USB`. That connector is used for flashing and for the CLI. The connector labeled `UART` (CP2105 bridge) is not used by this example.
5. **Region and frequency**: the firmware starts at 869 MHz and 14 dBm. The LoRa operating band of the module is 863 MHz to 930 MHz. 869 MHz lies in the European 863 MHz to 870 MHz band. In regions that use 902 MHz to 928 MHz, a frequency such as 915 MHz is used instead. The user is responsible for local rules. Both boards must use the same frequency.

## 3. Choose a Path

Ask which path fits the user:

- **Path A, prebuilt firmware**: for users who only want to run the tests. Needs Python and esptool, no ESP-IDF. Go to section 4.
- **Path B, build from source**: for users who want to change code or configuration. Needs ESP-IDF v5.3 or later. Go to section 5.

## 4. Path A: Flash the Prebuilt Firmware

1. Download the firmware from the latest release: <https://github.com/lierda-iot/esp32_lora_samples/releases/latest>. Either the merged image `esp_lr2021_lora_example-vX.Y.Z-merged.bin` alone, or the three files `bootloader.bin`, `partition-table.bin`, and `esp_lr2021_lora_example.bin`. Take the exact file names from the release page.
2. Install esptool if needed: `python -m pip install esptool`. An ESP-IDF installation already includes it.
3. Find the serial port (section 6).
4. Flash the merged image, replacing `<PORT>` and the file name:

   ```
   python -m esptool --chip esp32s3 -p <PORT> -b 460800 write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m 0x0 esp_lr2021_lora_example-vX.Y.Z-merged.bin
   ```

   Or flash the three separate files:

   ```
   python -m esptool --chip esp32s3 -p <PORT> -b 460800 write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 esp_lr2021_lora_example.bin
   ```

5. If esptool cannot connect, enter download mode: press and hold `BOOT`, press the `RST` (RESET) button once, release `BOOT`, then flash again. After flashing in download mode entered this way, press `RST` once to start the firmware.
6. Continue with section 7.

## 5. Path B: Build from Source

1. **Check ESP-IDF**: open an ESP-IDF terminal (on Windows the "ESP-IDF PowerShell" or "ESP-IDF CMD" shortcut, on Linux and macOS a shell where `export.sh` of ESP-IDF has been sourced) and run:

   ```
   idf.py --version
   ```

   - The version must be v5.3 or later. Older versions cannot build this example: [main/CMakeLists.txt](main/CMakeLists.txt) requires the `esp_driver_spi`, `esp_driver_gpio`, and `esp_driver_usb_serial_jtag` components, which first appear in v5.3.
   - If `idf.py` is not found or the version is too old, point the user to the setup guides linked in the "Quick Start" section of [README_EN.md](README_EN.md).
2. **Get the code**:

   ```
   git clone https://github.com/lierda-iot/esp32_lora_samples.git
   cd esp32_lora_samples
   ```

3. **Set the target and build**:

   ```
   idf.py set-target esp32s3
   idf.py build
   ```

   - `set-target` regenerates `sdkconfig` from [sdkconfig.defaults](sdkconfig.defaults), which sets the CLI line ending, the prompt, the CPU clock, and the FreeRTOS tick rate. If the user skips `set-target` in a folder that already has an older `sdkconfig`, these settings are not applied.
   - The first build downloads `lierda-iot/esp_lora_driver` from the ESP Component Registry, so the PC needs access to <https://components.espressif.com/>.
   - Success ends with `Project build complete.`
4. **Flash**, after finding the port (section 6):

   ```
   idf.py -p <PORT> flash
   ```

   If flashing cannot connect, use the download-mode steps in section 4, step 5.
5. Continue with section 7. Users of Path B can also use `idf.py -p <PORT> monitor` as the serial terminal: it sends CR when Enter is pressed. Exit it with `Ctrl+]` before flashing again.

## 6. Find the Serial Port

The `USB` connector appears as the ESP32-S3 USB-Serial-JTAG device (USB vendor ID 303A):

- Windows: Device Manager, "Ports (COM & LPT)", entry "USB Serial Device (COMx)". Entries named "Silicon Labs Dual CP2105 ... Enhanced COM Port" or "Standard COM Port" belong to the `UART` connector and are the wrong ports.
- Linux: `/dev/ttyACM0` or a higher number. The user may need permission for the port (for example membership in the `dialout` group).
- macOS: `/dev/cu.usbmodem...`.

Resetting or flashing the board also resets its USB connection. If a serial terminal loses the port, close it and open it again.

## 7. Check the CLI

1. Open the port in a serial terminal such as PuTTY, Tera Term, minicom, SSCOM, or `idf.py monitor`. The baud rate setting does not matter. Enter must send CR (`\r`) or CR LF. Most terminals do this; in SSCOM and similar tools enable the option that appends a line ending.
2. Press Enter. The prompt is `LR2021 >`.
3. Run these commands and compare the output:
   - `info`: prints `Product  : ESP LR2021 LoRa Example`, the firmware version on the `FW Ver` line, and the build time.
   - `show`: prints `modem       : lora`, `sync word   : 0x12`, and `frequency   : 869000000 Hz` after a reset.
   - `help`: lists every command with its usage.
4. The firmware does not save settings. A board reset returns to LoRa, 869 MHz, 14 dBm, SF7, 125 kHz, CR 4/5.
5. [main/main.c](main/main.c) switches most ESP-IDF information and warning logs off, so log lines on the port are mainly errors starting with `E (`.

## 8. Tests

Ask which tests the user wants. For tests with two boards, call one board RX and the other TX. Both boards must use the same modem, frequency, and parameters. If the user is not at 869 MHz, add `freq <hz>` on both boards before each test.

### T1. LoRa Point-to-Point (2 boards)

RX board:

```
freq 869000000
rx
```

TX board:

```
freq 869000000
tx 01 02 03 04
```

- TX board prints `TX done!`.
- RX board prints `RX done, size: 4, data: 01 02 03 04` and keeps receiving.
- Repeat `tx` a few times, then swap the roles. `standby` stops receiving.
- Packet statistics: on the RX board, `per 1` resets the counters and starts receiving. Each received packet then prints `PER: packet num=... success num=... Signal RSSI=... SNR=... crc err=... header error=...`. `per 0` stops it.
- Lines such as `RX header error irq received` or `RX CRC error irq received` on the RX board can also come from other LoRa devices nearby on the same frequency. If they appear while the TX board is idle, try another legal frequency on both boards.

### T2. FLRC Burst Throughput (2 boards)

Start the RX board first.

RX board:

```
modem flrc
freq 869000000
flrc_burst_rx 1/1
```

TX board:

```
modem flrc
freq 869000000
flrc_burst_tx 1/1
```

- RX board prints `FLRC BURST RX started ...`, then every second `[FLRC BURST RX] <n> pkt/s, <x> Mbps, RSSI <r> dBm, CRC_ERR <c>`.
- TX board prints `FLRC BURST TX started ...`, then every second `[FLRC BURST TX] <n> pkt/s, <x> Mbps`.
- The link works when the RX packet rate is about the same as the TX packet rate and `CRC_ERR` stays at or near 0.
- Each packet carries 511 bytes. The coding rate argument (`1/1`, `3/4`, `2/3`, `1/2`) must be the same on both boards. `1/1` (no coding) gives the highest rate, close to the 2.2 Mbit/s mentioned in [README_EN.md](README_EN.md). A result a few percent lower is normal; it depends on the host, timing, and radio environment.
- Stop with `flrc_burst_stop` on the TX board and on the RX board. Each prints `FLRC BURST stopped`.
- Burst mode uses 2.6 Mbit/s, which occupies a wide channel, and it transmits continuously. Check local bandwidth and duty cycle rules first, or use a shielded environment.
- The burst settings stay in effect for later FLRC commands. Reset the boards to return to the default FLRC parameters.

### T3. LoRa CAD (2 boards)

- RX board, with nothing transmitting: `lora_cad 1000` prints `Starting LoRa CAD, ...`, then `CAD Done (No activity detected).`
- To see a detection, start `tx_preamble` on the TX board (continuous LoRa preamble, same frequency and LoRa settings), then run `lora_cad 1000` on the RX board. It should print `CAD Positive (Preamble Detected)!` and start receiving.
- Stop the TX board with `standby`.

### T4. Change Parameters

- LoRa: `lora_sf`, `lora_bw`, `lora_cr`, `lora_preamble`, `lora_syncword`.
- FLRC: `flrc_br`, `flrc_cr`, `flrc_preamble`, `flrc_crc`.
- Both modems: `freq`, `power`, `syncword`.
- A command without arguments prints the current value; `show` prints the configuration.
- Set the same values on both boards, then repeat T1 (LoRa) or send `tx` packets with `modem flrc` selected on both boards (FLRC).

### T5. Single-Board Transmit Tests (instruments)

- `tx_cw` transmits an unmodulated carrier and `tx_preamble` a continuous LoRa preamble, at the current frequency and power.
- Use them with a spectrum analyzer or power meter, through an antenna or a suitable attenuator, preferably in a shielded environment.
- Stop with `standby`.

## 9. Troubleshooting

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| `idf.py` is not found | Not an ESP-IDF terminal | Open the ESP-IDF terminal, or source `export.sh` / run `export.ps1` of ESP-IDF |
| Build fails to resolve `esp_driver_spi`, `esp_driver_gpio`, or `esp_driver_usb_serial_jtag` | ESP-IDF older than v5.3 | Install ESP-IDF v5.3 or later |
| Build cannot download `lierda-iot/esp_lora_driver` | No access to the ESP Component Registry | Check the network or proxy, then build again |
| No new port appears | Charge-only cable, or cable in the `UART` connector | Use a data cable in the `USB` connector, try another PC USB port |
| esptool or `idf.py flash` cannot connect | Port busy, or the chip is not in download mode | Close other programs on the port, use the download-mode steps in section 4 |
| Typed characters are echoed but commands do not run | The terminal sends no line ending | Make Enter send CR or CR LF |
| Prompt shows `033[32mIRin >033[0m` instead of `LR2021 >` | Firmware built from an older `sdkconfig` without the settings of `sdkconfig.defaults` | Run `idf.py set-target esp32s3`, build, and flash again |
| No output at all | Wrong port (CP2105), port held by another program, or the port was lost after a reset | Select the USB-Serial-JTAG port, close other programs, reopen the port, press Enter |
| `Unknown command: ...` | Typing error or a command that this firmware does not have | Run `help` and use the listed names |
| `TX done!` but no `RX done` | RX board not receiving, different frequency, modem, or parameters, or no antenna | Run `rx` on the RX board, compare `show` on both boards, check antennas |
| FLRC RX stays at 0 pkt/s | RX started after TX, different coding rate or frequency | Stop both boards, start RX first, use the same coding rate and frequency |

## 10. Repository Map

- [main/main.c](main/main.c): `app_main()`; sets log levels, starts the radio and the CLI on USB-Serial-JTAG.
- [main/LiotRfCmd.c](main/LiotRfCmd.c): the CLI command table (`cmd_table`), the RF commands, FLRC burst streaming, and the radio IRQ task. A mutex keeps CLI commands and radio IRQ handling from accessing the radio at the same time.
- [main/LiotUsbJtag.c](main/LiotUsbJtag.c): CLI input and output over USB-Serial-JTAG.
- [components/microrl](components/microrl): the line editor of the CLI (history, Tab completion, line ending).
- [sdkconfig.defaults](sdkconfig.defaults): CLI line ending (CR), prompt, CPU 240 MHz, FreeRTOS tick 1000 Hz.
- [main/idf_component.yml](main/idf_component.yml): dependency on `lierda-iot/esp_lora_driver` and the minimum ESP-IDF version.
- LR2021 SPI and GPIO pins: options `CONFIG_LR2021_*` of the `esp_lora_driver` component, shown by `idf.py menuconfig`. Their defaults match the AM36 Pico board.
- [docs/](docs): board schematic and layout, module hardware design manuals, and the module 3D model.
