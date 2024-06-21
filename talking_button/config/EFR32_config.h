#ifndef __EFR32_CONFIG_H
#define __EFR32_CONFIG_H

//***************************************************************************
//<<< Use Configuration Wizard in Context Menu >>>
//***************************************************************************
//  <h> CMU Configure
//      <o> SYSCLK / HCLK Selection
//          <1=>FSRCO (no BLE support)
//          <2=>Digital-PLL
//          <3=>HFXO
    #define SYSCLK_SEL                  (3)
//          <n> ---> SYSCLK / HCLK DIV(1 performance) ==> HCLK (CPU)
//          <n> ---> HCLK / PCLK DIV(1/2 calculated) ==> PCLK (Peripherial)
//          <n> ---> PCLK / LSPCLK DIV(2) ==> LSPCLK (Low Speed Peripheral)
//      <o> LF 32768 CLK Source
//          <0=> RC Oscillator (LFRCO)
//          <1=> Crystal Oscillator (LFXO)
    #define LF_32K_SOURCE               (1)
//***************************************************************************
//      <h> CLock Group
//          <o> EM0/1 CLK Group A Selection
//              <1=>Digital-PLL
//              <2=>HFXO
//              <3=>FSRCO
    #define EM01_GRP_A_CLK_SEL          (2)
//          <o> EM0/1 CLK Group B Selection
//              <1=>Digital-PLL
//              <2=>HFXO
//              <3=>FSRCO
    #define EM01_GRP_B_CLK_SEL          (2)
//          <o> EM2/3 CLK Group Selection
//              <0=>LF (LFRCO/LFXO)
//              <3=>ULFRCO
    #define EM23_CLK_SEL                (0)
//          <o> EM4 CLK Group Selection
//              <0=>LF (LFRCO/LFXO)
//              <3=>ULFRCO
    #define EM4_CLK_SEL                 (0)
//      </h>
//***************************************************************************
//      <h> Peripherial Clock Route
//          <o> ADC
//              <1=>EM0/1 CLK Group A
//              <2=>FSRCO (20M internal)
    #define ADC_CLK_SEL                 (1)
//          <o> EUART0
//              <1=>EM0/1 CLK Group A
//              <2=>EM2/3 CLK Group
    #define EUART0_CLK_SEL              (2)
//          <o> WDT
//              <0=>LF (LFRCO/LFXO) CLK
//              <3=>ULFRCO (1000Hz)
//              <4=>HCLK / 1024
    #define WDT_CLK_SEL                 (0)
//  <n>----------------------- FIXED ROUTE ----------------------------------
//          <o> TIMER[n]
//              <1=>EM0/1 CLK Group A
    #define TIMER_CLK_SEL               (1)
//          <o> PDM
//              <1=>EM0/1 CLK Group B
    #define PDM_CLK_SEL                 (1)
//          <o> RTCC
//              <2=>EM2/3 CLK Group
    #define RTCC_CLK_SEL                (2)
//          <o> LETIMER
//              <2=>EM2/3 CLK Group
    #define LETIMER_CLK_SEL             (2)
//      </h>
//  </h>
//***************************************************************************
//  <h> Oscillators Configure
//      <h> HXFO
//          <e> External Digital-CLK Mode
    #define HFXO_IS_EXTCLK              (0)
//              <q> Squaring buffer schmitt trigger
    #define HXFO_SQBUFSCHTRGANA         (0)
//              <q> Enable XI Internal DC Bias
    #define HXFO_ENXIDCBIASANA          (0)
//          </e>
//  <n>----------------------- XTALCFG --------------------------------------
//  <i> 9.2.5 HFXO Register Description
//          <o> The core bias LSB change timeout
//          <i> wait duration for the CORE BIAS change to settle out
//              <0=> 8 us
//              <1=> 20 us
//              <2=> 41 us
//              <3=> 62 us
//              <4=> 83 us      (RESET default)
//              <5=> 104 us
//              <6=> 125 us
//              <7=> 166 us
//              <8=> 208 us
//              <9=> 250 us
//              <10=> 333 us
//              <11=> 416 us    (Simplicity IDE default)
//              <12=> 833 us
//              <13=> 1250 us
//              <14=> 2083 us
//              <15=> 3750 us
    #define HFXO_TIMEOUTCBLSB           (11)
//          <o> The steady state timeout
//              <0=> 8 us
//              <1=> 41 us
//              <2=> 83 us      (Simplicity IDE default)
//              <3=> 125 us
//              <4=> 166 us     (RESET default)
//              <5=> 208 us
//              <6=> 250 us
//              <7=> 333 us
//              <8=> 416 us
//              <9=> 500 us
//              <10=> 666 us
//              <11=> 833 us
//              <12=> 1666 us
//              <13=> 2500 us
//              <14=> 4166 us
//              <15=> 7500 us
    #define HFXO_TIMEOUTSTEADY          (2)
