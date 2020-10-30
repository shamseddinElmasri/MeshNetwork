/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"
#include "Transceiver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint32_t counter = 0;
uint8_t routingTable[255] = {0};
uint8_t  advCounter[255] = {0};

//uint8_t timeElapsed = 0;

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */


  uint8_t receivedPacket[32];
  uint8_t receivedData[25];
  uint8_t txPacket[32];
  char txData[25] = "Shamseddin Elmasri 12345";


  uint8_t ackFlag = 0;
  uint8_t ackReceived = 0;
  uint8_t broadcasting = 0;
  uint8_t pushButton;

  struct packetHeader pHeader;
  struct headerFlags  hFlags;


 // printf("\r\n\r\n\r\n");

  //readAllRegisters();



  //printf("\r\n");

  //readAllRegisters();

  state = START_STATE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  switch(state){
	  	case START_STATE:

	  		transceiverInit();									// Initialize transceiver

	  		state = PRX_STATE;									// Switch to PRX state

	  		break;

	  	case PRX_STATE:

	  		/* timer will be used instead of this */
	  		counter++;
	  		if(counter == 500000){
	  			broadcastRoutingTable(routingTable, txPacket);
	  			broadcasting = 1;
	  			state = PTX_STATE;								// Switch to PTX state to send advertisement
	  			counter = 0;
	  			break;
	  		}
	  		pushButton = HAL_GPIO_ReadPin(GPIOC,PBUTTON);

	  		/* Check if packet arrived */
	  		if(HAL_GPIO_ReadPin(GPIOC,IRQ) == 0){

	  			CE_LOW();													// Enter standby-I mode

	  			memset(receivedPacket, 0, sizeof(receivedPacket));			// Clear packet array
	  			memset(receivedData, 0, sizeof(receivedData));				// Clear data array

	  			hal_nrf_read_rx_pload(receivedPacket);						// Read received packet
	  			hal_nrf_get_clear_irq_flags(); 								// Clear data ready flag

	  			state = CHECK_TYPE_STATE;								// Switch to Check Type state
	  		}
	  		else if(pushButton == 0){
	  			while(pushButton == 0){

	  			}

	  			state = PTX_STATE;
	  		}
	  		break;

	  	case CHECK_TYPE_STATE:

	  		pHeader = disassemblePacket(receivedData, receivedPacket);		// Disassemble packet

	  		if(pHeader.type == ADVERTISEMENT){
	  			updateRoutingTable(receivedData, pHeader.sourceAddr);
	  			printf("Routing Table:\r\n");
	  			for(int i = 1; i < 25; i++){
	  				printf("%u\t%u",i, routingTable[i]);
	  			}
	  			CE_HIGH();
	  			state = PRX_STATE;										// Switch to PRX state
	  		}
	  		else if(pHeader.type == DATA){

	  			state = CHECK_ADDRESS_STATE;							// Switch to Check Address state
	  		}
	  		else if(pHeader.type == ACK){
	  			ackReceived = 1;
	  			printf("Ack packet received\r\n");
	  		}
	  		break;

	  	case CHECK_ADDRESS_STATE:

	  		/* Check destination address */

	  		if(pHeader.destAddr == MYADDRESS){

	  			state = PROCESS_PACKET_STATE;								// Switch to Process Packet state
	  		}
	  		else if(pHeader.gateway == MYADDRESS){
	  			state = RELAY_PACKET_STATE;									// Switch to Relay Packet state
	  		}
	  		else{

	  			// Discard packet
	  			printf("Wrong gateway, packet dropped!\r\n");
	  			CE_HIGH();
	  			state = PRX_STATE;											// Switch to PRX state
	  		}
	  		break;

	  	case PROCESS_PACKET_STATE:

	  		if(pHeader.checksum != calculateChecksum(receivedData)){

	  			// Discard packet
	  			printf("Checksum error, packet dropped!\r\n");
	  			CE_HIGH();
	  			state = PRX_STATE;

	  		}
	  		else{

	  			hFlags = processHeader(&pHeader);
	  			displayData(receivedData);

	  			if(hFlags.ackFlag == 1){
	  				state = ACK_STATE;							// Switch to Ack state
	  			}
	  			else{
	  				CE_HIGH();
	  				state = PRX_STATE;							// Switch to PRX state
	  			}
	  		}

	  		break;

	  	case RELAY_PACKET_STATE:

	  		if(pHeader.TTL == 0){

	  			// Discard packet
	  			printf("TTL reached zero, packet dropped!\r\n");
	  			CE_HIGH();
	  			state = PRX_STATE;											// Switch to PRX state

	  		}
	  		else{

	  			/*
		  		new_gateway = checkRoutingTable();

				// If gateway found, update header fields

	  			// pHeader.TTL--;
	  			// pHeader.gateway = new_gateway;

	  			// Transmit packet

	  			*/
	  		}

	  		break;
	  	case ACK_STATE:

	  		//sendAckPacket();								// Send Ack Packet
	  		state = PRX_STATE;								// Switch to PRX state
	  		break;

	  	case PTX_STATE:

	  		CE_LOW();										// Enter standby-I mode
	  		PTX_Mode();										// Set module as PTX

	  		if(broadcasting == 0){

	  			pHeader = setHeaderValues(NODE_1, NODE_1, NODE_2, 255, DATA, 0b0010, 0, 0); // Configure header fields
	  			assemblePacket(txData, txPacket, pHeader);		// Prepare packet
	  			ackReceived = 0;								// Reset Ack receive flag
	  		}
	  		hal_nrf_write_tx_pload(txPacket, 32);			// Load packet
			CE_HIGH();										// Send packet

		  	HAL_Delay(1);									// To be adjusted to 10 microseconds!!!!!!!

		  	CE_LOW();										// Enter standby-I mode

	  		PRX_Mode();										// Set module as PRX

	  		CE_HIGH();
	  		state = PRX_STATE;								// Switch to PRX state
	  		break;

	  	default:
	  		state = PRX_STATE;								// Switch to PRX state
	  		break;
	  }

  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
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
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/*
 *  Read All registers
 *
 *  */
