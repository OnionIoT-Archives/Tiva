#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_pwm.h"
#include "inc/hw_gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
//#include "utils/lwiplib.h"
//#include "utils/ustdlib.h"
#include "utils/uartstdio.h"
#include "drivers/pinout.h"
#include "OnionClient.hpp"


//*****************************************************************************
//
// Defines for setting up the system clock.
//
//*****************************************************************************
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)


//*****************************************************************************
//
// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
//
//*****************************************************************************
#define LED_INT_PRIORITY        0x40
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0


//*****************************************************************************
//
// The system clock frequency.
//
//*****************************************************************************
uint32_t g_ui32SysClock;
OnionClient *client;

extern "C"
void SysTickIntHandler(void) {
    if(client != 0) {
        client->systick(SYSTICKMS);
    }
}
//*****************************************************************************
//
// Display an lwIP type IP Address.
//
//*****************************************************************************
//void
//DisplayIPAddress(uint32_t ui32Addr)
//{
//    char pcBuf[16];
//
//    //
//    // Convert the IP Address into a string.
//    //
//    usprintf(pcBuf, "%d.%d.%d.%d", ui32Addr & 0xff, (ui32Addr >> 8) & 0xff,
//            (ui32Addr >> 16) & 0xff, (ui32Addr >> 24) & 0xff);
//
//    //
//    // Display the string.
//    //
//    UARTprintf(pcBuf);
//}


#define WS2812B_DATABIT_PERIOD  150
#define WS2812B_DATAHIGH_VALUE  96
#define WS2812B_DATALOW_VALUE   48
#define WS2812B_RESET_VALUE     2
#define WS2812B_RESET_PERIOD    6100
#define LEDCOUNT    800
#define COLORCOUNT  (LEDCOUNT*3)

volatile uint8_t color[COLORCOUNT] = {0};
volatile uint16_t byteCount = COLORCOUNT;

extern "C" {
    void LEDBitUpdatePWMIntHandler(void) {
        static uint16_t byte_ptr = 0;
        static uint8_t bit_ptr = 7;
        static uint8_t byte = color[0];
        static uint8_t reset_pulse = 0;
        //MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_2, GPIO_PIN_2);
        HWREG(GPIO_PORTN_BASE + (GPIO_O_DATA + (GPIO_PIN_2 << 2))) = GPIO_PIN_2;
        if (reset_pulse == 1) {
            //MAP_PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, WS2812B_RESET_PERIOD);
            HWREG(PWM0_BASE + PWM_GEN_0 + PWM_O_X_LOAD) = WS2812B_RESET_PERIOD - 1;
            //MAP_PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, WS2812B_RESET_VALUE);
            HWREG(PWM0_BASE + (PWM_OUT_1 & 0xFFFFFFC0) + PWM_O_X_CMPB) = WS2812B_RESET_PERIOD - WS2812B_RESET_VALUE;
            reset_pulse = 2;
        } else {
            if (reset_pulse == 2) {
                //MAP_PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, WS2812B_DATABIT_PERIOD);
                HWREG(PWM0_BASE + PWM_GEN_0 + PWM_O_X_LOAD) = WS2812B_DATABIT_PERIOD - 1;
                reset_pulse = 0;
            }
            if (byte & 1) {
                //MAP_PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, WS2812B_DATAHIGH_VALUE);
                HWREG(PWM0_BASE + (PWM_OUT_1 & 0xFFFFFFC0) + PWM_O_X_CMPB) = WS2812B_DATABIT_PERIOD - WS2812B_DATAHIGH_VALUE;
            } else {
                //MAP_PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, WS2812B_DATALOW_VALUE);
                HWREG(PWM0_BASE + (PWM_OUT_1 & 0xFFFFFFC0) + PWM_O_X_CMPB) = WS2812B_DATABIT_PERIOD - WS2812B_DATALOW_VALUE;
            } 
            
            if (bit_ptr == 0) {
                bit_ptr = 7;
                if (++byte_ptr >= byteCount) {
                    byte_ptr = 0;
                    reset_pulse = 1;
                }
                byte = color[byte_ptr];
            } else {
                bit_ptr--;
                byte = byte >> 1;
            }
        }
        MAP_PWMGenIntClear(PWM0_BASE,PWM_GEN_0,PWM_INT_CNT_ZERO | PWM_INT_CNT_AD);
        //MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_2, ~GPIO_PIN_2);
        HWREG(GPIO_PORTN_BASE + (GPIO_O_DATA + (GPIO_PIN_2 << 2))) = 0;
    }
    
}

