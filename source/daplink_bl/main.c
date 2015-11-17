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
 
#include "main.h"
#include "gpio.h"
#include "validation.h"
#include "virtual_fs_user.h"
#include "RTL.h"
#include "rl_usb.h"
#include "config_settings.h"
#include "version.h"
#include "target_config.h"

__asm void modify_stack_pointer_and_start_app(uint32_t r0_sp, uint32_t r1_pc)
{
    MOV SP, R0
    BX R1
}

// Event flags for main task
// Timers events
#define FLAGS_MAIN_90MS           (1 << 0)
#define FLAGS_MAIN_30MS           (1 << 1)
// USB Events
#define FLAGS_MAIN_USB_DISCONNECT (1 << 3)
#define FLAGS_MAIN_FORCE_MSC_DISCONNECT (1 << 7)
#define FLAGS_MAIN_MSC_DELAY_DISCONNECT (1 << 8)
// Used by msc when flashing a new binary
#define FLAGS_LED_BLINK_30MS      (1 << 6)

// Timing constants (in 90mS ticks)
// USB busy time
#define USB_BUSY_TIME           (33)
// Delay before a USB device connect may occur
#define USB_CONNECT_DELAY       (11)
// Decrement to zero
#define DECZERO(x)              (x ? --x : 0)
#define NO_TIMEOUT              (0xffff)

// Global state of usb used in 
main_usb_connect_t usb_state;

// Reference to our main task
OS_TID main_task_id;

static uint8_t msc_led_usb_activity = 0;
static main_led_state_t msc_led_state = MAIN_LED_FLASH;

static main_usb_busy_t usb_busy;
static uint32_t usb_busy_count;

#define TIMER_TASK_30_PRIORITY  (11)
#define TIMER_TASK_STACK        (100)
static uint64_t stk_timer_task[TIMER_TASK_STACK/sizeof(uint64_t)];

#define MAIN_TASK_PRIORITY      (10)
#define MAIN_TASK_STACK         (200)
static uint64_t stk_main_task [MAIN_TASK_STACK /sizeof(uint64_t)];

// Timer task, set flags every 30mS and 90mS
__task void timer_task_30mS(void)
{
    uint8_t i = 0;
    os_itv_set(3); // 30mS

    while(1) {
        os_itv_wait();
        os_evt_set(FLAGS_MAIN_30MS, main_task_id);
        if (!(i++ % 3)) {
            os_evt_set(FLAGS_MAIN_90MS, main_task_id);
        }
    }
}

// Flash MSD LED using 30mS tick
void main_blink_msc_led(main_led_state_t permanent)
{
    msc_led_usb_activity = 1;
    msc_led_state = (permanent) ? MAIN_LED_FLASH_PERMANENT : MAIN_LED_FLASH;
    return;
}

// A new program has been flashed in the target
void main_msc_disconnect_event(void)
{
    os_evt_set(FLAGS_MAIN_USB_DISCONNECT, main_task_id);
    return;
}

// New data has arrived so the disconnect needs to be delayed
void main_msc_delay_disconnect_event(void)
{
    os_evt_set(FLAGS_MAIN_MSC_DELAY_DISCONNECT, main_task_id);
    return;
}

// A failure has occured during msc programming
void main_force_msc_disconnect_event(void)
{
    os_evt_set(FLAGS_MAIN_FORCE_MSC_DISCONNECT, main_task_id);
    return;
}