void readAllRegisters(void){
	uint8_t value[6] = {};
	uint8_t address[1] = {};

	for(address[0] = 0x00; address[0]<=0x1D;address[0]++){

		if(address[0] != 0x18 && address[0] != 0x19 &&  address[0] != 0x1A && address[0] != 0x1B){

			if(address[0] == 0x0A || address[0] == 0x0B || address[0] == 0X10){

				spiTransaction(address, value, 6);
				printf("Value in reg at address: %x is %x%x%x%x%x\r\n", address[0], value[1], value[2], value[3], value[4], value[5]);
			}
			else{
				spiTransaction(address, value, 2);
				printf("Value in reg at address: %x is %x\r\n",address[0],value[1]);
			}
		}
	}
}

uint8_t spiTransaction(uint8_t *tx, uint8_t *rx, uint8_t nOfBytes){

	CSN_LOW();
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, nOfBytes, 100);
	CSN_HIGH();
	return *rx;
}

/*
 *
 */
void transceiverPtxInit(void){

	HAL_Delay(100);											// Power on reset transition state

	hal_nrf_set_power_mode(HAL_NRF_PWR_UP);					// Power up module

	HAL_Delay(5);											// Oscillator startup delay

	hal_nrf_set_irq_mode(HAL_NRF_RX_DR, false);				// Disable "Data Sent" interrupt
	hal_nrf_set_irq_mode(HAL_NRF_MAX_RT, false);			// Disable "Max Number of Retransmits" interrupt

	hal_nrf_set_crc_mode(HAL_NRF_CRC_8BIT);					// Enable CRC mode

	hal_nrf_set_operation_mode(HAL_NRF_PTX);				// Set module in receiver mode

	hal_nrf_close_pipe(HAL_NRF_ALL);						// Disable Enhanced mode

	hal_nrf_open_pipe(HAL_NRF_PIPE0, false);				// Open pipe0 without auto Ack

	hal_nrf_set_auto_retr(0, 0);							// Disable Automatic Retransmission

	hal_nrf_disable_dynamic_pl();							// Disable dynamic payload length feature

	hal_nrf_set_rx_pload_width(0, 32);							// Set payload width

	setDataRate(DATA_RATE_250KBPS);							// Set RF data rate

	hal_nrf_set_address_width(HAL_NRF_AW_5BYTES);			// Set address width

	hal_nrf_set_rf_channel(16);								// Set RF channel

	uint8_t pipe0Addr[] = {0x3B, 0x2D, 0x0E, 0x38, 0x77}; 	// Address of pipe 0
	hal_nrf_set_address(HAL_NRF_PIPE0, pipe0Addr);			// Set Rx address

	uint8_t txAddr[] = {0x61, 0x37, 0x71, 0xF4, 0x94};		// Address of destination
	hal_nrf_set_address(HAL_NRF_TX, txAddr);				// Set Tx address

}


/*
 *
 */
