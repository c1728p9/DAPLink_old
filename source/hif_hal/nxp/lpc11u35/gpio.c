/* CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "LPC11Uxx.h"
#include "RTL.h"

#include "gpio.h"
#include "compiler.h"
#include "daplink.h"

// This GPIO configuration is only valid for the LPC11U35 HIF
COMPILER_ASSERT(DAPLINK_HIF_ID == DAPLINK_HIF_ID_LPC11U35);

static uint16_t isr_flags;
static OS_TID isr_notify;

#ifdef DBG_LPC812
#define SW_RESET_BUTTON    0
#else
#define SW_RESET_BUTTON    1
#endif

#ifdef SW_RESET_BUTTON
#define RESET_PORT        (1)
#define RESET_PIN         (19)
#define RESET_INT_CH      (0)
#define RESET_INT_MASK    (1 << RESET_INT_CH)
#endif

#define PIN_DAP_LED       (1<<21)
#define PIN_MSD_LED       (1<<20)
#define PIN_CDC_LED       (1<<11)


volatile uint32_t TimeTick = 0;

#define SYSTICK_TICKSPERSEC 100
#define SYSTICK_DELAY_CYCLES (SystemCoreClock/SYSTICK_TICKSPERSEC)
#define INTERNAL_RC_HZ 12000000

#define WDTCLK_SRC_IRC_OSC          0
#define WDTCLK_SRC_WDT_OSC          1


/* This data must be global so it is not read from the stack */
typedef void (*IAP)(uint32_t [], uint32_t []);
IAP iap_entry = (IAP)0x1fff1ff1;
uint32_t command[5], result[4];
#define init_msdstate() *((uint32_t *)(0x10000054)) = 0x0

/* This function resets some microcontroller peripherals to reset
   hardware configuration to ensure that the USB In-System Programming module
   will work properly. It is normally called from reset and assumes some reset
   configuration settings for the MCU.
   Some of the peripheral configurations may be redundant in your specific
   project.
*/
void ReinvokeISP(void)
{
  /* make sure USB clock is turned on before calling ISP */
  LPC_SYSCON->SYSAHBCLKCTRL |= 0x04000;
  /* make sure 32-bit Timer 1 is turned on before calling ISP */
  LPC_SYSCON->SYSAHBCLKCTRL |= 0x00400;
  /* make sure GPIO clock is turned on before calling ISP */
  LPC_SYSCON->SYSAHBCLKCTRL |= 0x00040;
  /* make sure IO configuration clock is turned on before calling ISP */
  LPC_SYSCON->SYSAHBCLKCTRL |= 0x10000;

  /* make sure AHB clock divider is 1:1 */
  LPC_SYSCON->SYSAHBCLKDIV = 1;

  /* Send Reinvoke ISP command to ISP entry point*/
  command[0] = 57;

  init_msdstate();					 /* Initialize Storage state machine */
  /* Set stack pointer to ROM value (reset default) This must be the last
     piece of code executed before calling ISP, because most C expressions
     and function returns will fail after the stack pointer is changed. */
  __set_MSP(*((volatile uint32_t *)0x00000000));

  /* Enter ISP. We call "iap_entry" to enter ISP because the ISP entry is done
     through the same command interface as IAP. */
  iap_entry(command, result);
  // Not supposed to come back!
}



void gpio_init(void) {
    // enable clock for GPIO port 0
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 6);

    // configure GPIO-LED as output
    // DAP led (green)
    LPC_GPIO->DIR[0]  |= (PIN_DAP_LED);
    LPC_GPIO->CLR[0]  |= (PIN_DAP_LED);

    // MSD led (red)
    LPC_GPIO->DIR[0]  |= (PIN_MSD_LED);
    LPC_GPIO->CLR[0]  |= (PIN_MSD_LED);

    // Serial LED (blue)
      LPC_IOCON->TDI_PIO0_11 |= 0x01;
    LPC_GPIO->DIR[0]  |= (PIN_CDC_LED);
    LPC_GPIO->CLR[0]  |= (PIN_CDC_LED);

    // configure Button as input
#if SW_RESET_BUTTON
    LPC_GPIO->DIR[RESET_PORT]  &= ~(1 << RESET_PIN);
#endif

    /* Enable AHB clock to the FlexInt, GroupedInt domain. */
    LPC_SYSCON->SYSAHBCLKCTRL |= ((1<<19) | (1<<23) | (1<<24));
    
    if (gpio_get_sw_reset() == 0) {
        ReinvokeISP();
    }
    
}

void gpio_set_hid_led(gpio_led_state_t state) {
    if (state) {
        LPC_GPIO->SET[0] |= (PIN_DAP_LED);
    } else {
        LPC_GPIO->CLR[0] |= (PIN_DAP_LED);
    }
}

void gpio_set_cdc_led(gpio_led_state_t state) {
    if (state) {
      LPC_GPIO->SET[0] |= (PIN_CDC_LED);
    } else {
      LPC_GPIO->CLR[0] |= (PIN_CDC_LED);
    }
}

void gpio_set_msc_led(gpio_led_state_t state) {
    if (state) {
        LPC_GPIO->SET[0] |= (PIN_MSD_LED);
    } else {
        LPC_GPIO->CLR[0] |= (PIN_MSD_LED);
    }
}

void gpio_enable_button_flag(OS_TID task, uint16_t flags) {
    /* When the "reset" button is pressed the ISR will set the */
    /* event flags "flags" for task "task" */

    /* Keep a local copy of task & flags */
    isr_notify=task;
    isr_flags=flags;

#if SW_RESET_BUTTON
    LPC_SYSCON->STARTERP0 |= RESET_INT_MASK;
    LPC_SYSCON->PINTSEL[RESET_INT_CH] = (RESET_PORT) ? (RESET_PIN + 24) : (RESET_PIN);

    if (!(LPC_GPIO_PIN_INT->ISEL & RESET_INT_MASK))
        LPC_GPIO_PIN_INT->IST = RESET_INT_MASK;

    LPC_GPIO_PIN_INT->IENF |= RESET_INT_MASK;

    NVIC_EnableIRQ(FLEX_INT0_IRQn);
#endif
}

void FLEX_INT0_IRQHandler() {
    isr_evt_set(isr_flags, isr_notify);
    NVIC_DisableIRQ(FLEX_INT0_IRQn);

    // ack interrupt
    LPC_GPIO_PIN_INT->IST = 0x01;
}

uint8_t gpio_get_sw_reset(void)
{
    return LPC_GPIO->W[RESET_PORT * 32 + RESET_PIN] ? 1 : 0;
}

void target_forward_reset(bool assert_reset)
{
    // Do nothing - reset button is already tied to the target 
    //              reset pin on lpc11u35 interface hardware
}
