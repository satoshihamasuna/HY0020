#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern void SystemInit(void);
extern int main(void);

void Default_Handler(void)
{
    for (;;) { }
}

void Reset_Handler(void)
{
    uint32_t *source = &_sidata;
    for (uint32_t *p = &_sdata; p < &_edata; ) { *p++ = *source++; }
    for (uint32_t *p = &_sbss; p < &_ebss; ) { *p++ = 0; }
    SystemInit();
    (void)main();
    for (;;) { }
}

typedef void (*handler_t)(void);
/* nRF52832: 16 core exception slots + 39 external IRQ slots. */
__attribute__((used, section(".isr_vector")))
const handler_t vectors[55] = {
    [0] = (handler_t)&_estack,
    [1] = Reset_Handler,
    [2 ... 54] = Default_Handler,
};
