/*******************************************************************************
  * File Name          : System_improvements.c
  * Description        : Auto Retransmission and Dynamic output power algorthims along with commands
  * 
  * Author             : Mohana Velisetti          
  * Date               : 14-Dec-2020			 
  ******************************************************************************
  */

#include<stdio.h>
#include<stdint.h>

#include "common.h"
#include "main.h"

#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"
#include "Transceiver.h"
TIM_HandleTypeDef tim3;        //used for dynamic output power
TIM_HandleTypeDef tim16;       //used for auto retransmission
static enum nodeState state;

void decrease_op_power(void);  // decreasing output power by a step
void increase_op_power(void);  // increasing output power by a step    

/* Timer 3 used for implementing Dynamic output power algorithm */
void Timer3Init_IT(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();
  
  tim3.Instance = TIM3;
  tim3.Init.Prescaler     = 7199;                   //divide the sysclk with 7199 to get 0.1 milli seconds resolution
  tim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
  tim3.Init.Period        = 40;                     //invoking periodelapsedcallback function(ISR) for every 4000 microseconds
  tim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim3.Init.RepetitionCounter = 0;
  HAL_TIM_Base_Init(&tim3);
	/*Configure the TIM6 IRQ priority */
  HAL_NVIC_SetPriority(TIM3_IRQn, 0 ,1);
  
  /* Enable the TIM6 global Interrupt */
  HAL_NVIC_EnableIRQ(TIM3_IRQn);

  //HAL_TIM_Base_Start_IT(&tim3);                   //Enabling the timer with interrupt enable and we will enable it after transmitting a packet which requires acknowledgement
	//printf("Timer3 has initialized successfully with interrupt enabled\n");      
}
/**
 * @brief  This function handles TIM interrupt request.
 * @param  None
 * @retval None
 */
void TIM3_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&tim3);
}

/* Timer 16 used for implementing Auto retransmission algorithm */
void Timer16_Init_IT(void)           
{
  __HAL_RCC_TIM16_CLK_ENABLE();
  
  tim16.Instance = TIM16;
  tim16.Init.Prescaler     = 7199;   //divide the sysclk with 7199 to get 0.1 milli seconds resolution
  tim16.Init.CounterMode   = TIM_COUNTERMODE_UP;
  tim16.Init.Period        = 40;    //invoking periodelapsedcallback function(ISR) for every 4000 microseconds
  tim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim16.Init.RepetitionCounter = 0;
  HAL_TIM_Base_Init(&tim16);

  /*Configure the TIM16 IRQ priority */
  HAL_NVIC_SetPriority(TIM16_IRQn, 0 ,1);
  
  /* Enable the TIM16 global Interrupt */
  HAL_NVIC_EnableIRQ(TIM16_IRQn);

  //HAL_TIM_Base_Start_IT(&tim16); //Enabling the timer with interrupt enable and we will enable it after transmitting a packet which requires acknowledgement
  //printf("Timer16 has initialized successfully with interrupt enabled\n");
}
/**
 * @brief  This function handles TIM interrupt request.
 * @param  None
 * @retval None
 */
void TIM16_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&tim16);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3) {                            //checks whether interrput raises by TIM3 to do Dynamic output power
		if(/*ack_Packet == 1*/1)                               // A flag to indicate acknowlegement is received
		{
			if(power_flag == flag_18DBM)                         //if we're already using minimum output power then stop interrupt timer and proceed with data stream
			{
				//proceed with data stream;
				HAL_TIM_Base_Stop_IT(&tim3);
			}
			else
			{
				decrease_op_power();                           //if not using minimum output power then increment the power by 1 lvl & restart the timer and start the process again
				TIM3->CNT = 0;
				HAL_TIM_Base_Start_IT(&tim3);
			}
		 }
		else
		{
			if(power_flag != flag_0DBM)                      //if not using the maximum output power then increment the output power by 1 lvl and proceed with data stream
			{
				increase_op_power();
				//proceed with data stream;
				HAL_TIM_Base_Stop_IT(&tim3);
			}
			else
			{   //if using the maximum output power and cannot reach the destination node then update the routing table.
				//updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr); 
				state = PRX_STATE;
			}
		}
	}
  /****************************************************************************************************************************/
	if (htim->Instance == TIM16) {                                             //checks whether interrput raises by TIM16 to do auto retransmission
    
		static uint8_t retransmission_Counter = 0;                               //using static, cause we don't need old values to get re-written until we assign '0' to it
		uint8_t max_Retransmission_Attempts = 5;                                 //desired number of retransmission attempts
		if(/*ack_Packet == 1*/ 1)                                                // A flag to indicate acknowlegement is received
		{
			retransmission_Counter = 0;
			HAL_TIM_Base_Stop_IT(&tim16);                                          // Stopping the timer16(ISR)
			state = PRX_STATE;                                                     //go back to receiver mode
		}
	    else if(retransmission_Counter >= max_Retransmission_Attempts)
		{
			retransmission_Counter = 0;
			//Discard the packet;
			//updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr);   
			TIM16->CNT = 0;
			HAL_TIM_Base_Stop_IT(&tim16);
		}
		else if(retransmission_Counter < max_Retransmission_Attempts)   // if retransmission_counter doesn't excees the max_Retransmission_Attempts
		{
		    retransmission_Counter ++;
		    //transmitData(uint8_t* _packet);                        //  Use same packet to re-transmit;  increment the counter,reset timer16 counter						
		    TIM16->CNT = 0;                                          //  Reset Timer 16 counter to 0, to start the timer again
		}
	}
}

