/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2016 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for STM32L4 processor
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include <zephyr/arch/arm/aarch32/nmi.h>
#include <zephyr/irq.h>
#include <zephyr/linker/linker-defs.h>
#include <string.h>
#include <soc.h>


/**
 * @brief Perform basic hardware initialization at boot.
 *
 * This needs to be run from the very beginning.
 * So the init priority has to be 0 (zero).
 *
 * @return 0
 */
static int bluenrg_init(const struct device *arg)
{
	uint32_t key;

	ARG_UNUSED(arg);

	key = irq_lock();

	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	irq_unlock(key);
	
	/* enable clock */
	// LL_Power à ajouter (pour le sleep mode donc pas obligé mtn)
	// // BEGIN of SystemInit() 
	LL_PWR_EnableRAMBankRet(LL_PWR_RAMRET_1);
	// BEGIN of SmpsTrimConfig() 
	SystemReadyWait(200, LL_PWR_IsSMPSReady, 1);
	/* Config HW SMPS 10uH */
	LL_PWR_SetSMPSBOM(LL_PWR_SMPS_BOM3);
	/* SMPS Clock 4Mhz configuration */
	LL_RCC_SetSMPSPrescaler(LL_RCC_SMPS_DIV_4);

	uint32_t main_regulator, smps_out_voltage, lsi_bw, hsi_calib, lsi_lpmu;
	uint8_t eng_lsi_bw_flag;
	
	/* Retrieve Trimming values from engineering flash locations */
	if (*(volatile uint32_t*) 0x10001EF8 == 0xFCBCECCC) {
		main_regulator    = ((*(volatile uint32_t*)0x10001EE4) & (0x0F << 0)) >> 0;
		smps_out_voltage  = ((*(volatile uint32_t*)0x10001EE4) & (0x07 << 4)) >> 4;
		lsi_lpmu          = ((*(volatile uint32_t*)0x10001EE4) & (0x0F << 8)) >> 8;
		lsi_bw            = ((*(volatile uint32_t*)0x10001EE4) & (0x0F << 12)) >> 12;
		hsi_calib         = ((*(volatile uint32_t*)0x10001EE4) & (0x3F << 16)) >> 16;
		eng_lsi_bw_flag   = 1U; //TRUE
	} 
	else {
		main_regulator    = 0x08;
		lsi_lpmu          = 0x08;
		hsi_calib         = 0x1E;
		eng_lsi_bw_flag   = 0U; //FALSE
		smps_out_voltage  = 0x03;
	}
	
	/* Set HSI Calibration Trimming value */
	LL_RCC_HSI_SetCalibTrimming(hsi_calib);

	/* Low speed internal RC trimming value set by software */
	if (eng_lsi_bw_flag)
		LL_RCC_LSI_SetTrimming(lsi_bw);
	
	/* Set LSI LPMU Trimming value */
	LL_PWR_SetLSILPMUTrim(lsi_lpmu);
		
	/* Set Main Regulator voltage Trimming value */ 
	LL_PWR_SetMRTrim(main_regulator);

	/* Set SMPS output voltage Trimming value */
	LL_PWR_SetSMPSTrim(smps_out_voltage);
	
	/* Set SMPS in LP Open */
	LL_PWR_SetSMPSOpenMode(LL_PWR_SMPS_LPOPEN);	
	//END SmpsTrimConfig() function
	
	// BEGIN LSConfig()
	// Low speed crystal configuration 
	
	LL_PWR_SetNoPullB(LL_PWR_PUPD_IO12|LL_PWR_PUPD_IO13);
	LL_RCC_LSCO_SetSource(LL_RCC_LSCO_CLKSOURCE_LSE);
	
	// Set LSE oscillator drive capability 
	LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_HIGH);
	
	LL_RCC_LSI_Disable();
	
	// Need to explicitly disable LSE to make LSERDY flag go to 0 even without POR, in case it was enabled. 
	LL_RCC_LSE_Disable();  
	SystemReadyWait(300, LL_RCC_LSE_IsReady, 0);

	LL_RCC_LSE_Enable();
	SystemReadyWait(300, LL_RCC_LSE_IsReady, 1);
	// END LSConfig()
	

	// Set current and capacitors for High Speed Crystal Oscillator
	LL_RCC_HSE_SetCapacitorTuning(32); //centrer crystal
	LL_RCC_HSE_SetCurrentControl(LL_RCC_HSE_CURRENTMAX_3); // LL_RCC_HSE_CURRENTMAX_3 : (RCC_RFSWHSECR_GMC_1| RCC_RFSWHSECR_GMC_0) : (0x2U << (4UL) | 0x1U << (4UL))


	// BEGIN SystemClockConfig()
	//SystemTimer_TimeoutConfig(32000000, 100, TRUE);
	LL_RCC_HSE_Enable();
	SystemReadyWait(100, LL_RCC_HSE_IsReady, 1);
	LL_RCC_SetRC64MPLLPrescaler(LL_RCC_RC64MPLL_DIV_4); // RCC 16Mhz : (0x4U) << (5UL)
	LL_RCC_RC64MPLL_Enable();
	if(SystemReadyWait(200, LL_RCC_RC64MPLL_IsReady, 1) == 0U);
	else { // DIRECT HSE configuration
		LL_RCC_SetRC64MPLLPrescaler(LL_RCC_RC64MPLL_DIV_2);
		LL_RCC_DIRECT_HSE_Enable();
		SystemCoreClock = 32000000;
	}
	
	// FLASH Wait State configuration
	if (SystemCoreClock != 64000000) {
		LL_FLASH_SetWaitStates(FLASH, LL_FLASH_WAIT_STATES_0);
	}
	// END SystemClockConfig()

	/* Set Systick to 1ms using system clock frequency */
	LL_InitTick(SystemCoreClock, 1000);

	// BEGIN Configure_GPIO()
	// LL_AHB_EnableClock(0x4UL);
	// LL_AHB_EnableClock(LL_AHB_PERIPH_GPIOB);
	// LL_APB0_EnableClock(0x100UL);
	// LL_APB1_EnableClock(0x400UL);

	// /* Configure IO in output */
	// LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_8, LL_GPIO_MODE_OUTPUT);
	// /* Reset value is LL_GPIO_OUTPUT_PUSHPULL */
	// LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_8, LL_GPIO_OUTPUT_PUSHPULL);
	// /* Reset value is LL_GPIO_SPEED_FREQ_LOW */
	// LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_8, LL_GPIO_SPEED_FREQ_LOW);
	// /* Reset value is LL_GPIO_PULL_NO */
	// LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_8, LL_GPIO_PULL_NO);
	// END Configure_GPIO()


	// BEGIN MX_GPIO_Init()
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* LED2 GPIO CLK ENABLE */
	LL_AHB_EnableClock(LL_AHB_PERIPH_GPIOB);

	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_8);
	GPIO_InitStruct.Pin = LL_GPIO_PIN_8;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);	//Le problème est ici
	// END MX_GPIO_Init()

	for (unsigned int i = 0; i < 4294967295; i++);
	LL_GPIO_TogglePin(GPIOB, LL_GPIO_PIN_8);
	for (unsigned int i = 0; i < 4294967295; i++);
	LL_GPIO_TogglePin(GPIOB, LL_GPIO_PIN_8);
	for (unsigned int i = 0; i < 320000000; i++);
	LL_GPIO_TogglePin(GPIOB, LL_GPIO_PIN_8);

	/*
	// Set systick clock to 1ms using system clock frequency
	//LL_Init1msTick(SystemCoreClock);

	//MX_GPIO_Init();

	//LL_GPIO_TogglePin((GPIO_TypeDef*)(((0x40000000U)+(0x08000000U))+(0x100000UL)), 0x100UL);
	//LL_AHB_PERIPH_ALL //Tout init

	// Sortir la HSE sur MCO
	
	
	//Avant de faire un UART, toggle un gpio dans une boucle infinie pour voir si mes inits clocks fonctionnent
	//Il faut qd même activer la clock du gpio et ensuite taper dans le registre en write 1, wait, write 0.
	LL_AHB_PERIPH_ALL
	Smps -> alimentation
	LSCOnfig -> choisir config RO
	SysclkCOnfig -> Mettre la sysclk en direct HSE. C'est la que je lance la HSE et que je peux voir sur mon crystal Q2 si j'ai ma clock 32MHz
	Radio clock config je m'en fou pour l'instant
	D'abord je fais toggle mon IO ici et qd ça marche j'essaie de le toggle dans le main à la place d'un printk
	Dans mon main, je met plus printk mais toogle IO
	
	Si ça marche, faire du debug
	Soit gdb breakpoint voir si on y va
	Soit flash depuis PC linux. Brancher board sur PC Windows, ouvrir keil sans projet. File -> attach to target et regarder les registres.
	Qd je fais du debug, pour rester dans la zone de code jusqu'à activation j'utilise des boucles d'attentes actives. Exemple : while(LL_PWR_IsSMPSReady()) pour pas que le code passe à la suite avant que SMPS activer.
	//init UART
	//MX_USART_UART_Init();
	*/
	
	return 0;
}

SYS_INIT(bluenrg_init, PRE_KERNEL_1, 0);
