/*
===============================================================================
 Name        : TPF_EDIII_Calculadora.c
 Authors     :
 Version     :
 Copyright   : $(copyright)
 Description :
===============================================================================
*/

#include "LPC17xx.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

#define MULVAL      2
#define DIVADDVAL   1
#define Ux_FIFO_EN  (1<<0)
#define Rx_FIFO_RST (1<<1)
#define Tx_FIFO_RST (1<<2)
#define DLAB_BIT    (1<<7)
#define ONE_STOP_BIT  (1<<0)
#define RDR (1<<0)  //Receiver Data Ready
#define THRE (1<<5) //Transmit Holding Register Empty
#define LINE_FEED   0x0A //LF, For Linux, MAC and Windows Terminals
#define CARRIAGE_RETURN 0x0D //CR, For Windows Terminals (CR+LF).
#define NO_PARITY_BIT (1<<1)
#define OUTPUT 		(uint32_t)	 1
#define INPUT  		(uint32_t)	 0
#define PORT_ZERO 	(uint8_t)  	 0
#define PORT_ONE 	(uint8_t)    1
#define PORT_TWO 	(uint8_t)    2
#define PORT_THREE 	(uint8_t) 	 3
#define PIN_6		((uint32_t)(1<<6))
#define PIN_7		((uint32_t)(1<<7))
#define PIN_29		((uint32_t)(1<<29))
#define PIN_0_to_4	((uint32_t)(15<<0))
#define RAISING_EDGE (uint32_t)	0
#define FALLING_EDGE (uint32_t)	1
#define MATCH_0		 (uint8_t)	0
#define MATCH_1		 (uint8_t)  1
#define MATCH_2		 (uint8_t)  2
#define MATCH_3		 (uint8_t)  3

void config_GPIO(void); // Configura el pulsador usado para actualizar la hora y las interrupciones
void config_timer0(void); // Configura el timer0 para matchear a los 2 minutos, en la interrupción se actualiza la hora.
void config_UART3(void); // Configura la UART3
char U3Read(void); // Verifica que el buffer de lectura esté vacío, lee lo que recibe de la UART3 y lo devuelve como char
void U3Write(char);// Verifica que el buffer de escritura esté vacío y escribe en la UART3 el char pasado como parámetro

int main(void) {

	SystemInit();
	config_GPIO();
	config_timer0(); // Configura timer0 para la hora en caso de que no se pulse P0.6 al cabo de 2 min
	config_UART3(); // Configura la UART3

	/* Este programa envia "Hello from LPC1769" repetidamente a través de la UART3, separado un envío de otro
	* a través del corte de línea (enter)
	*/
	char msg[] = { 'H','e','l','l','o',' ','f','r','o','m',' ','L','P','C','1','7','6','9','\0' };
	int count=0;

	while(1){
		while( msg[count]!='\0' )
		{
			U3Write(msg[count]);
			count++;
		}
		//Send NEW Line Character(s) i.e. "\n"
		U3Write(CARRIAGE_RETURN);
		U3Write(LINE_FEED); //Windows uses CR+LF for newline.
		count=0; // reset counter
		/*
		 *
		 * do{
		 * U3Write('Inserte la operacion a realizar. Suma: + _ Resta: - _ Multiplicacion: * _ Division: /');
		 *
		 * while(!(LPC_UART3->LSR & RDR)); // Espera a que se pulse el operando
		 * operador = LPC_UART3->RBR;
		 *
		 * U3Write('Inserte los dos operandos. Pulsando 'q' se sale de la calculadora.');
		 *
		 * while(!(LPC_UART3->LSR & RDR)); // Espera a que se ingrese el primer operando
		 * operando_1=LPC_UART3->RBR;
		 *
		 * while(!(LPC_UART3->LSR & RDR)); // Espera a que se ingrese el segundo operando
		 * operando_2=LPC_UART3->RBR;
		 *
		 * switch(operador){
		 * 		case '+':
		 * 			U3Write(operando_1+operando_2);
		 * 			break;
		 * 		case '-':
		 * 			U3Write(operando_1-operando_2);
		 * 			break;
		 * 		case '*':
		 * 			U3Write(operando_1*operando_2);
		 * 			break;
		 * 		case '/':
		 * 			U3Write(operando_1/operando_2);
		 * 			break;
		 * 		case 'q': // con la tecla q sale
		 * 			break;
		 * 	}
		 * }while(operador!='q');
		 *
		 * U3Write('Finalizado');
		 */
	}

	return 0;
}
void U3Write(char txData) // Verifica que el registro para enviar esté vacío y luego manda el char pasado como parámetro
{
	while(!(LPC_UART3->LSR & THRE)); //wait until THR is empty
	LPC_UART3->THR = txData;
	return;
}
char U3Read(void)
{
	while(!(LPC_UART3->LSR & RDR)); //wait until data arrives in Rx FIFO
	return LPC_UART3->RBR;
}

