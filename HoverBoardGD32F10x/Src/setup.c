/*
* This file is part of the hoverboard-firmware-hack-V2 project. The 
* firmware is used to hack the generation 2 board of the hoverboard.
* These new hoverboards have no mainboard anymore. They consist of 
* two Sensorboards which have their own BLDC-Bridge per Motor and an
* ARM Cortex-M3 processor GD32F130C8.
*
* Copyright (C) 2018 Florian Staeblein
* Copyright (C) 2018 Jakob Broemauer
* Copyright (C) 2018 Kai Liebich
* Copyright (C) 2018 Christoph Lehnert
*
* The program is based on the hoverboard project by Niklas Fauth. The 
* structure was tried to be as similar as possible, so that everyone 
* could find a better way through the code.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gd32f10x.h"
#include "../Inc/setup.h"
#include "../Inc/defines.h"
#include "../Inc/config.h"
#include "../Inc/it.h"

#define TIMEOUT_FREQ  1000

#define GPIO_OTYPE_PP GPIO_MODE_OUT_PP //JMA translation from F130 to F103

// timeout timer parameter structs
timer_parameter_struct timeoutTimer_paramter_struct;

// PWM timer Parameter structs
timer_parameter_struct timerBldc_paramter_struct;	
timer_break_parameter_struct timerBldc_break_parameter_struct;
timer_oc_parameter_struct timerBldc_oc_parameter_struct;

// DMA (USART) structs
dma_parameter_struct dma_init_struct_usart;
uint8_t usartMasterSlave_rx_buf[USART_MASTERSLAVE_RX_BUFFERSIZE];
uint8_t usartSteer_COM_rx_buf[USART_STEER_COM_RX_BUFFERSIZE];

// DMA (ADC) structs
dma_parameter_struct dma_init_struct_adc;
extern adc_buf_t adc_buffer;

//----------------------------------------------------------------------------
// Initializes the interrupts
//----------------------------------------------------------------------------
void Interrupt_init(void)
{
  // Set IRQ priority configuration
	nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
}

//----------------------------------------------------------------------------
// Initializes the watchdog
//----------------------------------------------------------------------------
ErrStatus Watchdog_init(void)
{
	// Check if the system has resumed from FWDGT reset
	if (RESET != rcu_flag_get(RCU_FLAG_FWDGTRST))
	{   
		// FWDGTRST flag set
		rcu_all_reset_flag_clear();
	}
	
	// Clock source is IRC40K (40 kHz)
	// Prescaler is 16
	// Reload value is 4096 (0x0FFF)
	// Watchdog fires after 1638.4 ms
	//if (fwdgt_config(0x0FFF, FWDGT_PSC_DIV16) != SUCCESS ||
	//	fwdgt_window_value_config(0x0FFF) != SUCCESS)
	//{
	//	return ERROR;
	//}
	//JMA F103 fwdgt_window_value_config is not available anymore, prescaler must always be included
	if (fwdgt_config(0x0FFF, FWDGT_PSC_DIV16) != SUCCESS)
	{
		return ERROR;
	}
	// Enable free watchdog timer
	fwdgt_enable();
	
	return SUCCESS;
}

//----------------------------------------------------------------------------
// Initializes the timeout timer
//----------------------------------------------------------------------------
void TimeoutTimer_init(void)
{
	// Enable timer clock
	rcu_periph_clock_enable(RCU_TIMER3); //JMA changed from TIMER13 to TIMER3
	
	// Initial deinitialize of the timer
	timer_deinit(TIMER3); //JMA changed from TIMER13 to TIMER3
	
	// Set up the basic parameter struct for the timer
	// Update event will be fired every 1ms
	timeoutTimer_paramter_struct.counterdirection 	= TIMER_COUNTER_UP;
	timeoutTimer_paramter_struct.prescaler 					= 0;
	timeoutTimer_paramter_struct.alignedmode 				= TIMER_COUNTER_CENTER_DOWN;
	timeoutTimer_paramter_struct.period							= 72000000 / 2 / TIMEOUT_FREQ;
	timeoutTimer_paramter_struct.clockdivision 			= TIMER_CKDIV_DIV1;
	timeoutTimer_paramter_struct.repetitioncounter 	= 0;
	timer_auto_reload_shadow_disable(TIMER3); //JMA changed from TIMER13 to TIMER3
	timer_init(TIMER3, &timeoutTimer_paramter_struct); //JMA changed from TIMER13 to TIMER3
	
	// Enable TIMER_INT_UP interrupt and set priority
	nvic_irq_enable(TIMER3_IRQn, 0, 0); //JMA changed from TIMER13 to TIMER3
	timer_interrupt_enable(TIMER3, TIMER_INT_UP); //JMA changed from TIMER13 to TIMER3
	
	// Enable timer
	timer_enable(TIMER3); //JMA changed from TIMER13 to TIMER3
}

//----------------------------------------------------------------------------
// Initializes the GPIOs
//----------------------------------------------------------------------------
void GPIO_init(void)
{
	// Enable all GPIO clocks
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);
	rcu_periph_clock_enable(RCU_GPIOC);
	rcu_periph_clock_enable(RCU_GPIOF);

	
	//JMA enable RCU_AF for alternate functions
	rcu_periph_clock_enable(RCU_AF);
	
	//JMA remapping needed to use PA15 for other than JTAG or SWD
	//gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP,  ENABLE); // JW: Dont remap PA15 since it is not used (yet).
	gpio_pin_remap_config(GPIO_USART0_REMAP, ENABLE); // JW: Remap USART0 to PB6 and PB7

	// Init green LED
	//JMA changed for GD32F103
	//Was for F130:
	//gpio_mode_set(LED_GREEN_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,LED_GREEN);	
	//gpio_output_options_set(LED_GREEN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, LED_GREEN);
	//for F103:
	gpio_init(LED_GREEN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LED_GREEN);
	
	// Init red LED
	//gpio_mode_set(LED_RED_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,LED_RED);	
	//gpio_output_options_set(LED_RED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, LED_RED);
	//for F103:
	gpio_init(LED_RED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LED_RED);	

	// Init orange LED
	//gpio_mode_set(LED_ORANGE_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,LED_ORANGE);	
	//gpio_output_options_set(LED_ORANGE_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, LED_ORANGE);
	//for F103:
	//gpio_init(LED_ORANGE_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LED_ORANGE); //JW: Skip PA15 for now.

	// Init UPPER/LOWER LED
	#if defined(UPPER_LED_PIN) && defined(UPPER_LED_PORT)
		gpio_init(UPPER_LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, UPPER_LED_PIN);
		//gpio_mode_set(UPPER_LED_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,UPPER_LED_PIN);	
		//gpio_output_options_set(UPPER_LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, UPPER_LED_PIN);
	#endif
	//gpio_mode_set(LOWER_LED_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,LOWER_LED_PIN);	
	//gpio_output_options_set(LOWER_LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, LOWER_LED_PIN);
	//for F103:
//	gpio_init(UPPER_LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, UPPER_LED_PIN);
//	gpio_init(LOWER_LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LOWER_LED_PIN);	

	// Init mosfet output
	//gpio_mode_set(MOSFET_OUT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, MOSFET_OUT_PIN);	
	//gpio_output_options_set(MOSFET_OUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, MOSFET_OUT_PIN);
	//for F103:
//	gpio_init(MOSFET_OUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, MOSFET_OUT_PIN);	

	// Init HAL input
	//gpio_mode_set(HALL_A_PORT , GPIO_MODE_INPUT, GPIO_PUPD_NONE, HALL_A_PIN);
	//gpio_mode_set(HALL_B_PORT , GPIO_MODE_INPUT, GPIO_PUPD_NONE, HALL_B_PIN);
	//gpio_mode_set(HALL_C_PORT , GPIO_MODE_INPUT, GPIO_PUPD_NONE, HALL_C_PIN);
	//JMA F103
	gpio_init(HALL_A_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, HALL_A_PIN);	
	gpio_init(HALL_B_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, HALL_B_PIN);
	gpio_init(HALL_C_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, HALL_C_PIN);


	// Init USART_MASTERSLAVE
	//JMA TODO: is different for F103
	//gpio_mode_set(USART_MASTERSLAVE_TX_PORT , GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_MASTERSLAVE_TX_PIN);	
	//gpio_mode_set(USART_MASTERSLAVE_RX_PORT , GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_MASTERSLAVE_RX_PIN);
	//gpio_output_options_set(USART_MASTERSLAVE_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_MASTERSLAVE_TX_PIN);
	//gpio_output_options_set(USART_MASTERSLAVE_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_MASTERSLAVE_RX_PIN);	
	//gpio_af_set(USART_MASTERSLAVE_TX_PORT, GPIO_AF_1, USART_MASTERSLAVE_TX_PIN);
	//gpio_af_set(USART_MASTERSLAVE_RX_PORT, GPIO_AF_1, USART_MASTERSLAVE_RX_PIN);

	// JW: Configure USART2 TX (PB10) and RX (PB11) pins
	gpio_init(USART_MASTERSLAVE_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART_MASTERSLAVE_TX_PIN); // JW:
	gpio_init(USART_MASTERSLAVE_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART_MASTERSLAVE_RX_PIN); // JW:
	
	// Init ADC pins
	//gpio_mode_set(VBATT_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, VBATT_PIN);
	//gpio_mode_set(CURRENT_DC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, CURRENT_DC_PIN);
	//gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_6);
	//gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
	//JMA F103
	gpio_init(VBATT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, VBATT_PIN);	
	gpio_init(CURRENT_DC_PORT, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, CURRENT_DC_PIN);
	gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, GPIO_PIN_6);
	gpio_init(GPIOB, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, GPIO_PIN_0);
	
	// Init debug pin
	//gpio_mode_set(DEBUG_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DEBUG_PIN);	
	//gpio_output_options_set(DEBUG_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_PIN);
	//JMA F103
	gpio_init(DEBUG_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_PIN);	





//------- deepseek begin
		// CORRECTED PWM OUTPUT CONFIGURATION:
    // Changed to alternate function push-pull with higher speed
    gpio_init(TIMER_BLDC_GH_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_GH_PIN);    
    gpio_init(TIMER_BLDC_BH_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_BH_PIN);
    gpio_init(TIMER_BLDC_YH_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_YH_PIN);
    gpio_init(TIMER_BLDC_GL_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_GL_PIN);    
    gpio_init(TIMER_BLDC_BL_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_BL_PIN);
    gpio_init(TIMER_BLDC_YL_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, TIMER_BLDC_YL_PIN);
/*

	// Init emergency shutdown pin JMA TODO No such pin
	//gpio_mode_set(TIMER_BLDC_EMERGENCY_SHUTDOWN_PORT , GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_EMERGENCY_SHUTDOWN_PIN);
	//gpio_af_set(TIMER_BLDC_EMERGENCY_SHUTDOWN_PORT, GPIO_AF_2, TIMER_BLDC_EMERGENCY_SHUTDOWN_PIN);
	
	// Init PWM output Pins (Configure as alternate functions, push-pull, no pullup)
  //gpio_mode_set(TIMER_BLDC_GH_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_GH_PIN);
	//gpio_mode_set(TIMER_BLDC_BH_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_BH_PIN);
	//gpio_mode_set(TIMER_BLDC_YH_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_YH_PIN);
	//gpio_mode_set(TIMER_BLDC_GL_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_GL_PIN);
	//gpio_mode_set(TIMER_BLDC_BL_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_BL_PIN);
	//gpio_mode_set(TIMER_BLDC_YL_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TIMER_BLDC_YL_PIN);
	
  //gpio_output_options_set(TIMER_BLDC_GH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_GH_PIN);
  //gpio_output_options_set(TIMER_BLDC_BH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_BH_PIN);
  //gpio_output_options_set(TIMER_BLDC_YH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_YH_PIN);
	//gpio_output_options_set(TIMER_BLDC_GL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_GL_PIN);
  //gpio_output_options_set(TIMER_BLDC_BL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_BL_PIN);
  //gpio_output_options_set(TIMER_BLDC_YL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_YL_PIN);
	//JMA F103
	gpio_init(TIMER_BLDC_GH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_GH_PIN);	
	gpio_init(TIMER_BLDC_BH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_BH_PIN);
	gpio_init(TIMER_BLDC_YH_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_YH_PIN);
	gpio_init(TIMER_BLDC_GL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_GL_PIN);	
	gpio_init(TIMER_BLDC_BL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_BL_PIN);
	gpio_init(TIMER_BLDC_YL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, TIMER_BLDC_YL_PIN);

  //gpio_af_set(TIMER_BLDC_GH_PORT, GPIO_AF_2, TIMER_BLDC_GH_PIN);
  //gpio_af_set(TIMER_BLDC_BH_PORT, GPIO_AF_2, TIMER_BLDC_BH_PIN);
	//gpio_af_set(TIMER_BLDC_YH_PORT, GPIO_AF_2, TIMER_BLDC_YH_PIN);
	//gpio_af_set(TIMER_BLDC_GL_PORT, GPIO_AF_2, TIMER_BLDC_GL_PIN);
  //gpio_af_set(TIMER_BLDC_BL_PORT, GPIO_AF_2, TIMER_BLDC_BL_PIN);
	//gpio_af_set(TIMER_BLDC_YL_PORT, GPIO_AF_2, TIMER_BLDC_YL_PIN);
------- deepseek end	*/

	
	//JMA for F103 no remapping needed fro TIMER0
	//gpio_pin_remap_config(uint32_t remap, ControlStatus newvalue); //JMA for F103

	
	// Init self hold
	//gpio_mode_set(SELF_HOLD_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SELF_HOLD_PIN);	
	//gpio_output_options_set(SELF_HOLD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, SELF_HOLD_PIN);
	//JMA F103
	gpio_init(SELF_HOLD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, SELF_HOLD_PIN);

	// Init USART0
	//gpio_mode_set(USART_STEER_COM_TX_PORT , GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_STEER_COM_TX_PIN);	
	//gpio_mode_set(USART_STEER_COM_RX_PORT , GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_STEER_COM_RX_PIN);
	//gpio_output_options_set(USART_STEER_COM_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_STEER_COM_TX_PIN);
	//gpio_output_options_set(USART_STEER_COM_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_STEER_COM_RX_PIN);	
	//JMA F103
	gpio_init(USART_STEER_COM_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART_STEER_COM_TX_PIN);       // JW: Configure PB6 as USART0 TX (Alternate Function Push-Pull)
	gpio_init(USART_STEER_COM_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART_STEER_COM_RX_PIN); // JW: Configure PB7 as USART0 RX (Input Floating)
	//JMA F103 TODO check if AF needed for USART GPIOB PIN6 and 7
	//gpio_af_set(USART_STEER_COM_TX_PORT, GPIO_AF_0, USART_STEER_COM_TX_PIN);
	//gpio_af_set(USART_STEER_COM_RX_PORT, GPIO_AF_0, USART_STEER_COM_RX_PIN);
	
