#ifndef __N32X45X_CONFIG_H
#define __N32X45X_CONFIG_H

//***************************************************************************
//<<< Use Configuration Wizard in Context Menu >>>
//***************************************************************************
//  <h> Clock Configure
//      <o> HS CLK Source
//          <0=> RC Oscillator (HSI 8 MHz)
//          <1=> Crystal Oscillator (HSE)
//          <2=> Digital Oscillator (HSE)
    #define HS_SCLK_SEL                 (1)
////
//      <o> LS CLK Source
//          <0=> RC Oscillator (LSI 40 KHz)
//          <1=> XTAL Crystal Oscillator (LSE 32768 Hz)
    #define LS_SCLK_SEL                 (1)
//
//      <o> SYSCLK / CPU / AHB frequency (MHZ)
//      <i> due to USB can only use PLL output DIV (1 or 1.5 or 2 or 3), it narrows SYSCLK selection
//          <144=> PLL 144 MHz
//          <96=>  PLL 96 MHz
//          <72=>  PLL 72 MHz
//          <48=>  PLL 48 MHz
    #define SYSCLK_FREQ                 (144U * 1000000)
//
//      <o> HSE: provided HS-XTAL Crystal / Digital Oscillator frequency
//      <i> due to PLL can only DIV by 1 or 2, it narrows HSE selection
//          <6=>  6 MHz
//          <8=>  8 MHz
//          <12=> 12 MHz
//          <16=> 16 MHz
//          <24=> 24 MHz
    #define HSE_FREQ                    (8UL * 1000000)
//  </h>
//
//  <h> Clock-Source
//      <o> RNG CLK Source
//          <0=> Internal High Speed Clock (HSI)
//          <1=> External High Speed Clock (HSE)
    #define RNG1M_SCLK_SEL              (0)
//  </h>
//
//  <h> Power mangement
//      <q> OS tickless goes EM2
    #define PMU_EM2_EN                  (1)
//  </h>
//
//  <h> Bluetooth
//      <o0> Minimum advertising interval
//      <o1> Maximum advertising interval
    #define BLUETOOTH_ADV_MIN_INTERVAL  (650)
    #define BLUETOOTH_ADV_MAX_INTERVAL  (650)
//  </h>
//--------------------------------------------------------------------------
//<<< end of configuration section >>>
//**************************************************************************

#endif
