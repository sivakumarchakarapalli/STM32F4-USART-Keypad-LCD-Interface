 /*
 * File: stm32f4_usart_keypad_lcd.c
 * Target: STM32F439ZI / compatible STM32F4 device
 * Function: 4x4 keypad input, 16x2 LCD display, USART2 TX/RX.
 *
 * LCD control:
 *   RS = PE2, RW = PE4, EN = PE5
 * LCD data bus:
 *   DB0 = PE6, DB1 = PE3, DB2 = PF8, DB3 = PF7
 *   DB4 = PF9, DB5 = PG1, DB6 = PG0, DB7 = PD1
 * Keypad:
 *   Rows = PA4..PA7, Columns = PA0..PA3
 * USART2:
 *   TX = PD5 (AF7), RX = PD6 (AF7), 9600 baud for 16 MHz APB1 clock
 */

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

#define RS_PIN 2U
#define RW_PIN 4U
#define EN_PIN 5U

#define KEYPAD_ROW_PORT GPIOA
#define KEYPAD_COL_PORT GPIOA

static const uint8_t ROW_PINS[4] = {4U, 5U, 6U, 7U};
static const uint8_t COL_PINS[4] = {0U, 1U, 2U, 3U};

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static void LCD_INIT(void);
static void PORT_INIT(void);
static void LCD_COMMAND(uint8_t cmd);
static void LCD_DATA(uint8_t data);
static void LCD_SET_CURSOR(uint8_t row, uint8_t col);
static void LCD_PRINT(const char *str);
static void DELAY_MS(uint32_t ms);

static void USART2_init(void);
static void USART2_write(char ch);
static char USART2_read_nonblocking(bool *ok);
static char scan_keypad(void);
static void LCD_CLEAR_RX_AREA(void);

int main(void)
{
    char key;
    char received_char;
    bool rx_ok;
    uint8_t rx_col = 9U; /* "USART RX:" occupies columns 0 through 8 */

    LCD_INIT();
    USART2_init();

    LCD_SET_CURSOR(0U, 0U);
    LCD_PRINT("Keypad:");

    LCD_SET_CURSOR(1U, 0U);
    LCD_PRINT("USART RX:");

    while (1)
    {
        /* Scan keypad, display the key, and transmit it over USART2. */
        key = scan_keypad();

        if (key != '\0')
        {
            LCD_SET_CURSOR(0U, 7U);
            LCD_DATA((uint8_t)key);
            USART2_write(key);
        }

        /* Poll USART2 without blocking when no character is available. */
        received_char = USART2_read_nonblocking(&rx_ok);

        if (rx_ok)
        {
            if (rx_col >= 16U)
            {
                LCD_CLEAR_RX_AREA();
                rx_col = 9U;
            }

            LCD_SET_CURSOR(1U, rx_col);
            LCD_DATA((uint8_t)received_char);
            rx_col++;
        }

        DELAY_MS(10U);
    }
}

/* ---------------- USART2 functions ---------------- */

static void USART2_init(void)
{
    /* Enable GPIOD and USART2 clocks. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PD5 = USART2_TX, PD6 = USART2_RX, alternate-function mode. */
    GPIOD->MODER &= ~((3U << (5U * 2U)) | (3U << (6U * 2U)));
    GPIOD->MODER |=  ((2U << (5U * 2U)) | (2U << (6U * 2U)));

    /* AF7 for PD5 and PD6. */
    GPIOD->AFR[0] &= ~((0xFU << (5U * 4U)) | (0xFU << (6U * 4U)));
    GPIOD->AFR[0] |=  ((7U << (5U * 4U)) | (7U << (6U * 4U)));

    /*
     * BRR = 0x0683 is suitable for 9600 baud when USART2's APB1 clock
     * is 16 MHz. Change this value if your clock configuration differs.
     */
    USART2->BRR = 0x0683U;

    USART2->CR1 = USART_CR1_TE | USART_CR1_RE;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->CR1 |= USART_CR1_UE;
}

static void USART2_write(char ch)
{
    while ((USART2->SR & USART_SR_TXE) == 0U)
    {
        /* Wait until the transmit data register is empty. */
    }

    USART2->DR = (uint8_t)ch;
}

static char USART2_read_nonblocking(bool *ok)
{
    if ((USART2->SR & USART_SR_RXNE) != 0U)
    {
        *ok = true;
        return (char)(USART2->DR & 0xFFU);
    }

    *ok = false;
    return '\0';
}

/* ---------------- Keypad scanning ---------------- */