#ifdef MASTER	
	// Init buzzer
	//gpio_mode_set(BUZZER_PORT , GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BUZZER_PIN);	
	//gpio_output_options_set(BUZZER_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BUZZER_PIN);
	//JMA F103
	gpio_init(BUZZER_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BUZZER_PIN);
	
	// Init button
	//gpio_mode_set(BUTTON_PORT , GPIO_MODE_INPUT, GPIO_PUPD_NONE, BUTTON_PIN);
	//JMA F103
	gpio_init(BUTTON_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, BUTTON_PIN);	
	
	// Init charge state
	//gpio_mode_set(CHARGE_STATE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, CHARGE_STATE_PIN);
	//JMA F103
	gpio_init(CHARGE_STATE_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, CHARGE_STATE_PIN);
#endif
}
	
//----------------------------------------------------------------------------
// Initializes the PWM
//----------------------------------------------------------------------------
void PWM_init(void)
{
	// Enable timer clock
	rcu_periph_clock_enable(RCU_TIMER_BLDC);
	
	// Initial deinitialize of the timer
	timer_deinit(TIMER_BLDC);
	
	// Set up the basic parameter struct for the timer
	timerBldc_paramter_struct.counterdirection 	= TIMER_COUNTER_UP;
	timerBldc_paramter_struct.prescaler 				= 0;
	timerBldc_paramter_struct.alignedmode 			= TIMER_COUNTER_CENTER_DOWN;
	timerBldc_paramter_struct.period						= 72000000 / 2 / PWM_FREQ;
	timerBldc_paramter_struct.clockdivision 		= TIMER_CKDIV_DIV1;
	timerBldc_paramter_struct.repetitioncounter = 0;
	timer_auto_reload_shadow_disable(TIMER_BLDC);
	
	// Initialize timer with basic parameter struct
	timer_init(TIMER_BLDC, &timerBldc_paramter_struct);

	// Deactivate output channel fastmode
	timer_channel_output_fast_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, TIMER_OC_FAST_DISABLE);
	timer_channel_output_fast_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, TIMER_OC_FAST_DISABLE);
	timer_channel_output_fast_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, TIMER_OC_FAST_DISABLE);
	
	// Deactivate output channel shadow function
	timer_channel_output_shadow_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, TIMER_OC_SHADOW_DISABLE);
	timer_channel_output_shadow_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, TIMER_OC_SHADOW_DISABLE);
	timer_channel_output_shadow_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, TIMER_OC_SHADOW_DISABLE);
	
	// Set output channel PWM type to PWM1
	timer_channel_output_mode_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, TIMER_OC_MODE_PWM1);
	timer_channel_output_mode_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, TIMER_OC_MODE_PWM1);
	timer_channel_output_mode_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, TIMER_OC_MODE_PWM1);

	// Initialize pulse length with value 0 (pulse duty factor = zero)
	timer_channel_output_pulse_value_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, 0);
	timer_channel_output_pulse_value_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, 0);
	timer_channel_output_pulse_value_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, 0);
	
	// Set up the output channel parameter struct
	timerBldc_oc_parameter_struct.ocpolarity 		= TIMER_OC_POLARITY_HIGH;
	timerBldc_oc_parameter_struct.ocnpolarity 	= TIMER_OCN_POLARITY_LOW;
	timerBldc_oc_parameter_struct.ocidlestate 	= TIMER_OC_IDLE_STATE_LOW;
	timerBldc_oc_parameter_struct.ocnidlestate 	= TIMER_OCN_IDLE_STATE_HIGH;
	
	// Configure all three output channels with the output channel parameter struct
	timer_channel_output_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, &timerBldc_oc_parameter_struct);
  timer_channel_output_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, &timerBldc_oc_parameter_struct);
	timer_channel_output_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, &timerBldc_oc_parameter_struct);

	// Set up the break parameter struct
	timerBldc_break_parameter_struct.runoffstate			= TIMER_ROS_STATE_ENABLE;
	timerBldc_break_parameter_struct.ideloffstate 		= TIMER_IOS_STATE_DISABLE;
	timerBldc_break_parameter_struct.protectmode			= TIMER_CCHP_PROT_OFF;
	timerBldc_break_parameter_struct.deadtime 				= DEAD_TIME;
	timerBldc_break_parameter_struct.breakstate				= TIMER_BREAK_ENABLE;
	timerBldc_break_parameter_struct.breakpolarity		= TIMER_BREAK_POLARITY_LOW;
	timerBldc_break_parameter_struct.outputautostate 	= TIMER_OUTAUTO_ENABLE;
	
	// Configure the timer with the break parameter struct
	timer_break_config(TIMER_BLDC, &timerBldc_break_parameter_struct);

	// Disable until all channels are set for PWM output
	timer_disable(TIMER_BLDC);

	// Enable all three channels for PWM output
	timer_channel_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, TIMER_CCX_ENABLE);
	timer_channel_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, TIMER_CCX_ENABLE);
	timer_channel_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, TIMER_CCX_ENABLE);

	// Enable all three complemenary channels for PWM output
	timer_channel_complementary_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_G, TIMER_CCXN_ENABLE);
	timer_channel_complementary_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_B, TIMER_CCXN_ENABLE);
	timer_channel_complementary_output_state_config(TIMER_BLDC, TIMER_BLDC_CHANNEL_Y, TIMER_CCXN_ENABLE);
	
	// Enable TIMER_INT_UP interrupt and set priority
	//nvic_irq_enable(TIMER0_BRK_UP_TRG_COM_IRQn, 0, 0);
	//timer_interrupt_enable(TIMER_BLDC, TIMER_INT_UP);
	//JMA F103
	nvic_irq_enable(TIMER0_UP_IRQn, 0, 0);
	timer_interrupt_enable(TIMER_BLDC, TIMER_INT_UP);	
	// Enable the timer and start PWM
	timer_enable(TIMER_BLDC);
}

