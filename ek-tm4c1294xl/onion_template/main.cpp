#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
//#include "utils/lwiplib.h"
//#include "utils/ustdlib.h"
//#include "utils/uartstdio.h"
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

void ledOn(char** params) {
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
}

void ledOff(char** params) {
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);
}

void pubTest(char** params) {
    client->publish("/publish",params[0]);
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
    // Configure the device pins.
    //
    PinoutSet(true, false);

    //
    // Configure UART.
    //
    //UARTStdioConfig(0, 115200, g_ui32SysClock);

    //
    // Clear the terminal and print banner.
    //
    //UARTprintf("\033[2J\033[H");
    //UARTprintf("Ethernet lwIP example\n\n");

    //
    // Configure Port N1 for as an output for the animation LED.
    //
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);

    //
    // Initialize LED to OFF (0)
    //
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);

    //
    // Configure SysTick for a periodic interrupt.
    //
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
    MAP_SysTickIntEnable();

    // Init Onion Client
    client = new OnionClient("DEVICE_ID","DEVICE_KEY");
    client->registerFunction("/on",ledOn,0,0);
    client->registerFunction("/off",ledOff,0,0);
    char *params[1] = {"data"};
    client->registerFunction("/pubTest",pubTest,params,1);
    client->init(g_ui32SysClock);
    //
    // Setup Onion Client
    //
    //httpd_init();
    //client->begin();
    //
    // Set the interrupt priorities.  We set the SysTick interrupt to a higher
    // priority than the Ethernet interrupt to ensure that the file system
    // tick is processed if SysTick occurs while the Ethernet handler is being
    // processed.  This is very likely since all the TCP/IP and HTTP work is
    // done in the context of the Ethernet interrupt.
    //
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);

    //
    // Loop forever.  All the work is done in interrupt handlers.
    //
    while(1)
    {
      client->loop();
    }
  return 0;
}
