// Minimal, CubeMX-style smoke test - no ADIOS involved at all, just raw
// CMSIS register writes. Sole purpose: prove code in flash at 0x08000000
// actually executes on this board, independent of every other variable
// (ADIOS init chain, bare-metal loop, dispatcher, option bytes...).
// PC6 driven high and left high - onboard user LED per board schematic.

#include "stm32g0xx.h"

int main(void)
{
  // enable GPIOC clock (RCC_IOPENR bit 2 = GPIOC)
  RCC->IOPENR |= RCC_IOPENR_GPIOCEN;

  // PC6 general purpose output (MODER[13:12] = 01)
  GPIOC->MODER &= ~(3UL << (6 * 2));
  GPIOC->MODER |=  (1UL << (6 * 2));

  // drive PC6 high
  GPIOC->BSRR = (1UL << 6);

  while (1) {
  }

  return 0;
}

void SystemInit(void)
{
}