// FUNCTION      : decrease_op_power
// DESCRIPTION   :
//   This function will decrement the output power by 1 level.
// PARAMETERS    : Nothing
//
// RETURNS       : Nothing
void decrease_op_power()
{
	if(power_flag == flag_0DBM)
	{
		hal_nrf_set_output_power(HAL_NRF_6DBM);                //if 0dbm then output power is at 0dBM and setting it to -6dBM
	}
	else if(power_flag == flag_6DBM)
	{
		hal_nrf_set_output_power(HAL_NRF_12DBM);        //if 6dbm then output power is at 6dBM and setting it to -12dBM
	}
	else if(power_flag == flag_12DBM)
	{		
	    hal_nrf_set_output_power(HAL_NRF_18DBM); //if 12dbm then output power is at 12dBM and setting it to -18dBM
	}
	else if(power_flag == flag_18DBM)
	{
		asm volatile ("nop\n");          //if 18dbm then output power is at 18dBM so do nothing.
	}
}
// FUNCTION      : increase_op_power
// DESCRIPTION   :
//   This function will increment the output power by 1 level.
// PARAMETERS    : Nothing
//
// RETURNS       : Nothing
void increase_op_power()
{
	if(power_flag == flag_6DBM)
	{
		hal_nrf_set_output_power (HAL_NRF_0DBM);               //if 6dbm then output power is at 6dBM and setting it to 0dBM
	}
	else if(power_flag == flag_12DBM)
	{
		hal_nrf_set_output_power(HAL_NRF_6DBM);         //if 12dbm then output power is at 12dBM and setting it to 6dBM
	}
	else if(power_flag == flag_18DBM)
	{
		hal_nrf_set_output_power(HAL_NRF_12DBM); //if 18dbm then output power is at 18dBM and setting it to 12dBM
	}
	else if(power_flag == flag_0DBM)
	{
		asm volatile ("nop\n");           //if 0dbm then output power is at 0dBM so do nothing.
	}	
}

/********************************************************
  Command for setting RF data rate
*********************************************************/
ParserReturnVal_t CmdRFdatarate(int mode)
{
	int rc;
	uint16_t value;
  if(mode != CMD_INTERACTIVE) return CmdReturnOk;
	rc = fetch_uint16_arg(&value);
	if(rc)
	{
		printf("please supply 250 or 1 or 2\n");
		return CmdReturnBadParameter1;
	}
	if(value == 250)
	{
		setDataRate(DATA_RATE_250KBPS);
	  }
	else if(value == 1)
	{
		setDataRate(DATA_RATE_1MBPS);
	}
	else if(value ==2)
	{
		setDataRate(DATA_RATE_2MBPS);
	}
	else if(value != 250 || 1 || 2)
	{
		printf("wrong parameter for datarate has selected\n");
	}
	return CmdReturnOk;
}
ADD_CMD("datarate",CmdRFdatarate,"<250> <1> <2>   Set RF datarate")

/********************************************************
  Command for setting RF channel
*********************************************************/
ParserReturnVal_t CmdRFdatachannel(int mode)
{
	int rc;
	uint16_t value;
  	if(mode != CMD_INTERACTIVE) return CmdReturnOk;
	rc = fetch_uint16_arg(&value);
	if(rc)
	  {
		printf("please supply <0-127> \n");
		return CmdReturnBadParameter1;
	  }
	if(value <= 127 && value >=2)
	{
		hal_nrf_set_rf_channel(value);
	}
	else
	{
		printf("wrong parameter for RF channel has selected\n");
	}
	return CmdReturnOk;
}
ADD_CMD("channel",CmdRFdatachannel,"<2-127>   Set RF datarate")

/********************************************************
  Command for setting RF transmitter output power
*********************************************************/
ParserReturnVal_t CmdRFop(int mode)
{
	int rc;
	uint16_t value;
  	if(mode != CMD_INTERACTIVE) return CmdReturnOk;
	rc = fetch_uint16_arg(&value);
	if(rc)
	{
		printf("please supply <0> <6> <12> <18>\n");
		return CmdReturnBadParameter1;
	}
	if(value == 0)
	{
		hal_nrf_set_output_power(HAL_NRF_0DBM);
	}
	else if(value == 6)
	{
		hal_nrf_set_output_power(HAL_NRF_6DBM);
	}
	else if(value == 12)
	{
	   hal_nrf_set_output_power(HAL_NRF_12DBM);
	}
	else if(value == 18)
	{
		hal_nrf_set_output_power(HAL_NRF_18DBM);
	}  	
	else if(value != 0 || 6 || 12 || 18)
	{
		printf("wrong parameter for output power has selected\n");
	}
  return CmdReturnOk;
}
ADD_CMD("power",CmdRFop,"<0> <6> <12> <18>  Set RF output power of transmitter")

