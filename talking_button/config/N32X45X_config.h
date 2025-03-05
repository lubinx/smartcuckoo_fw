#ifndef __N32X45X_CONFIG_H
#define __N32X45X_CONFIG_H

//***************************************************************************
//<<< Use Configuration Wizard in Context Menu >>>
//***************************************************************************
//  <h> Clock Configure
//      <o> HSE: provided HS-XTAL Crystal/Digital Oscillator frequency
//      <i> due to PLL can only DIV by 1 or 2, it narrows HSE selection
//          <6=>  6 MHz
//          <8=>  8 MHz
//          <12=> 12 MHz
//          <16=> 16 MHz
//          <24=> 24 MHz
    #define HSE_FREQ                    (8UL * 1000000)
//
//      <o> HSI: HS-RC Oscillator frequency
//          <8=> fixed 8 MHz
    #define HSI_FREQ                    (8UL * 1000000)
//
//      <o> LSE: LS-XTAL Crystal Oscillator frequency
//          <32768=> fixed 32768 Hz
    #define LSE_FREQ                    (32768U)
//
//      <o> LSI: LS-RC Oscillator frequency
//          <40=> fixed 40 KHz
    #define LSI_FREQ                    (40UL * 1000)
//
//      <o> SYSCLK/CPU/AHB frequency (MHZ)
//      <i> due to USB can only use PLL output DIV (1 or 1.5 or 2 or 3), it narrows SYSCLK selection
//          <144=> PLL 144 MHz
//          <96=>  PLL 96 MHz
//          <72=>  PLL 72 MHz
//          <48=>  PLL 48 MHz
    #define SYSCLK_FREQ                 (144U * 1000000)
//  </h>
//
//  <h> Clock-Source
//      <o> HS CLK Source
//          <0=> RC Oscillator (HSI)
//          <1=> Crystal Oscillator (HSE)
//          <2=> Digital Oscillator (HSE)
    #define HS_SCLK_SEL                 (1)
//
//      <o> LS CLK Source
//          <0=> RC Oscillator (LSI)
//          <1=> XTAL Crystal Oscillator (LSE)
    #define LS_SCLK_SEL                 (1)
//
//      <o> ADC CLK Source
//          <0=> Internal High Speed Clock (HSI)
//          <1=> External High Speed Clock (HSE)
    #define ADC1M_SCLK_SEL              (0)
//
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
    #define BLUETOOTH_ADV_MIN_INTERVAL  (190)
    #define BLUETOOTH_ADV_MAX_INTERVAL  (210)
//  </h>
//--------------------------------------------------------------------------
//<<< end of configuration section >>>
//**************************************************************************

#endif