//          <o> Startup Tuning Capacitance on XO
    #define HFXO_CTUNEXOSTARTUP         (0)
//          <o> Startup Tuning Capacitance on XI
    #define HFXO_CTUNEXISTARTUP         (0)
//          <o> Startup Core Bias Current
    #define HFXO_COREBIASSTARTUP        (32)
//          <o> Intermediate Startup Core Bias Current
    #define HFXO_COREBIASSTARTUPI       (32)
//  <n>----------------------- XTALCTRL -------------------------------------
//          <o> Core Degeneration
//              <0=> Do not apply core degeneration resistence
//              <1=> Apply 33 ohm core degeneration resistence
//              <2=> Apply 50 ohm core degeneration resistence
//              <3=> Apply 100 ohm core degeneration resistence
    #define HFXO_COREDGENANA            (0)
//          <o> Fixed Tuning Capacitance
//              <0=> Remove fixed capacitance on XI and XO nodes
//              <1=> Adds fixed capacitance on XI node
//              <2=> Adds fixed capacitance on XO node
//              <3=> Adds fixed capacitance on both XI and XO nodes
    #define HFXO_CTUNEFIXANA            (3)
//          <o> Tuning Capacitance on XO (pF)   <0-20400><#/80>
    #define HFXO_CTUNEXOANA             (120)
//          <o> Tuning Capacitance on XI (pF)   <0-20400><#/80>
    #define HFXO_CTUNEXIANA             (120)
//          <o> Core Bias Current (uA)          <1-255><#/10>
    #define HFXO_COREBIASANA            (60)
//      </h>
//***************************************************************************
//      <h> LFXO
//          <o> Mode
//              <0=> XTAL
//              <1=> External Sine Source
//              <2=> External Digital-CLK
    #define LFXO_MODE                   (0)
//          <o> PPM                 <1-500>
//          <i> this is user provide precision value of LFXO crystal
//          <i> 500ppm is minimum requirement for BLE
    #define LFXO_PRECISION_PPM          (40)
//          <o> Startup Delay
//              <0=> 2
//              <1=> 256
//              <1=> 512
//              <2=> 1024
//              <3=> 2048
//              <4=> 4096
//              <5=> 8192
//              <6=> 16384
//              <7=> 32768
    #define LFXO_STARTUP_DELAY          (3)
//          <q> Automatic Gain Control
    #define LFXO_AGC_EN                 (1)
//          <q> High Amplitude
//          <i> no effect when AGC is disabled
    #define LFXO_HIGH_AMPLITDUE_EN      (0)
//          <o> Load Capacitance (1/10) pf
    #define LFXO_LOAD_CAPACITANCE       (100)
//          <o> External Capacitance (1/10) pf
    #define LFXO_EXT_CAPACITANCE        (0)
//      </h>
//***************************************************************************
//      <h> Digital-PLL (optional)
//      <i> Digital-PLL is auto enabled if CMU configure to use it
//          <o> Tune Frequency
//              <0=> 32 MHz
//              <1=> 48 MHz
//              <2=> 64 MHz
//              <3=> 80 MHz
    #define DPLL_TUNE_IDX               (2)
//          <o> Operating Mode Control
//              <0=> Frequency Lock
//              <1=> Phase Lock
    #define DPLL_LOCK_MODE              (0)
//          <o> Reference Edge Select
//              <0=> Falling Edge
//              <1=> Raising Edge
    #define DPLL_EDGESEL                (0)
//          <q> Automatic Recovery Control
    #define DPLL_AUTORECOVER            (1)
//          <q> Dither Enable Control
    #define DPLL_DITHEN                 (0)
//      </h>
//  </h>
//***************************************************************************
//  <h> GPIO
//      <h> GPIOA Port Control
//          <o> Slewrate (0~7)
    #define GPIOA_SLEWRATE              (4)
//      <h> PULL-UP when initialize / disabled
//          <q> PA00
    #define PA00_DISABLE_PULL_UP        (1)
//          <q> PA01
    #define PA01_DISABLE_PULL_UP        (0)
//          <q> PA02
    #define PA02_DISABLE_PULL_UP        (0)
//          <q> PA03
    #define PA03_DISABLE_PULL_UP        (0)
//          <q> PA04 (BRD4183A: Disabled + Pull-Up for VCOM)
    #define PA04_DISABLE_PULL_UP        (0)
//          <q> PA05
    #define PA05_DISABLE_PULL_UP        (0)
//          <q> PA06
    #define PA06_DISABLE_PULL_UP        (0)
//          <q> PA07
    #define PA07_DISABLE_PULL_UP        (0)
//          <q> PA08
    #define PA08_DISABLE_PULL_UP        (1)