static char scan_keypad(void)
{
    uint8_t row;
    uint8_t col;
    uint8_t pin;
    uint32_t input;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* Configure PA4..PA7 as outputs and PA0..PA3 as inputs with pull-ups. */
    for (row = 0U; row < 4U; row++)
    {
        pin = ROW_PINS[row];
        KEYPAD_ROW_PORT->MODER &= ~(3U << (pin * 2U));
        KEYPAD_ROW_PORT->MODER |=  (1U << (pin * 2U));
        KEYPAD_ROW_PORT->BSRR = (1U << pin);
    }

    for (col = 0U; col < 4U; col++)
    {
        pin = COL_PINS[col];
        KEYPAD_COL_PORT->MODER &= ~(3U << (pin * 2U));
        KEYPAD_COL_PORT->PUPDR &= ~(3U << (pin * 2U));
        KEYPAD_COL_PORT->PUPDR |=  (1U << (pin * 2U));
    }

    for (row = 0U; row < 4U; row++)
    {
        uint8_t j;

        /* Drive the selected row low and keep the other rows high. */
        for (j = 0U; j < 4U; j++)
        {
            if (j == row)
            {
                KEYPAD_ROW_PORT->BSRR = (1U << (ROW_PINS[j] + 16U));
            }
            else
            {
                KEYPAD_ROW_PORT->BSRR = (1U << ROW_PINS[j]);
            }
        }

        DELAY_MS(2U);

        for (col = 0U; col < 4U; col++)
        {
            pin = COL_PINS[col];
            input = (KEYPAD_COL_PORT->IDR >> pin) & 1U;

            if (input == 0U)
            {
                /* Debounce by waiting and checking the same column again. */
                DELAY_MS(20U);
                input = (KEYPAD_COL_PORT->IDR >> pin) & 1U;

                if (input == 0U)
                {
                    /* Wait for release to avoid repeatedly reporting one press. */
                    while (((KEYPAD_COL_PORT->IDR >> pin) & 1U) == 0U)
                    {
                        DELAY_MS(10U);
                    }

                    /* Return all keypad rows high before returning. */
                    for (j = 0U; j < 4U; j++)
                    {
                        KEYPAD_ROW_PORT->BSRR = (1U << ROW_PINS[j]);
                    }

                    return keymap[row][col];
                }
            }
        }
    }

    /* No key pressed: restore all rows high. */
    for (row = 0U; row < 4U; row++)
    {
        KEYPAD_ROW_PORT->BSRR = (1U << ROW_PINS[row]);
    }

    return '\0';
}

/* ---------------- LCD functions ---------------- */

static void LCD_INIT(void)
{
    PORT_INIT();
    DELAY_MS(100U);

    /* Standard HD44780 initialization sequence for 8-bit mode. */
    LCD_COMMAND(0x30U);
    DELAY_MS(30U);
    LCD_COMMAND(0x30U);
    DELAY_MS(1U);
    LCD_COMMAND(0x30U);

    LCD_COMMAND(0x38U); /* 8-bit interface, 2 lines, 5x8 font */
    LCD_COMMAND(0x0CU); /* Display on, cursor off */
    LCD_COMMAND(0x06U); /* Increment cursor after each character */
    LCD_COMMAND(0x01U); /* Clear display */
    DELAY_MS(2U);
    LCD_COMMAND(0x80U); /* Cursor home */
}

static void PORT_INIT(void)
{
    /* Enable GPIOE, GPIOF, GPIOG, GPIOD, and GPIOA clocks. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN |
                    RCC_AHB1ENR_GPIOFEN |
                    RCC_AHB1ENR_GPIOGEN |
                    RCC_AHB1ENR_GPIODEN |
                    RCC_AHB1ENR_GPIOAEN;

    /* LCD control pins PE2, PE4, PE5 as outputs. */
    GPIOE->MODER &= ~((3U << (RS_PIN * 2U)) |
                      (3U << (RW_PIN * 2U)) |
                      (3U << (EN_PIN * 2U)));
    GPIOE->MODER |=  ((1U << (RS_PIN * 2U)) |
                      (1U << (RW_PIN * 2U)) |
                      (1U << (EN_PIN * 2U)));

    GPIOE->BSRR = (1U << (RS_PIN + 16U)) |
                  (1U << (RW_PIN + 16U)) |
                  (1U << (EN_PIN + 16U));

    /* LCD data pins PE3 and PE6 as outputs. */
    GPIOE->MODER &= ~((3U << (3U * 2U)) | (3U << (6U * 2U)));
    GPIOE->MODER |=  ((1U << (3U * 2U)) | (1U << (6U * 2U)));

    /* LCD data pins PF7, PF8, and PF9 as outputs. */
    GPIOF->MODER &= ~((3U << (7U * 2U)) |
                      (3U << (8U * 2U)) |
                      (3U << (9U * 2U)));
    GPIOF->MODER |=  ((1U << (7U * 2U)) |
                      (1U << (8U * 2U)) |
                      (1U << (9U * 2U)));

    /* LCD data pins PG0 and PG1 as outputs. */
    GPIOG->MODER &= ~((3U << (0U * 2U)) | (3U << (1U * 2U)));
    GPIOG->MODER |=  ((1U << (0U * 2U)) | (1U << (1U * 2U)));

    /* LCD data pin PD1 as output. */
    GPIOD->MODER &= ~(3U << (1U * 2U));
    GPIOD->MODER |=  (1U << (1U * 2U));

    /* Configure keypad rows as outputs and columns as pulled-up inputs. */
    for (uint8_t i = 0U; i < 4U; i++)
    {
        uint8_t row_pin = ROW_PINS[i];
        uint8_t col_pin = COL_PINS[i];

        KEYPAD_ROW_PORT->MODER &= ~(3U << (row_pin * 2U));
        KEYPAD_ROW_PORT->MODER |=  (1U << (row_pin * 2U));
        KEYPAD_ROW_PORT->BSRR = (1U << row_pin);

        KEYPAD_COL_PORT->MODER &= ~(3U << (col_pin * 2U));
        KEYPAD_COL_PORT->PUPDR &= ~(3U << (col_pin * 2U));
        KEYPAD_COL_PORT->PUPDR |=  (1U << (col_pin * 2U));
    }
}