void transceiverInit(void){

	 CE_LOW();										// Set chip enable bit low

	HAL_Delay(100);									// Power on reset transition state

	hal_nrf_set_power_mode(HAL_NRF_PWR_UP);			// Power up module

	HAL_Delay(5);									// Oscillator startup delay

	hal_nrf_set_irq_mode(HAL_NRF_RX_DR, true);		// Enable "Data Ready" interrupt
	hal_nrf_set_irq_mode(HAL_NRF_TX_DS, false);		// Disable "Data Sent" interrupt
	hal_nrf_set_irq_mode(HAL_NRF_MAX_RT, false);	// Disable "Max Number of Retransmits" interrupt

	hal_nrf_set_crc_mode(HAL_NRF_CRC_8BIT);			// Enable CRC mode

	hal_nrf_set_operation_mode(HAL_NRF_PRX);		// Set module in receiver mode

	hal_nrf_close_pipe(HAL_NRF_ALL);				// Disable Enhanced mode

	hal_nrf_open_pipe(HAL_NRF_PIPE0, false);		// Open pipe0 without auto Ack

	hal_nrf_set_auto_retr(0, 0);					// Disable Automatic Retransmission

	hal_nrf_disable_dynamic_pl();					// Disable dynamic payload length feature

	hal_nrf_set_rx_pload_width(0, 32);				// Set payload width

	setDataRate(DATA_RATE_250KBPS);					// Set RF data rate

	hal_nrf_set_address_width(HAL_NRF_AW_5BYTES);	// Set address width

	hal_nrf_set_rf_channel(16);						// Set RF channel

	uint8_t pipe0Addr[] = {0x61, 0x37, 0x71, 0xF4, 0x94}; // Address of pipe 0
	hal_nrf_set_address(HAL_NRF_PIPE0, pipe0Addr);			// Set Rx address

	uint8_t txAddr[] = {0x61, 0x37, 0x71, 0xF4, 0x94};		// Address of destination
	hal_nrf_set_address(HAL_NRF_TX, txAddr);				// Set Tx address


	hal_nrf_flush_rx();										// Flush Rx FIFO

	hal_nrf_flush_tx();										// Flush TX FIFO

	hal_nrf_get_clear_irq_flags(); 					// Clear all interrupt flags

	 CE_HIGH();										// Set chip enable bit high
}

/*
*
*	set RF data rate
*
*/

void setDataRate(dataRate dRate){

	if(dRate == DATA_RATE_250KBPS){

		hal_nrf_write_reg(RF_SETUP, (hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR) | (1<<RF_DR_LOW)));
	}
	else if(dRate == DATA_RATE_1MBPS){

		hal_nrf_write_reg(RF_SETUP, (hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR_LOW) & ~(1<<RF_DR)));
  }
  else
  {
    hal_nrf_write_reg(RF_SETUP, (hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR_LOW) | (1<<RF_DR)));
  }

}


/*
 *
 */
struct packetHeader disassemblePacket(uint8_t *receivedData, uint8_t *receivedPacket){

	struct packetHeader _packetHeader;

	/* Extract header fields */
	_packetHeader.destAddr		= receivedPacket[0];
	_packetHeader.gateway		= receivedPacket[1];
	_packetHeader.sourceAddr	= receivedPacket[2];
	_packetHeader.TTL 			= receivedPacket[3];
	_packetHeader.type 			= receivedPacket[4];
	_packetHeader.packetFlags 	= receivedPacket[5];
	_packetHeader.PID 			= receivedPacket[6];

	/* Extract data */
	for(int i = 0; i < 24; i++){
		receivedData[i] = receivedPacket[7 + i];
	}

	_packetHeader.checksum = receivedPacket[31];

	return _packetHeader;
}


/*
 *
 */
void assemblePacket(const char* payload, uint8_t* _packet, struct packetHeader pHeader){

	memset(_packet, 0, sizeof(_packet));						// Clear packet

	/* Assemble packet header fields */
	_packet[0] = pHeader.destAddr;
	_packet[1] = pHeader.gateway;
	_packet[2] = pHeader.sourceAddr;
	_packet[3] = pHeader.TTL;
	_packet[4] = pHeader.type;
	_packet[5] = pHeader.packetFlags;
	_packet[6] = pHeader.PID;

	/* Prepare payload field and calculate checksum */
	for(int i = 0; i < 24; i++){
		_packet[i + 7] = (uint8_t)payload[i];
		pHeader.checksum += countSetBits(payload[i]);					// Calculate check sum
	}
	_packet[31] = pHeader.checksum;										// Attached checksum byte
}
/*
 *
 */

struct headerFlags processHeader(const struct packetHeader *_packetHeader){

	struct headerFlags _headerFlags;

	/* Check destination address */
	if(_packetHeader->destAddr != MYADDRESS){

		printf("This packet is not for this node.\r\n");
		printf("Need to check routing table.\r\n");
		}

	/* Check source address */
	if( (_packetHeader->sourceAddr >= MAX_NODE) || (_packetHeader->sourceAddr < 0)){
		printf("Packet received from unknown Node \r\n");
	}
	else{
		printf("Packet received from Node %d\r\n", _packetHeader->sourceAddr);
	}