//      </h> </h>
//------------------------------------------
//      <h> GPIOB Port Control
//          <o> Slewrate (0~7)
    #define GPIOB_SLEWRATE              (4)
//      <h> PULL-UP when initialize / disabled
//          <q> PB00
    #define PB00_DISABLE_PULL_UP        (1)
//          <q> PB01
    #define PB01_DISABLE_PULL_UP        (0)
//          <q> PB02
    #define PB02_DISABLE_PULL_UP        (0)
//          <q> PB03
    #define PB03_DISABLE_PULL_UP        (0)
//          <q> PB04 (BRD4182A: Disabled + Pull-Up for VCOM)
    #define PB04_DISABLE_PULL_UP        (0)
//      </h> </h>
//------------------------------------------
//      <h> GPIOC Port Control
//          <o> Slewrate (0~7)
    #define GPIOC_SLEWRATE              (4)
//      <h> PULL-UP when initialize / disabled
//          <q> PC00
    #define PC00_DISABLE_PULL_UP        (0)
//          <q> PC01
    #define PC01_DISABLE_PULL_UP        (0)
//          <q> PC02
    #define PC02_DISABLE_PULL_UP        (0)
//          <q> PC03
    #define PC03_DISABLE_PULL_UP        (0)
//          <q> PC04
    #define PC04_DISABLE_PULL_UP        (0)
//          <q> PC05
    #define PC05_DISABLE_PULL_UP        (0)
//          <q> PC06
    #define PC06_DISABLE_PULL_UP        (0)
//          <q> PC07
    #define PC07_DISABLE_PULL_UP        (0)
//          <q> PC08
    #define PC08_DISABLE_PULL_UP        (0)
//      </h> </h>
//------------------------------------------
//      <h> GPIOD Port Control
//          <o> Slewrate (0~7)
    #define GPIOD_SLEWRATE              (4)
//      <h> PULL-UP when initialize / disabled
//          <q> PD00
    #define PD00_DISABLE_PULL_UP        (0)
//          <q> PD01
    #define PD01_DISABLE_PULL_UP        (0)
//          <q> PD02
    #define PD02_DISABLE_PULL_UP        (0)
//          <q> PD03
    #define PD03_DISABLE_PULL_UP        (0)
//      </h> </h>
//  </h>
//***************************************************************************
//  <h> Peripheral Configure
//      <h> DCDC
//          <q> Bypass
    #define DCDC_BYPASS                 (0)
//          <o> VREGVDD Comparator
//              <0=> 2.0V
//              <1=> 2.1V
//              <2=> 2.1V
//              <3=> 2.3V
    #define DCDC_VREGVDD_CMP            (3)
//          <o> Peak Current Timeout
//              <0=> Off
//              <1=> 0.35us
//              <2=> 0.63us
//              <3=> 0.91us
//              <4=> 1.19us
//              <5=> 1.47us
//              <6=> 1.75us
//              <7=> 2.03us
    #define DCDC_IPEEK_TIMEO            (4)
//          <q> DCM only mode
    #define DCDC_DCM_ONLY               (1)
//          <h> EM 0/1 Control
//              <o.8..9>  Efficiency(Drive Speed)
//                  <0=> Lowest / Best EMI
//                  <1=> Default
//                  <2=> Intermediate
//                  <3=> Best
//              <o.0..3>  EM 0/1 Peak Current
//                  <3=> Peek  90mA / Load 36mA
//                  <4=> Peek 100mA / Load 40mA
//                  <5=> Peek 110mA / Load 44mA
//                  <6=> Peek 120mA / Load 48mA
//                  <7=> Peek 130mA / Load 52mA
//                  <8=> Peek 140mA / Load 56mA
//                  <9=> Peek 150mA / Load 60mA
    #define DCDC_EM01_CTRL              (777)
//          </h>
//          <h> EM 2/3 Control
//              <o.8..9>  Efficiency(Drive Speed)
//                  <0=> Lowest / Best EMI
//                  <1=> Default
//                  <2=> Intermediate
//                  <3=> Best
//              <o.0..3>  EM 0/1 Peak Current
//                  <3=> Peek  90mA / Load  5mA
//                  <9=> Peek 150mA / Load 10mA
    #define DCDC_EM23_CTRL              (771)
//          </h>
//      </h>
//      <h> PMU
//          <q> OS tickless goes EM2
    #define PMU_EM2_EN                  (1)
//      </h>
//      <h> Radio
//          <h> Blueooth
//              <o0> Minimum advertising interval
//              <o1> Maximum advertising interval
    #define BLUETOOTH_ADV_MIN_INTERVAL  (800)
    #define BLUETOOTH_ADV_MAX_INTERVAL  (800)
//          </h>
//      </h>
//  </h>
//--------------------------------------------------------------------------
//<<< end of configuration section >>>
//**************************************************************************
#endif