/********************************************************
  Command for setting data pipe no. and address channel
*********************************************************/
ParserReturnVal_t Cmddatapipe(int mode)
{
	int rc;
	uint16_t pipe_no;
	uint16_t value[6];                                                        //takes 16 bit inputs from command line
	uint8_t pipe_arr[6];                                                      //pipe_array to be passed to hal_nrf_set_address function
	uint8_t *temp_ptr0,*temp_ptr1,*temp_ptr2,*temp_ptr3,*temp_ptr4,*temp_ptr5;

	
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	rc = fetch_uint16_arg(&pipe_no);

	if(rc)
	{
		printf("please supply pipe number \n");
		return CmdReturnBadParameter1;
	}

	rc = fetch_uint16_arg(&value[0]);  //store 16-bit values into value array from command line.
	rc = fetch_uint16_arg(&value[1]);
	rc = fetch_uint16_arg(&value[2]);
	rc = fetch_uint16_arg(&value[3]);
	rc = fetch_uint16_arg(&value[4]);
	rc = fetch_uint16_arg(&value[5]);

	temp_ptr0 = (uint8_t *) &value[0]; //typecasting 16 bit value address to temp_ptr which is 8 bit pointer
	temp_ptr1 = (uint8_t *) &value[1];
	temp_ptr2 = (uint8_t *) &value[2];
	temp_ptr3 = (uint8_t *) &value[3];
	temp_ptr4 = (uint8_t *) &value[4];
	temp_ptr5 = (uint8_t *) &value[5];

	pipe_arr[0] = *temp_ptr0;          //transferring uint8_t pointer data to uint8_t pipe_array.
	pipe_arr[1] = *temp_ptr1;
	pipe_arr[2] = *temp_ptr2;
	pipe_arr[3] = *temp_ptr3;
	pipe_arr[4] = *temp_ptr4;
	pipe_arr[5] = *temp_ptr5;

	
	
	if(pipe_no >= 0 && pipe_no <=5)
	{
		switch(pipe_no)
		{
			case 0: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE0, pipe_arr);
				break;
			}
			case 1: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE1, pipe_arr);
				break;
			}
			case 2: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE2, pipe_arr);
				break;
			}
			case 3: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE3, pipe_arr);
				break;
			}
			case 4: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE4, pipe_arr);
				break;
			}
			case 5: 
			{
				hal_nrf_set_address(HAL_NRF_PIPE5, pipe_arr);
				break;
			}
			default : 
			{
				return 1;
			}

		}
		
	}
	return CmdReturnOk;
}
ADD_CMD("pipe",Cmddatapipe,"<0-5> <byte1> <byte2> <byte3> <byte4> <byte4>   Set pipe no. and pipe address")

/********************************************************
  Command for setting transmitter address
*********************************************************/
ParserReturnVal_t Cmdtxaddr(int mode)
{
	uint16_t value[6];                                                        //takes 16 bit inputs from command line
	uint8_t tx_addr[6];                                                       //tx_addr to be passed to hal_nrf_set_address function
	uint8_t *temp_ptr0,*temp_ptr1,*temp_ptr2,*temp_ptr3,*temp_ptr4,*temp_ptr5;

	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	fetch_uint16_arg(&value[0]);        //store 16-bit values into value array from command line.
	fetch_uint16_arg(&value[1]);
	fetch_uint16_arg(&value[2]);
	fetch_uint16_arg(&value[3]);
	fetch_uint16_arg(&value[4]);
	fetch_uint16_arg(&value[5]);

	temp_ptr0 = (uint8_t *) &value[0]; //typecasting 16 bit value address to temp_ptr which is 8 bit pointer
	temp_ptr1 = (uint8_t *) &value[1];
	temp_ptr2 = (uint8_t *) &value[2];
	temp_ptr3 = (uint8_t *) &value[3];
	temp_ptr4 = (uint8_t *) &value[4];
	temp_ptr5 = (uint8_t *) &value[5];

	tx_addr[0] = *temp_ptr0;           //transferring uint8_t pointer data to uint8_t pipe_array.
	tx_addr[1] = *temp_ptr1;
	tx_addr[2] = *temp_ptr2;
	tx_addr[3] = *temp_ptr3;
	tx_addr[4] = *temp_ptr4;
	tx_addr[5] = *temp_ptr5;
	
	hal_nrf_set_address(HAL_NRF_TX, tx_addr);
	return CmdReturnOk;
}
ADD_CMD("txaddress",Cmdtxaddr,"<byte1> <byte1> <byte1> <byte1> <byte1>   Set tx_address")
