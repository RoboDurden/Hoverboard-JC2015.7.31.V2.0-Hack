#include "gd32f10x.h"
#include "../Inc/config.h"

// LED defines
#define LED_GREEN GPIO_PIN_3 //JMA ok
#define LED_GREEN_PORT GPIOB //JMA ok
#define LED_ORANGE GPIO_PIN_15 //JMA ok
#define LED_ORANGE_PORT GPIOA //JMA ok
#define LED_RED GPIO_PIN_12 //JMA ok
#define LED_RED_PORT GPIOA //JMA ok

#define UPPER_LED_PIN GPIO_PIN_11		// robo according to "Hoverboard JC2015.7.31.pdf"
#define UPPER_LED_PORT GPIOA
//#define LOWER_LED_PIN GPIO_PIN_0
//#define LOWER_LED_PORT GPIOA

// Mosfet output
//#define MOSFET_OUT_PIN GPIO_PIN_13
//#define MOSFET_OUT_PORT GPIOC

// Brushless Control DC (BLDC) defines
// Channel G
#define RCU_TIMER_BLDC RCU_TIMER0
#define TIMER_BLDC TIMER0
#define TIMER_BLDC_CHANNEL_G TIMER_CH_2
#define TIMER_BLDC_GH_PIN GPIO_PIN_10
#define TIMER_BLDC_GH_PORT GPIOA
#define TIMER_BLDC_GL_PIN GPIO_PIN_15
#define TIMER_BLDC_GL_PORT GPIOB
// Channel B
#define TIMER_BLDC_CHANNEL_B TIMER_CH_1
#define TIMER_BLDC_BH_PIN GPIO_PIN_9
#define TIMER_BLDC_BH_PORT GPIOA
#define TIMER_BLDC_BL_PIN GPIO_PIN_14
#define TIMER_BLDC_BL_PORT GPIOB
// Channel Y
#define TIMER_BLDC_CHANNEL_Y TIMER_CH_0
#define TIMER_BLDC_YH_PIN GPIO_PIN_8
#define TIMER_BLDC_YH_PORT GPIOA
#define TIMER_BLDC_YL_PIN GPIO_PIN_13
#define TIMER_BLDC_YL_PORT GPIOB

// Timer BLDC short circuit emergency shutoff define
//#define TIMER_BLDC_EMERGENCY_SHUTDOWN_PIN GPIO_PIN_12
//#define TIMER_BLDC_EMERGENCY_SHUTDOWN_PORT GPIOB


#define HALL_A_PIN GPIO_PIN_1
#define HALL_B_PIN GPIO_PIN_0
#define HALL_C_PIN GPIO_PIN_2
#define HALL_A_PORT GPIOA //was GPIOB
#define HALL_B_PORT GPIOA //was GPIOF
#define HALL_C_PORT GPIOA //was GPIOC

// Usart master slave defines JMA OK
#define USART_MASTERSLAVE USART2 //was USART1
#define USART_MASTERSLAVE_TX_PIN GPIO_PIN_10 //was GPIO_PIN_2
#define USART_MASTERSLAVE_TX_PORT GPIOB //was GPIOA
#define USART_MASTERSLAVE_RX_PIN GPIO_PIN_11 //was GPIO_PIN_3
#define USART_MASTERSLAVE_RX_PORT GPIOB //was GPIOA

// ADC defines JMA OK
#define VBATT_PIN	GPIO_PIN_5//was GPIO_PIN_4
#define VBATT_PORT GPIOA //was GPIOA
#define VBATT_CHANNEL ADC_CHANNEL_5 //was ADC_CHANNEL_4

#define CURRENT_DC_PIN	GPIO_PIN_7 // robo according to "Hoverboard JC2015.7.31.pdf", was GPIO_PIN_6
#define CURRENT_DC_PORT GPIOA //was GPIOA
#define CURRENT_DC_CHANNEL ADC_CHANNEL_7 // robo according to "Hoverboard JC2015.7.31.pdf", was ADC_CHANNEL_6

// Self hold defines JMA OK
#define SELF_HOLD_PIN GPIO_PIN_4 //was GPIO_PIN_2
#define SELF_HOLD_PORT GPIOA //was GPIOB

// Button defines JMA OK
#define BUTTON_PIN GPIO_PIN_3//was GPIO_PIN_15
#define BUTTON_PORT GPIOA //was GPIOC

// Usart steer defines
#define USART_STEER_COM USART0
#define USART_STEER_COM_TX_PIN GPIO_PIN_6
#define USART_STEER_COM_TX_PORT GPIOB
#define USART_STEER_COM_RX_PIN GPIO_PIN_7
#define USART_STEER_COM_RX_PORT GPIOB

#ifdef MASTER
// Buzzer defines JMA OK
#define BUZZER_PIN GPIO_PIN_1 //was GPIO_PIN_10
#define BUZZER_PORT GPIOB //was GPIOB

// Charge state defines
#define CHARGE_STATE_PIN GPIO_PIN_0
#define CHARGE_STATE_PORT GPIOF
#endif

// Debug pin defines JMA OK External-
#define DEBUG_PIN GPIO_PIN_4
#define DEBUG_PORT GPIOB

// ADC value conversion defines
#define MOTOR_AMP_CONV_DC_AMP 0.201465201465  // 3,3V * 1/3 - 0,004Ohm * IL(ampere) = (ADC-Data/4095) *3,3V
//#define ADC_BATTERY_VOLT      0.024169921875 	// V_Batt to V_BattMeasure = factor 30: ( (ADC-Data/4095) *3,3V *30 )
#define ADC_BATTERY_VOLT     0.01205891378 	// V_Batt to V_BattMeasure = factor 14.96: ( (ADC-Data/4095) *3,3V *14.96 )

// Useful math function defines
#define ABS(a) (((a) < 0.0) ? -(a) : (a))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define MAX(x, high) (((x) > (high)) ? (high) : (x))
#define MAP(x, xMin, xMax, yMin, yMax) ((x - xMin) * (yMax - yMin) / (xMax - xMin) + yMin)