//----------------------------------------------------------------------------
// Initializes the ADC for polling use
//----------------------------------------------------------------------------
// JW: Old deprecated? Not used, commented out in main...
void ADC_init_Poll(void)
{
	// Enable ADC clock only
	//rcu_periph_clock_enable(RCU_ADC);
	//rcu_periph_clock_enable(RCU_DMA);
	//JMA F103
	rcu_periph_clock_enable(RCU_ADC0);

  // Configure ADC clock (APB2 clock is DIV1 -> 72MHz, ADC clock is DIV6 -> 12MHz)
	//rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);
	//JMA F103
	rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);
	

	//Configure ADC
	adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1); //use only 1 channel JMA ADC0 added
	adc_regular_channel_config(ADC0, 0, VBATT_CHANNEL, ADC_SAMPLETIME_13POINT5); //JMA ADC0 added
//	adc_regular_channel_config(ADC0, 1, CURRENT_DC_CHANNEL, ADC_SAMPLETIME_13POINT5); //JMA ADC0 added
	adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT); //JMA ADC0 added
	
	// Set trigger of ADC
	adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE); //JMA ADC0 added
	adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE); //JMA ADC) added and was ADC_EXTTRIG_REGULAR_SWRCST
	
	// Disable the temperature sensor, Vrefint and vbat channel
	adc_tempsensor_vrefint_disable();
	
	// ADC analog watchdog disable
	adc_watchdog_disable(ADC0); //JMA ADC0 added
	
	// Enable ADC (must be before calibration)
	adc_enable(ADC0); //JMA ADC0 added
	
	// Calibrate ADC values
	adc_calibration_enable(ADC0); //JMA ADC0 added

    
	// Do not Set ADC to scan mode
	adc_special_function_config(ADC0, ADC_SCAN_MODE, DISABLE); //JMA ADC0 added
}