__task void main_task(void)
{
    // State processing
    uint16_t flags;
    // LED
    gpio_led_state_t msc_led_value = GPIO_LED_ON;
    // USB
    uint32_t usb_state_count;

    if (config_ram_get_initial_hold_in_bl()) {
        // Delay for 3 seconds for VMs
        os_dly_wait(300);
    }

    // Get a reference to this task
    main_task_id = os_tsk_self();
    
    // Turn on LEDs
    gpio_set_hid_led(GPIO_LED_OFF);
    gpio_set_cdc_led(GPIO_LED_OFF);
    gpio_set_msc_led(GPIO_LED_OFF);

    // Update version information file
    init_auth_config();

    // USB
    usbd_init();
    usbd_connect(0);
    usb_busy = MAIN_USB_IDLE;
    usb_busy_count = 0;
    usb_state = MAIN_USB_CONNECTING;
    usb_state_count = USB_CONNECT_DELAY;

    // Start timer tasks
    os_tsk_create_user(timer_task_30mS, TIMER_TASK_30_PRIORITY, (void *)stk_timer_task, TIMER_TASK_STACK);

    while(1) {
        // need to create a new event for programming failure
        os_evt_wait_or(   FLAGS_MAIN_90MS               // 90mS tick
                        | FLAGS_MAIN_30MS               // 30mS tick
                        | FLAGS_MAIN_USB_DISCONNECT     // Programming complete, disconnect and reset MCU
                        | FLAGS_MAIN_MSC_DELAY_DISCONNECT // new data has arrived so delay the disconnect
                        | FLAGS_MAIN_FORCE_MSC_DISCONNECT // programming error, eject MSC and show error file upon connection
                        , NO_TIMEOUT );

        // Find out what event happened
        flags = os_evt_get();

        // data has arrived over usb so delay the disconnect
        if (flags & FLAGS_MAIN_MSC_DELAY_DISCONNECT) {
            if ((MAIN_USB_DISCONNECT_CONNECT == usb_state) ||
                    (MAIN_USB_DISCONNECTING == usb_state)) {
                // Reset the disconnect counter
                usb_state_count = USB_CONNECT_DELAY;
            }
        }

        // this happens when programming is complete and successful.
        if (flags & FLAGS_MAIN_USB_DISCONNECT) {
            usb_busy = MAIN_USB_IDLE;            // USB not busy
            usb_state_count = USB_BUSY_TIME;
            usb_state = MAIN_USB_DISCONNECTING;  // disconnect the usb
        }
        
        // programming failure occured. Eject and re-connect with fail file.
        if (flags & FLAGS_MAIN_FORCE_MSC_DISCONNECT) {
            usb_busy = MAIN_USB_IDLE;                    // USB not busy
            usb_state_count = USB_BUSY_TIME;
            usb_state = MAIN_USB_DISCONNECT_CONNECT;     // disconnect the usb
        }

        if (flags & FLAGS_MAIN_90MS) {
            // Update USB busy status
            switch (usb_busy) {

                case MAIN_USB_ACTIVE:
                    if (DECZERO(usb_busy_count) == 0) {
                        usb_busy = MAIN_USB_IDLE;
                    }
                    break;

                case MAIN_USB_IDLE:
                default:
                    break;
            }

            // Update USB connect status
            switch (usb_state) {

                case MAIN_USB_DISCONNECTING:
                    // Wait until USB is idle before disconnecting
                    if (usb_busy == MAIN_USB_IDLE && (DECZERO(usb_state_count) == 0)) {
                        usbd_connect(0);
                        usb_state = MAIN_USB_DISCONNECTED;
                    }
                    break;

                case MAIN_USB_DISCONNECT_CONNECT:
                    // Wait until USB is idle before disconnecting
                    if ((usb_busy == MAIN_USB_IDLE) && (DECZERO(usb_state_count) == 0)) {
                        USBD_MSC_MediaReady = 0;
                        usb_state = MAIN_USB_CONNECTING;
						// Delay the connecting state before reconnecting to the host - improved usage with VMs
						usb_state_count = USB_CONNECT_DELAY;
                    }
                    break;

                case MAIN_USB_CONNECTING:
                    // Wait before connecting
                    if (DECZERO(usb_state_count) == 0) {
                        usbd_connect(1);
                        usb_state = MAIN_USB_CHECK_CONNECTED;
                    }
                    break;

                case MAIN_USB_CHECK_CONNECTED:
                    if(usbd_configured()) {
                        usb_state = MAIN_USB_CONNECTED;
                        vfs_user_build_filesystem();
                        USBD_MSC_MediaReady = 1;
                    }
                    break;

                case MAIN_USB_DISCONNECTED:
                    NVIC_SystemReset();
                    break;
                
                case MAIN_USB_CONNECTED:
                default:
                    break;
            }
         }

        // 30mS tick used for flashing LED when USB is busy
        if (flags & FLAGS_MAIN_30MS) {
            if (msc_led_usb_activity && ((msc_led_state == MAIN_LED_FLASH) || (msc_led_state == MAIN_LED_FLASH_PERMANENT))) {
                // Flash MSD LED ONCE
                msc_led_value = (GPIO_LED_ON == msc_led_value) ? GPIO_LED_OFF : GPIO_LED_ON;
                msc_led_usb_activity = ((GPIO_LED_ON == msc_led_value) && (MAIN_LED_FLASH == msc_led_state)) ? 0 : 1;
                // Update hardware
                gpio_set_msc_led(msc_led_value);
            }
        }
    }
}

int main (void)
{
    // init leds and button
    gpio_init();
    // init settings
    config_init();
    // check for invalid app image or rst button press. Should be checksum or CRC but NVIC validation is better than nothing.
    // If the interface has set the hold in bootloader setting don't jump to app
    if (gpio_get_sw_reset() && validate_bin_nvic((uint8_t*)target_device.flash_start) && !config_ram_get_initial_hold_in_bl()) {
        // change to the new vector table
        SCB->VTOR = target_device.flash_start;
        // modify stack pointer and start app
        modify_stack_pointer_and_start_app((*(uint32_t *)(target_device.flash_start)), (*(uint32_t *)(target_device.flash_start + 4)));
    }
    // config the usb interface descriptor and web auth token before USB connects
    //unique_string_auth_config();
    // either the rst pin was pressed or we have an empty app region
    os_sys_init_user(main_task, MAIN_TASK_PRIORITY, stk_main_task, MAIN_TASK_STACK);
}