static void LCD_COMMAND(uint8_t cmd)
{
    /* RS = 0, RW = 0: command mode. */
    GPIOE->BSRR = (1U << (RS_PIN + 16U)) | (1U << (RW_PIN + 16U));

    /* Map each command bit to the corresponding LCD data pin. */
    GPIOE->BSRR = (cmd & 0x01U) ? (1U << 6U) : (1U << 22U); /* DB0 = PE6 */
    GPIOE->BSRR = (cmd & 0x02U) ? (1U << 3U) : (1U << 19U); /* DB1 = PE3 */
    GPIOF->BSRR = (cmd & 0x04U) ? (1U << 8U) : (1U << 24U); /* DB2 = PF8 */
    GPIOF->BSRR = (cmd & 0x08U) ? (1U << 7U) : (1U << 23U); /* DB3 = PF7 */
    GPIOF->BSRR = (cmd & 0x10U) ? (1U << 9U) : (1U << 25U); /* DB4 = PF9 */
    GPIOG->BSRR = (cmd & 0x20U) ? (1U << 1U) : (1U << 17U); /* DB5 = PG1 */
    GPIOG->BSRR = (cmd & 0x40U) ? (1U << 0U) : (1U << 16U); /* DB6 = PG0 */
    GPIOD->BSRR = (cmd & 0x80U) ? (1U << 1U) : (1U << 17U); /* DB7 = PD1 */

    /* Enable pulse latches the byte into the LCD. */
    GPIOE->BSRR = (1U << EN_PIN);
    DELAY_MS(1U);
    GPIOE->BSRR = (1U << (EN_PIN + 16U));
    DELAY_MS(1U);

    if ((cmd == 0x01U) || (cmd == 0x02U))
    {
        DELAY_MS(2U);
    }
}

static void LCD_DATA(uint8_t data)
{
    /* RS = 1, RW = 0: data mode. */
    GPIOE->BSRR = (1U << RS_PIN) | (1U << (RW_PIN + 16U));

    GPIOE->BSRR = (data & 0x01U) ? (1U << 6U) : (1U << 22U); /* DB0 = PE6 */
    GPIOE->BSRR = (data & 0x02U) ? (1U << 3U) : (1U << 19U); /* DB1 = PE3 */
    GPIOF->BSRR = (data & 0x04U) ? (1U << 8U) : (1U << 24U); /* DB2 = PF8 */
    GPIOF->BSRR = (data & 0x08U) ? (1U << 7U) : (1U << 23U); /* DB3 = PF7 */
    GPIOF->BSRR = (data & 0x10U) ? (1U << 9U) : (1U << 25U); /* DB4 = PF9 */
    GPIOG->BSRR = (data & 0x20U) ? (1U << 1U) : (1U << 17U); /* DB5 = PG1 */
    GPIOG->BSRR = (data & 0x40U) ? (1U << 0U) : (1U << 16U); /* DB6 = PG0 */
    GPIOD->BSRR = (data & 0x80U) ? (1U << 1U) : (1U << 17U); /* DB7 = PD1 */

    GPIOE->BSRR = (1U << EN_PIN);
    DELAY_MS(1U);
    GPIOE->BSRR = (1U << (EN_PIN + 16U));
    DELAY_MS(1U);
}

static void LCD_SET_CURSOR(uint8_t row, uint8_t col)
{
    uint8_t address = (uint8_t)(col + ((row != 0U) ? 0x40U : 0x00U));
    LCD_COMMAND((uint8_t)(0x80U | address));
}

static void LCD_PRINT(const char *str)
{
    while (*str != '\0')
    {
        LCD_DATA((uint8_t)*str);
        str++;
    }
}

static void LCD_CLEAR_RX_AREA(void)
{
    /* Clear columns 9..15 without erasing the "USART RX:" label. */
    LCD_SET_CURSOR(1U, 9U);
    for (uint8_t i = 9U; i < 16U; i++)
    {
        LCD_DATA((uint8_t)' ');
    }
}

/*
 * Simple software delay. Its duration depends on the CPU clock and compiler
 * optimization settings; calibrate it for your board if accurate timing matters.
 */
static void DELAY_MS(uint32_t ms)
{
    volatile uint32_t i;

    while (ms-- > 0U)
    {
        for (i = 0U; i < 3195U; i++)
        {
            __NOP();
        }
    }
}
