#include "nrf.h"

#define UART_TX_PIN 6u

/* Number of status lines sent; also visible in a debugger. */
volatile uint32_t print_count;

static void uart_init(void)
{
    /* Crystal clock gives a stable UART baud rate. */
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (!NRF_CLOCK->EVENTS_HFCLKSTARTED) { }

    NRF_P0->OUTSET = 1u << UART_TX_PIN;
    NRF_P0->DIRSET = 1u << UART_TX_PIN;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELTXD = UART_TX_PIN;
    NRF_UART0->PSELRXD = 0xFFFFFFFFu;
    NRF_UART0->PSELCTS = 0xFFFFFFFFu;
    NRF_UART0->PSELRTS = 0xFFFFFFFFu;
    /* 8 data bits, no parity, one stop bit, no flow control. */
    NRF_UART0->CONFIG = 0;
    NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud115200;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
    NRF_UART0->TASKS_STARTTX = 1;
}

static void uart_putc(char character)
{
    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->TXD = (uint8_t)character;
    while (!NRF_UART0->EVENTS_TXDRDY) { }
}

static void uart_print(const char *text)
{
    while (*text) { uart_putc(*text++); }
}

static void uart_print_uint(uint32_t value)
{
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (count) { uart_putc(digits[--count]); }
}

static void delay_ms(uint32_t milliseconds)
{
    /* CPU runs at 64 MHz; poll SysTick without enabling interrupts. */
    SysTick->LOAD = 64000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (milliseconds--) {
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) { }
    }
    SysTick->CTRL = 0;
}

int main(void)
{
    /* Use LDO mode; do not assume an external DC/DC connection. */
    NRF_POWER->DCDCEN = 0;
    uart_init();
    uart_print("\r\nHY0020 UART test started (115200 8N1)\r\n");
    for (;;) {
        uart_print("HY0020 alive: count=");
        uart_print_uint(print_count++);
        uart_print("\r\n");
        delay_ms(1000);
    }
}