void config_GPIO(){

	/*** Configuracion del puerto P0.6 para que actualice la hora ***/

	PINSEL_CFG_Type pin_configuration;

	pin_configuration.Portnum	=   PINSEL_PORT_0;      // Seleccionar puerto 0
	pin_configuration.Funcnum	=   PINSEL_FUNC_0;	// default function (GPIO) con pull-up activado
	pin_configuration.Pinnum        =   PINSEL_PIN_6;	// Seleccionar pin 6
	pin_configuration.OpenDrain     =   PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&pin_configuration);
	GPIO_SetDir(PORT_ZERO, PIN_6, INPUT); // Seteo puerto P0.6 como entrada

	/*** Configuración de la interrupción por P0.6 ***/

    GPIO_IntCmd(PORT_ZERO, PIN_6, FALLING_EDGE); // Pin P0.6 para interrumpir por flanco descendente
    GPIO_ClearInt(PORT_ZERO, PIN_6); 	     // Limpio flag interrupcion
	NVIC_EnableIRQ(EINT3_IRQn); 		     // Habilito el vector de interrupcion por GPIO

	return;
}
void config_timer0(){
	/*
	 * Configuracion Timer 0 por match0.0 para cuando no se pulsa P0.6 y hay que actualizar la hora automáticamente.
	 *
	 */

	LPC_SC->PCLKSEL0 |= (1<<2); // Selecciono PCLK = CCLK/1 para el timer 0 (división por 1)

	TIM_TIMERCFG_Type	timer0_config; // estructura para configuracion del timer0
	TIM_MATCHCFG_Type	timer0_match;  // estructura para configuracion del match0.0

	timer0_config.PrescaleOption		= TIM_PRESCALE_USVAL; // Prescale in microsecond value
	timer0_config.PrescaleValue		= 100e6;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &timer0_config);

	timer0_match.MatchValue    		= 118859424; // Este valor de match, junto con el de PR, me genera una interrupcion por timer0 cada 120 [s] = 2 min.
	timer0_match.MatchChannel    		= 0; 	// Se elije MAT0.0
	timer0_match.IntOnMatch			= ENABLE;// Interrumpir en MAT0.0
	timer0_match.ResetOnMatch		= ENABLE;// Resetear la cuenta al coincidir con MAT0.0
	timer0_match.StopOnMatch		= DISABLE; // No parar el timer0 al coincidir con el MAT0.0
	timer0_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING; // No hacer nada con la salida externa al coincidir con el MAT0.0
	TIM_ConfigMatch(LPC_TIM0, &timer0_match); // Carga la configuracion del timer0 para operar en modo match0.0
	TIM_ResetCounter(LPC_TIM0); 		  // Reseteo el TC
	TIM_Cmd(LPC_TIM0, ENABLE); 		  // Habilitar (Start/Stop) dispositivo Timer0
	NVIC_EnableIRQ(TIMER0_IRQn);		  // Habilitar vector interrupción de timer0
	return;
}
void config_UART3(){ // Configura la UART3

	// Power
	// LPC_SC->PCONP|= (1<<3); // No hace falta habilitarlo porque desde el reset sale habilitado la UART3

	// Peripheral Clock for UART3
	LPC_SC->PCLKSEL1|= (1<<18); // PCLK=CLK/1 (division por 1)

	// Baud rate
	LPC_UART3->LCR|= DLAB_BIT | ONE_STOP_BIT | NO_PARITY_BIT; // 8-bit character length, Enable access to Divisor Latches, 1 Stop bit, no parity bit
	LPC_UART3->DLL = 178; // Valores del Divisor Latch para lograr un baud rate de 9600
	LPC_UART3->DLM = 1;

	/* Siguiendo el algoritmo propuesto en el manual (pag 324), se procede a calcular el baud rate.
	PCLK = 100Mhz
	BR=9600

	DLest = 651.0416667
	FRest = 1.5
	-> DLest = Int(PCLK/(16xBRxFRest)) = Int(434.0277778) = 434
	-> FRest = PCLK/(16xBRxDLest) = 1.500096006, como FRest está entre 1.1 y 1.9, prosigo
	-> DIVADDVAL = table(FRest) = table(1.500) = 1
	-> MULVAL = table(FRest) = table(1.500) = 2
	-> DLM = DLest [15:8] -> [00000000 00000000] = [DLM   DLL] = [00000001 10110010] -> DLM = 1 (0x01) & DLL = 178 (0xB2)
	-> DLL = DLest [7:0]
	*/

	// Enable UART FIFO and set MULVAL + DIVADDVAL
	LPC_UART3->FCR|= Ux_FIFO_EN | Rx_FIFO_RST | Tx_FIFO_RST; // Enable UART FIFO, RX FIFO Reset, TX FIFO Reset
	LPC_UART3->FDR|= (MULVAL<<4) | (DIVADDVAL<<0);// Set value for DIVADDVAL (1) & MULVAL (2) -> 00100001
	LPC_UART3->LCR&= ~(DLAB_BIT); // Disable access to Divisor Latches

	// Pins
	LPC_PINCON->PINSEL0 |= (1<<1) | (1<<3); // Use P0.0 for Transmitter output & P0.1 for Receveir input in UART3
	// Interrupts
	//LPC_UART3->IER |= ..; //Edit this if want you to use UART interrupts

	return;
}

void EINT3_IRQHandler(void){ // Muestra el valor de la hora de la PC si se pulsó P0.6 y resetea el contador del timer0.

	if(GPIO_GetIntStatus(PORT_ZERO, 6, FALLING_EDGE)){ // Interrumpió P0.6?

		// Implementar la obtención de la hora y mostrarla por display LCD y UART

	}
    TIM_ResetCounter(LPC_TIM0); // Reseteo el TC del timer0
	GPIO_ClearInt(PORT_ZERO, PIN_6); // Limpia bandera interrup. de P0.6
	return;
}