//----------------------------------------------------------------------------
// Initializes the ADC
//----------------------------------------------------------------------------
void ADC_init(void)
{
	// Enable ADC and DMA clock
	//rcu_periph_clock_enable(RCU_ADC);
	//rcu_periph_clock_enable(RCU_DMA);
	//JMA F103
	rcu_periph_clock_enable(RCU_ADC0);
	rcu_periph_clock_enable(RCU_DMA0);	

  // Configure ADC clock (APB2 clock is DIV1 -> 72MHz, ADC clock is DIV6 -> 12MHz)
	//rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);
	//JMA F103
	rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);

	// Interrupt channel 0 enable
	nvic_irq_enable(DMA0_Channel0_IRQn, 1, 0); //JMA was DMA_Channel0_IRQn
	
	// Initialize DMA channel 0 for ADC
	dma_deinit(DMA0, DMA_CH0); //JMA DMA0 added
	dma_init_struct_adc.direction = DMA_PERIPHERAL_TO_MEMORY;
	dma_init_struct_adc.memory_addr = (uint32_t)&adc_buffer;
	dma_init_struct_adc.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct_adc.memory_width = DMA_MEMORY_WIDTH_16BIT;
	dma_init_struct_adc.number = 2; // JW: was 1! JMA was 2
	dma_init_struct_adc.periph_addr = (uint32_t)&ADC_RDATA(ADC0); //JMA (ADC0) added. was &ADC_RDATA
	dma_init_struct_adc.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct_adc.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
	dma_init_struct_adc.priority = DMA_PRIORITY_ULTRA_HIGH;
	dma_init(DMA0, DMA_CH0, &dma_init_struct_adc); //JMA DMA0 added; "&" added before dma_init_struct_adc
	
	// Configure DMA mode
	dma_circulation_enable(DMA0, DMA_CH0); //JMA DMA0 added
	dma_memory_to_memory_disable(DMA0, DMA_CH0); //JMA DMA0 added
	
	// Enable DMA transfer complete interrupt
	dma_interrupt_enable(DMA0, DMA_CH0, DMA_CHXCTL_FTFIE); //JMA DMA0 added
	
	// At least clear number of remaining data to be transferred by the DMA 
	dma_transfer_number_config(DMA0, DMA_CH0, 2); // JW: was 1! was 2 JMA DMA0 added
	
	// Enable DMA channel 0
	dma_channel_enable(DMA0, DMA_CH0); //JMA DMA0 added
	
	adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1); //was 2 JMA ADC0 added
	adc_regular_channel_config(ADC0, 0, VBATT_CHANNEL, ADC_SAMPLETIME_13POINT5); //JMA ADC0 added
	adc_regular_channel_config(ADC0, 1, CURRENT_DC_CHANNEL, ADC_SAMPLETIME_13POINT5); // JW: uncommented! JMA ADC0 added
	adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT); //JMA ADC0 added
	
	// Set trigger of ADC
	adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE); //JMA ADC0 added
	adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE); //JMA ADC) added and was ADC_EXTTRIG_REGULAR_SWRCST
	
	// Disable the temperature sensor, Vrefint and vbat channel
	adc_tempsensor_vrefint_disable();
	//adc_vbat_disable(); JMA VBAT channel not present in F103
	
	// ADC analog watchdog disable
	adc_watchdog_disable(ADC0); //JMA ADC0 added
	
	// Enable ADC (must be before calibration)
	adc_enable(ADC0); //JMA ADC0 added
	
	// Calibrate ADC values
	adc_calibration_enable(ADC0); //JMA ADC0 added
	
	// Enable DMA request
	adc_dma_mode_enable(ADC0); //JMA ADC0 added
    
	// Set ADC to scan mode
	adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE); //JMA ADC0 added
	//adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE); // JW: added: to trigger adc and interupt continously.
}

