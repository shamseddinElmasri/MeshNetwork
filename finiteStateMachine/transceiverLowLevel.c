#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "main.h"

#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"
#include "Transceiver.h"



/*
 * Function:	spi_init()
 * Description:	Initializes SPI and GPIO							
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void spi_init(void){

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	HAL_StatusTypeDef rc;

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_SPI2_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, CSN|CE, GPIO_PIN_RESET);

	/*Configure GPIO pin : PC7 */
	GPIO_InitStruct.Pin = IRQ;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  	/* EXTI interrupt Init*/
  	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  	
	/*Configure GPIO pins : PB8 and PB9 */
	GPIO_InitStruct.Pin = CSN|CE;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	//HAL_SPI_MspInit(&hspi2);                      
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 7;
	hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

	 /**SPI2 GPIO Configuration    
	PB13     ------> SPI2_SCK
	PB14     ------> SPI2_MISO
	PB15     ------> SPI2_MOSI
	*/
	GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	rc = HAL_SPI_Init(&hspi2);
	if(rc != HAL_OK){   
		printf("SPI initialization failed with rc=%u\n",rc);
	}
}



/*
 * Function:	transceiverInit()
 * Description:	Initializes the tranceiver and sets it in receiver mode
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void transceiverInit(void){

	CE_LOW();					// Set chip enable bit low

	HAL_Delay(100);					// Power on reset transition state
	
	hal_nrf_set_power_mode(HAL_NRF_PWR_UP);		// Power up module

	HAL_Delay(5);					// Oscillator startup delay
	
	hal_nrf_set_irq_mode(HAL_NRF_RX_DR, true);	// Enable "Data Ready" interrupt
	hal_nrf_set_irq_mode(HAL_NRF_TX_DS, false);	// Disable "Data Sent" interrupt
	hal_nrf_set_irq_mode(HAL_NRF_MAX_RT, false);	// Disable "Max Number of Retransmits" interrupt
	
	hal_nrf_set_crc_mode(HAL_NRF_CRC_8BIT);		// Enable CRC mode

	hal_nrf_set_operation_mode(HAL_NRF_PRX);	// Set module in receiver mode

	hal_nrf_close_pipe(HAL_NRF_ALL);		// Disable Enhanced mode

	hal_nrf_open_pipe(HAL_NRF_PIPE0, false);	// Open pipe0 without auto Ack

	hal_nrf_set_auto_retr(0, 0);			// Disable Automatic Retransmission

	hal_nrf_disable_dynamic_pl();			// Disable dynamic payload length feature

	hal_nrf_set_rx_pload_width(0, 32);		// Set payload width

	setDataRate(DATA_RATE_2MBPS);			// Set RF data rate
	
 	hal_nrf_set_output_power(HAL_NRF_18DBM);	// Set RF power
	
	hal_nrf_set_address_width(HAL_NRF_AW_5BYTES);	// Set address width

	hal_nrf_set_rf_channel(16);			// Set RF channel

	uint8_t pipe0Addr[] = {0x61, 0x37, 0x71, 0xF4, 0x94}; 	// Address of pipe 0
	hal_nrf_set_address(HAL_NRF_PIPE0, pipe0Addr);		// Set Rx address

	uint8_t txAddr[] = {0x61, 0x37, 0x71, 0xF4, 0x94};	// Address of destination
	hal_nrf_set_address(HAL_NRF_TX, txAddr);		// Set Tx address

	hal_nrf_flush_rx();					// Flush Rx FIFO

	hal_nrf_flush_tx();					// Flush TX FIFO

	hal_nrf_get_clear_irq_flags();				// Clear all interrupt flags

	CE_HIGH();						// Set chip enable bit high
}



/*
 * Function:	timer2Init()
 * Description:	Initializes timer 2 with 1 microsecond resolution
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void timer2Init(void){
  
	__HAL_RCC_TIM2_CLK_ENABLE();
  
	tim2.Instance = TIM2;
	tim2.Init.Prescaler     = (HAL_RCC_GetPCLK2Freq() / 1000000 - 1);
	tim2.Init.CounterMode   = TIM_COUNTERMODE_UP;
	tim2.Init.Period        = 0xffff;
	tim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim2.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&tim2);  
}




/*
 * Function:	timer17Init()
 * Description:	Initializes timer17 with 1 millisecond resolution, and enables the overflow interrupt
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void timer17Init(void){

	__HAL_RCC_TIM17_CLK_ENABLE();
	tim17.Instance = TIM17;
	tim17.Init.Prescaler = HAL_RCC_GetPCLK2Freq()/10000 - 1;
	tim17.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim17.Init.Period = 50000;
	tim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim17.Init.RepetitionCounter = 0;
	TIM17 -> CR1 |= (TIM_CR1_URS);			// Only counter over/under flow generates interrupt
	TIM17 -> DIER |= 0b1;				// Set Update Interrupt Enable bit
	TIM17 -> EGR |= 0b1;				// Set Update Generation bit
	HAL_TIM_Base_Init(&tim17);			//Enable Interrupts
	HAL_NVIC_SetPriority(TIM17_IRQn,0,1);		// To be adjusted!!!!
	NVIC_EnableIRQ(TIM17_IRQn);
  
	HAL_TIM_Base_Start(&tim17);			// Start timer
}



/*
 * Function:	microDelay()
 * Description:	Generates a delay in microsenconds using timer 2
 * Parameters: 	
 * 		uint32_t delay: Desired delay in microseconds
 * Returns:	Nothing
 */