#define REVERSEBYTE(x) (((x&0x01)<<7)+((x&0x02)<<5)+((x&0x04)<<3)+((x&0x08)<<1)+((x&0x10)>>1)+((x&0x20)>>3)+((x&0x40)>>5)+((x&0x80)>>7))


void setLEDRGB(uint16_t ledNum,uint8_t red, uint8_t green, uint8_t blue) {
    if (ledNum >= LEDCOUNT) {
        return;
    }
    // bits need to be reversed
    // colors packed G,R,B
    color[ledNum*3] = REVERSEBYTE(green);
    color[ledNum*3+1] = REVERSEBYTE(red);
    color[ledNum*3+2] = REVERSEBYTE(blue);
}

void setLED(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    // Convert x/y to index
    uint16_t index = 0;
    if ((y > 31) || (x>24)) {
        return;
    }
    if (y%2 == 0) {
        index = x*32 + (31-y);
    } else {
        index = x*32 + y;
    }
    setLEDRGB(index, r,g,b);
}

uint8_t running,r1,r2,r3,g1,g2,g3,b1,b2,b3;

void ledOn(char** params) {
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
}

void ledOff(char** params) {
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);
}

void start(char** params) {
  running = 1;
}

void stop(char** params) {
  running = 0;
}

uint8_t r,g,b;
void setLED1(char** params) {
    sscanf(params[0],"%hu",&r);
    sscanf(params[1],"%hu",&g);
    sscanf(params[2],"%hu",&b);
    r1 = r;
    g1 = g;
    b1 = b;
    char* status = new char[30];
    sprintf(status, "%hu:%hu:%hu",r,g,b);
    client->publish("/led1",status);
}

void setLED2(char** params) {
    sscanf(params[0],"%hu",&r);
    sscanf(params[1],"%hu",&g);
    sscanf(params[2],"%hu",&b);
    r2 = r;
    g2 = g;
    b2 = b;
    char* status = new char[30];
    sprintf(status, "%hu:%hu:%hu",r,g,b);
    client->publish("/led2",status);
}

void setLED3(char** params) {
    sscanf(params[0],"%hu",&r);
    sscanf(params[1],"%hu",&g);
    sscanf(params[2],"%hu",&b);
    r3 = r;
    g3 = g;
    b3 = b;
    char* status = new char[30];
    sprintf(status, "%hu:%hu:%hu",r,g,b);
    client->publish("/led3",status);
}

void update(char** params) {
    client->publish("/state",params[0]);
}

int main()
{

    //
    // Make sure the main oscillator is enabled because this is required by
    // the PHY.  The system must have a 25MHz crystal attached to the OSC
    // pins. The SYSCTL_MOSC_HIGHFREQ parameter is used when the crystal
    // frequency is 10MHz or higher.
    //
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    //
    // Run from the PLL at 120 MHz.
    //
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);

    //
    // Configure the device pins.
    //
    PinoutSet(true, false);

    //
    // Configure UART.
    //
    //UARTStdioConfig(0, 115200, g_ui32SysClock);

    // Enable PWM Module
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    
    //
    // Clear the terminal and print banner.
    //
    //UARTprintf("\033[2J\033[H");
    //UARTprintf("LED Driver Test\n\n");

    //
    // Configure Port N1 for as an output for the animation LED.
    //
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_2);

    //
    // Initialize LED to OFF (0)
    //
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, (GPIO_PIN_1+GPIO_PIN_2), ~(GPIO_PIN_1+GPIO_PIN_2));

    //
    // Configure SysTick for a periodic interrupt.
    //
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
   MAP_SysTickIntEnable();

    // Init Onion Client
    client = new OnionClient("sC23LMTO","NjFCUXCPSMu2TMIb");
    client->registerFunction("/ledOn",ledOn,0,0);
    client->registerFunction("/ledOff",ledOff,0,0);
    client->registerFunction("/start",start,0,0);
    client->registerFunction("/stop",stop,0,0);
    char *params[3] = {"red","green","blue"};
    client->registerFunction("/led1",setLED1,params,3);
    client->registerFunction("/led2",setLED2,params,3);
    client->registerFunction("/led3",setLED3,params,3);
    char *param[1] = {"state"};
    client->registerFunction("/update",update,param,1);
    //client->registerFunction("/pubTest",pubTest,params,1);
    client->init(g_ui32SysClock);
    //
    // Setup Onion Client
    //
    //httpd_init();

    //
    // Set the interrupt priorities.  We set the SysTick interrupt to a higher
    // priority than the Ethernet interrupt to ensure that the file system
    // tick is processed if SysTick occurs while the Ethernet handler is being
    // processed.  This is very likely since all the TCP/IP and HTTP work is
    // done in the context of the Ethernet interrupt.
    //
    MAP_IntPriorityGroupingSet(4);
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);
    MAP_IntPrioritySet(INT_PWM0_0, 0x20);

    
    // Init PWM Module
    MAP_GPIOPinConfigure(GPIO_PF1_M0PWM1);
    MAP_GPIOPinTypePWM(GPIO_PORTF_BASE, 1<<1);
    MAP_PWMClockSet(PWM0_BASE,PWM_SYSCLK_DIV_1);
    MAP_PWMDeadBandDisable(PWM0_BASE, PWM_GEN_0);
    