//----------------------------------------------------------------------------
// Initializes the usart master slave
//----------------------------------------------------------------------------
void USART_MasterSlave_init(void)
{
	// Enable ADC and DMA clock
	rcu_periph_clock_enable(RCU_USART2); // JW: was RCU_USART1
	rcu_periph_clock_enable(RCU_DMA0); //JMA was RCU_DMA
	
	// Reset USART
	usart_deinit(USART_MASTERSLAVE); // JW: added
	
	// Init USART for 115200 baud, 8N1
	usart_baudrate_set(USART_MASTERSLAVE, 115200);
	usart_parity_config(USART_MASTERSLAVE, USART_PM_NONE);
	usart_word_length_set(USART_MASTERSLAVE, USART_WL_8BIT);
	usart_stop_bit_set(USART_MASTERSLAVE, USART_STB_1BIT);
	usart_hardware_flow_rts_config(USART_MASTERSLAVE, USART_RTS_DISABLE);  // JW: Disable RTS
	usart_hardware_flow_cts_config(USART_MASTERSLAVE, USART_CTS_DISABLE);  // JW: Disable CTS
	//JMA no oversampling in F103 usart_oversample_config(USART_MASTERSLAVE, USART_OVSMOD_16);
	
	// Enable both transmitter and receiver
	usart_transmit_config(USART_MASTERSLAVE, USART_TRANSMIT_ENABLE);
	usart_receive_config(USART_MASTERSLAVE, USART_RECEIVE_ENABLE);
	
	// Enable USART
	usart_enable(USART_MASTERSLAVE);
	
	// Interrupt channel 3/4 enable
	//nvic_irq_enable(DMA_Channel3_4_IRQn, 2, 0);
	//JMA F103 cannel 3 and 4 are separate. Only channel 4 is used so only channel 4 interrupt enabled
	nvic_irq_enable(DMA0_Channel2_IRQn, 2, 0); // JW: Changed to Channel2 (from Channel4)

// Initialize DMA channel 4 for USART_SLAVE RX
	dma_deinit(DMA0, DMA_CH2); // JW: Changed to CH2 (from CH4). JMA DMA0 added
	dma_init_struct_usart.direction = DMA_PERIPHERAL_TO_MEMORY;
	dma_init_struct_usart.memory_addr = (uint32_t)usartMasterSlave_rx_buf;
	dma_init_struct_usart.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct_usart.memory_width = DMA_MEMORY_WIDTH_8BIT;
	dma_init_struct_usart.number = USART_MASTERSLAVE_RX_BUFFERSIZE;
	dma_init_struct_usart.periph_addr = (uint32_t)&USART_DATA(USART_MASTERSLAVE); // JW: USART_MASTERSLAVE_DATA_RX_ADDRESS;
	dma_init_struct_usart.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct_usart.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	dma_init_struct_usart.priority = DMA_PRIORITY_ULTRA_HIGH;
	dma_init(DMA0, DMA_CH2, &dma_init_struct_usart); // JW: Changed to CH2 (from CH4). JMA DMA0 added & added before dma_init_struct_usart
	
	// Configure DMA mode
	dma_circulation_enable(DMA0, DMA_CH2); // JW: Changed to CH2 (from CH4). JMA DMA0 added
	dma_memory_to_memory_disable(DMA0, DMA_CH2); // JW: Changed to CH2 (from CH4). JMA DMA0 added

	// USART DMA enable for transmission and receive
	usart_dma_receive_config(USART_MASTERSLAVE, USART_DENR_ENABLE);
	
	// Enable DMA transfer complete interrupt
	dma_interrupt_enable(DMA0, DMA_CH2, DMA_CHXCTL_FTFIE); // JW: Changed to CH2 (from CH4). JMA DMA0 added
	
	// At least clear number of remaining data to be transferred by the DMA 
	dma_transfer_number_config(DMA0, DMA_CH2, 1); // JW: Changed to CH2 (from CH4). JMA DMA0 added
	
	// Enable dma receive channel
	dma_channel_enable(DMA0, DMA_CH2); // JW: Changed to CH2 (from CH4). JMA DMA0 added
}

