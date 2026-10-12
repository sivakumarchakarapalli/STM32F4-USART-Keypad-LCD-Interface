# STM32F4 USART, Keypad and LCD Interface

## Overview
An Embedded C project using the STM32F439 microcontroller to interface USART2 with a 4×4 matrix keypad and a 16×2 LCD. The project demonstrates peripheral interfacing, GPIO configuration, serial communication, keypad scanning, and debouncing.

## Features
- USART2 transmission and reception
- 4×4 matrix keypad interfacing
- 16×2 LCD interfacing in 8-bit mode
- GPIO configuration using STM32 peripheral registers
- Polling-based UART communication
- Keypad scanning and debouncing
- Non-blocking USART reception
- LCD cursor positioning and display management

## Hardware Requirements
- STM32F439 Nucleo development board
- 4×4 matrix keypad
- 16×2 character LCD
- USB connection or suitable UART interface
- Connecting wires and power supply

## Software Requirements
- Embedded C
- Keil MDK or a compatible ARM development environment
- STM32F4 device headers
- PuTTY or Tera Term for serial communication

## System Architecture

        4×4 Keypad
             |
             v
      STM32F439 MCU
             |
       +-----+-----+
       |           |
       v           v
    USART2      16×2 LCD
       |
       v
    Computer

KEYPAD PIN MAPPING
| Keypad Signal | STM32 Pin |
| ------------- | --------- |
| Row 1         | PA4       |
| Row 2         | PA5       |
| Row 3         | PA6       |
| Row 4         | PA7       |
| Column 1      | PA0       |
| Column 2      | PA1       |
| Column 3      | PA2       |
| Column 4      | PA3       |

LCD PIN MAPPING
| LCD Signal | STM32 Pin |
| ---------- | --------- |
| RS         | PE2       |
| RW         | PE4       |
| Enable     | PE5       |
| DB0        | PE6       |
| DB1        | PE3       |
| DB2        | PF8       |
| DB3        | PF7       |
| DB4        | PF9       |
| DB5        | PG1       |
| DB6        | PG0       |
| DB7        | PD1       |

USART2 CONFIGURATION 
| Parameter     | Value       |
| ------------- | ----------- |
| Peripheral    | USART2      |
| TX Pin        | PD5         |
| RX Pin        | PD6         |
| Baud Rate     | 9600        |
| Data Bits     | 8           |
| Parity        | None        |
| Stop Bits     | 1           |
| Communication | Full duplex |

## Working Principle

1. Initialize GPIO ports and the LCD.
2. Configure USART2 for transmission and reception.
3. Scan the 4×4 keypad to detect key presses.
4. Display the pressed key on the LCD.
5. Transmit the pressed character through USART2.
6. Receive characters from the computer through USART2.
7. Display received characters on the LCD.

## Keypad Scanning

- Configure keypad rows as GPIO outputs.
- Configure keypad columns as GPIO inputs with pull-up resistors.
- Scan each row and detect pressed keys by reading column inputs.
- Apply debounce logic to reduce false or repeated key detection.

## UART Communication

- Configure USART2 with a baud rate of 9600.
- Use polling to transmit keypad characters.
- Check the receive status flag to detect incoming characters.
- Display received characters on the LCD.

## Verification

The following tests should be performed on the target hardware:

- [ ] Keypad input detection
- [ ] LCD character display
- [ ] UART transmission
- [ ] UART reception
- [ ] Keypad debounce behavior
- [ ] LCD cursor positioning

## Future Improvements

- Implement interrupt-driven UART reception.
- Add a UART command parser.
- Enable GPIO control through serial commands.
- Implement PWM duty-cycle control.
- Improve keypad debounce logic.
- Add error handling and structured UART messages.