	/* Read TTL */
	printf("TTL = %d\r\n", _packetHeader->TTL);

	/* Check packet type */

	switch(_packetHeader->type){
		case DATA: 			printf("Packet type: Data\r\n"); break;
		case ADVERTISEMENT: printf("Packet type: Adv\r\n"); break;
		case ACK: 			printf("Packet type: Ack\r\n"); break;
		default: 			printf("Invalid packet type\r\n"); break;
	}

	/* Check packet flags */

	if((_packetHeader->packetFlags & 0x01) != 0){
		_headerFlags.ackFlag = 1;
		printf("Ack is required\r\n");
	}
	else{
		_headerFlags.ackFlag = 0;
		printf("No Ack is required\r\n");
	}

	if((_packetHeader->packetFlags & 0x02) != 0){
		_headerFlags.lastPacket = 1;
		printf("Last packet in data stream\r\n");
	}
	else{
		_headerFlags.lastPacket = 0;
		printf("More packets in data stream \r\n");
	}

	/* Check Packet ID */

	printf("Packet ID: %d\r\n", _packetHeader->PID);

	/* Display checksum */

	printf("Checksum: %d\r\n", _packetHeader->checksum);

	printf("\r\n");

	return _headerFlags;
}

/*
 *
 */
void displayData(uint8_t *receivedData){

	/* Display data */
	printf("Packet received:\t");

	for(int i = 0; i < 25; i++){
		printf("%c", (char)receivedData[i]);
	}
	printf("\r\n\r\n\r\n");
}


uint8_t calculateChecksum(const uint8_t *payload){

	uint8_t checksum = 0;

	for(int i = 0; i < 24; i++){
			checksum += countSetBits(payload[i]);					// Calculate checksum
		}

	return checksum;
}

/*
 *
 */
uint8_t countSetBits(uint8_t n)
{
    uint8_t count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}


/*
 *
 */
struct packetHeader setHeaderValues(uint8_t destAddr, uint8_t gateway, uint8_t sourceAddr, uint8_t TTL, uint8_t type, uint8_t packetFlags, uint8_t PID, uint8_t checksum){

	struct packetHeader pHeader = {destAddr, gateway, sourceAddr, TTL, type, packetFlags, PID, checksum};

	return pHeader;
}


/*
 *
 */
void broadcastRoutingTable(const uint8_t* rTable, uint8_t* _packet){

	struct packetHeader pHeader = {0, 0, MYADDRESS, 0, ADVERTISEMENT, 0b0000, 0, 0};

	memset(_packet, 0, sizeof(_packet));						// Clear packet


	_packet[0] = pHeader.destAddr;
	_packet[1] = pHeader.gateway;
	_packet[2] = pHeader.sourceAddr;
	_packet[3] = pHeader.TTL;
	_packet[4] = pHeader.type;
	_packet[5] = pHeader.packetFlags;
	_packet[6] = pHeader.PID;

	_packet[31] = calculateChecksum(rTable);

	assemblePacket(rTable, _packet, pHeader);
}


/*
 *
 */
void updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr){

	routingTable[sourceAddr] = sourceAddr;				// Set neighbour as gateway to itself

	for(int i = 1; i < 25; i++){
		/* Skip entry if it contains MYADDRESS, sourceAddr, common neighbours or empty */
		if((i == MYADDRESS) || (i == sourceAddr) || (routingTable[i]  == i) || (rTable[i-1] == 0)){
			continue;
		}
		else if(rTable[i-1] != 0){
			routingTable[i] = sourceAddr;			// Set neighbour as gateway to its non-common neighbours
		}
	}
	advCounter[sourceAddr]++;
}


/*
 *
 */
void PTX_Mode(void){

	hal_nrf_flush_tx();								// Flush TX FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PTX);		// Set module in receiver mode

}


/*
 *
 */
void PRX_Mode(void){

	hal_nrf_flush_rx();								// Flush Rx FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PRX);		// Set module in receiver mode

}

/*
 *
 */
void CSN_LOW(void){

	HAL_GPIO_WritePin(GPIOB, CSN, GPIO_PIN_RESET);
}

/*
 *
 */
void CSN_HIGH(void){

	HAL_GPIO_WritePin(GPIOB, CSN, GPIO_PIN_SET);
}

/*
 *
 */
void CE_LOW(void){

	HAL_GPIO_WritePin(GPIOB, CE, GPIO_PIN_RESET);
}

/*
 *
 */
void CE_HIGH(void){

	HAL_GPIO_WritePin(GPIOB, CE, GPIO_PIN_SET);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