//----------------------------------------------------------------------------
// Initializes the usart steer/bluetooth
//----------------------------------------------------------------------------
void USART_Steer_COM_init(void)
{
		// Enable ADC and DMA clock
	rcu_periph_clock_enable(RCU_USART0);
	rcu_periph_clock_enable(RCU_DMA0); //JMA was RCU_DMA
	
	// Reset USART
	usart_deinit(USART_STEER_COM); // JW: added

	// Init USART for 19200 baud, 8N1
	usart_baudrate_set(USART_STEER_COM, 19200);
	usart_parity_config(USART_STEER_COM, USART_PM_NONE);
	usart_word_length_set(USART_STEER_COM, USART_WL_8BIT);
	usart_stop_bit_set(USART_STEER_COM, USART_STB_1BIT);
	usart_hardware_flow_rts_config(USART_STEER_COM, USART_RTS_DISABLE);  // JW: Disable RTS
	usart_hardware_flow_cts_config(USART_STEER_COM, USART_CTS_DISABLE);  // JW: Disable CTS
	//JMA no oversampling in F103 usart_oversample_config(USART_STEER_COM, USART_OVSMOD_16);
	
	// Enable both transmitter and receiver
	usart_transmit_config(USART_STEER_COM, USART_TRANSMIT_ENABLE);
	usart_receive_config(USART_STEER_COM, USART_RECEIVE_ENABLE);
	
	// Enable USART
	usart_enable(USART_STEER_COM);
	
	// Interrupt channel 1/2 enable
	//nvic_irq_enable(DMA_Channel1_2_IRQn, 2, 0);
	//JMA F103 cannel 3 and 4 are separate. Only channel 2 is used so only channel 2 interrupt enabled
	nvic_irq_enable(DMA0_Channel4_IRQn, 2, 0); // JW: Changed to Channel4 (from Channel2)
	// Initialize DMA channel 2 for USART_STEER_COM RX
	dma_deinit(DMA0, DMA_CH4); // JW: Changed to CH4 (from CH2). JMA DMA0 added
	dma_init_struct_usart.direction = DMA_PERIPHERAL_TO_MEMORY;
	dma_init_struct_usart.memory_addr = (uint32_t)usartSteer_COM_rx_buf;
	dma_init_struct_usart.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct_usart.memory_width = DMA_MEMORY_WIDTH_8BIT;
	dma_init_struct_usart.number = USART_STEER_COM_RX_BUFFERSIZE;
	dma_init_struct_usart.periph_addr = (uint32_t)&USART_DATA(USART_STEER_COM); // JW: USART_STEER_COM_DATA_RX_ADDRESS;
	dma_init_struct_usart.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct_usart.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	dma_init_struct_usart.priority = DMA_PRIORITY_ULTRA_HIGH;
	dma_init(DMA0, DMA_CH4, &dma_init_struct_usart); // JW: Changed to CH4 (from CH2). JMA DMA0 added "&" inserted
	
	// Configure DMA mode
	dma_circulation_enable(DMA0, DMA_CH4); // JW: Changed to CH4 (from CH2). JMA DMA0 added
	dma_memory_to_memory_disable(DMA0, DMA_CH4); // JW: Changed to CH4 (from CH2). JMA DMA0 added

	// USART DMA enable for transmission and receive
	usart_dma_receive_config(USART_STEER_COM, USART_DENR_ENABLE);
	
	// Enable DMA transfer complete interrupt
	dma_interrupt_enable(DMA0, DMA_CH4, DMA_CHXCTL_FTFIE); // JW: Changed to CH4 (from CH2). JMA DMA0 added
	
	// At least clear number of remaining data to be transferred by the DMA 
	dma_transfer_number_config(DMA0, DMA_CH4, 1); // JW: Changed to CH4 (from CH2). JMA DMA0 added
	
	// Enable dma receive channel
	dma_channel_enable(DMA0, DMA_CH4); // JW: Changed to CH4 (from CH2). JMA DMA0 added
}