void microDelay(uint32_t delay){

	HAL_TIM_Base_Start(&tim2);		// Start timer
	if(delay <= 65535){
		
		TIM2->CNT = 0;       		// Reset counter
		
		while(TIM2->CNT < delay) {
			asm volatile ("nop\n");
		}
    	}
	else{
		printf("input value for delay should be between 0 and 65536\n");
	}
	
	HAL_TIM_Base_Stop(&tim2);		// Stop timer
}



/*
 * Function:	spiTransaction()
 * Description:	Performs a SPI transaction with customized number of bytes
 *		used for reading all transceiver registers for monitoring and troubleshooting
 * Parameters: 	
 * 		uint8_t *tx: Points to byte to be transmitted
 *	 	uint8_t *rx: Points to received byte
 *		uint8_t nOfBytes: Number of bytes
 *
 * Returns:	Nothing
 */
void spiTransaction(uint8_t *tx, uint8_t *rx, uint8_t nOfBytes){

	CSN_LOW();
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, nOfBytes, 100);
	CSN_HIGH();
}




/*
 * Function:	CSN_LOW()
 * Description:	Sets chip select pin low to start SPI transaction
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void CSN_LOW(void){

	HAL_GPIO_WritePin(GPIOB, CSN, GPIO_PIN_RESET);
}



/*
 * Function:	CSN_HIGH()
 * Description:	Sets chip select pin high to end SPI transaction
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void CSN_HIGH(void){

	HAL_GPIO_WritePin(GPIOB, CSN, GPIO_PIN_SET);
}



/*
 * Function:	CE_LOW()
 * Description:	Sets chip enable pin low to activate transceiver module 
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void CE_LOW(void){

	HAL_GPIO_WritePin(GPIOB, CE, GPIO_PIN_RESET);
}



/*
 * Function:	CE_HIGH()
 * Description:	Sets chip enable pin low to deactivate transceiver module 
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void CE_HIGH(void){

	HAL_GPIO_WritePin(GPIOB, CE, GPIO_PIN_SET);
}



/*
 * Function:	readAllRegisters()
 * Description:	Reads all registers in transceiver module (Used for debugging)
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void readAllRegisters(void){

	uint8_t value[6] = {};
	uint8_t address[1] = {};

	for(address[0] = 0x00; address[0]<=0x1D;address[0]++){

		if(address[0] != 0x18 && address[0] != 0x19 &&  address[0] != 0x1A && address[0] != 0x1B){

			if(address[0] == 0x0A || address[0] == 0x0B || address[0] == 0X10){

				spiTransaction(address, value, 6);
				printf("Value in reg at address: %x is %x%x%x%x%x\n", address[0], value[1], value[2], value[3], value[4], value[5]);
			}
			else{
				spiTransaction(address, value, 2);
				printf("Value in reg at address: %x is %x\n",address[0],value[1]);
			}
		}
	}
}