//    uint32_t pwm_config = PWM_GEN_MODE_DOWN |
//                          PWM_GEN_MODE_NO_SYNC |
//                          PWM_GEN_MODE_DBG_STOP  |
//                          PWM_GEN_MODE_GEN_NO_SYNC |
//                          PWM_GEN_MODE_DB_NO_SYNC |
//                          PWM_GEN_MODE_FAULT_UNLATCHED |
//                          PWM_GEN_MODE_FAULT_NO_MINPER |
//                          PWM_GEN_MODE_FAULT_LEGACY;
//                          
    uint32_t pwm_config = PWM_GEN_MODE_DOWN |
                          PWM_GEN_MODE_NO_SYNC |
                          PWM_GEN_MODE_DBG_STOP  |
                          PWM_GEN_MODE_GEN_SYNC_LOCAL |
                          PWM_GEN_MODE_DB_NO_SYNC |
                          PWM_GEN_MODE_FAULT_UNLATCHED |
                          PWM_GEN_MODE_FAULT_NO_MINPER |
                          PWM_GEN_MODE_FAULT_LEGACY;
                          
    MAP_PWMGenConfigure(PWM0_BASE, PWM_GEN_0,pwm_config);
    MAP_PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, WS2812B_DATABIT_PERIOD);
    MAP_PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, WS2812B_DATAHIGH_VALUE);
    MAP_PWMPulseWidthSet(PWM0_BASE, PWM_OUT_0, 10);
    MAP_PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, true);
    //MAP_PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_0,PWM_INT_CNT_ZERO);
    MAP_PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_0,PWM_INT_CNT_AD);
    MAP_PWMIntEnable(PWM0_BASE, PWM_INT_GEN_0);
    MAP_IntEnable(INT_PWM0_0);
    MAP_PWMGenEnable(PWM0_BASE, PWM_GEN_0);
    //
    // Loop forever.  All the work is done in interrupt handlers.
    //
    
    setLEDRGB(0,30,0,0);
    setLEDRGB(1,0,30,0);
    setLEDRGB(2,0,0,30);
//    setLEDRGB(3,30,30,0);
//    setLEDRGB(4,0,30,30);
//    setLEDRGB(5,30,0,30);
//    setLEDRGB(6,30,30,30);
//    setLEDRGB(7,255,255,255);
    r1 = g2 = b3 = 30;
    r2 = r3 = g1 = g3 = b1 = b2 = 0;
    uint16_t pos = 0;  
    extern uint32_t millis;
    uint32_t last_millis;
    bool updated = false;
    while(1)
    {  
        //for(uint8_t y=0;y<255;y++) {
        uint8_t y = 30;
//            for (uint16_t r=0;r<60;r++) {
//                setLEDRGB(r,0,0,0);
//            }
        if ((millis - last_millis) > 30) {
          last_millis = millis;
          //if(!updated) {
            if (pos > 0) {
                setLEDRGB(pos-1,0,0,0);
            }
            setLEDRGB(pos+0,r1,g1,b1);
            setLEDRGB(pos+1,r2,g2,b2);
            setLEDRGB(pos+2,r3,g3,b3);
            //setLEDRGB(pos+3,y,y,0);
            //setLEDRGB(pos+4,0,y,y);
            //setLEDRGB(pos+5,y,0,y);
            if (running) {
              pos++;
              if (pos>160) {
                  pos = 0;
              }
            }
            updated = true;
          //}
        } else {
          updated = false;
        }
//            for(uint32_t x=0;x<6000;x++) {
//                setLEDRGB(0,0,0,0);
//            }
        //}
        client->loop();
    }
  return 0;
}