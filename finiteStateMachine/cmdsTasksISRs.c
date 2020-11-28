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

/***************Tasks*********************/

/*
 * This function executes at the begining of the PRX_Task. It initializes the module and sets it in receiver mode
 */
void PRX_Init(void* data){

	spi_init();
	transceiverInit();
	timer2Init();	
	timer17Init();
	
	memset(rxPacketsQ.receivedBytes, 0, 320);	// Set queue bytes to 0
	rxPacketsQ.front = rxPacketsQ.receivedBytes;	// Setting queue front pointer to start position
	rxPacketsQ.rear = rxPacketsQ.receivedBytes;	// Setting queue rear pointer to start position
	printf("init done\n");
}


/*
 *
 */
void PRX_Task(void *data){
	
	static char receivedData[25];
	static uint8_t receivedPacket[32];
	
	static struct packetHeader _pHeader;
	static struct packetHeader *pHeader = &_pHeader;		// Pointer to instance
  	static struct headerFlags  _hFlags;
	static struct headerFlags  *hFlags = &_hFlags;
	
	static enum nodeState state = PRX_STATE;
	
	switch(state){
	
		case PRX_STATE:
		
			//* Check if received packet */
			if(rxPacketsQ.Q_Counter > 0){
	
				memset (receivedPacket, 0, PACKETLENGTH);
				EXTI->IMR &= 0xFFFFFF7F; 		// Disable EXTI Interrupt
				dequeue(receivedPacket);
				EXTI->IMR |= 0x00000080;		// Enable EXTI Interrupt
				
				memset(receivedData, 0, 25);		// Clear data array
				
				state = CHECK_TYPE_STATE;		// Switch to CHECK_TYPE_STATE
				break;
			}
		
		
			/* Broadcast routing table */
			if(broadcasting == 1){

				/* Check if 16-second period elapsed */
				if(secondsCounter >= 4){
					deleteInactiveNodes();  // Delete inactive nodes from routing table
					secondsCounter = 0;     // Reset seconds counter
				}
      	
				/* Prepare advertisement packet */  
				broadcastRoutingTable(pHeader, routingTable, advPacket); // Prepare advertisement packet

				state = PTX_STATE;	

		    		break;
			}
			
			if(dataTransmitFlag){
				
				state = PTX_STATE;
			}	
	
			break;
		
		case CHECK_TYPE_STATE:
			
			/* Check packet type */
			disassemblePacket(pHeader, (uint8_t*)receivedData, receivedPacket); // Disassemble packet

			if(pHeader->type == ADVERTISEMENT){
	  					
				updateRoutingTable((uint8_t*)receivedData, pHeader->sourceAddr);
				  			
				displayRoutingTable();
				
				CE_HIGH();
				state = PRX_STATE;			// Switch to PRX state
	  		
 			}
	 		else if(pHeader->type == DATA || pHeader->type == ACK){
				
				state = CHECK_ADDRESS_STATE;		// Switch to Check Address State
				
 			}
			else{
				CE_HIGH();
				state = PRX_STATE;			// Switch to PRX state
			}
			break;
		
		case CHECK_ADDRESS_STATE:
			
			/* Check Address */
			if(pHeader->destAddr == MYADDRESS){

				state = PROCESS_PACKET_STATE;
	  		}
	  		else if (pHeader->gateway == MYADDRESS){
	  		
	  			state = RELAY_PACKET_STATE;
	  		}
	  		else{
	  			// Discard packet
	  			printf("Wrong gateway, packet dropped!\n");
	  			CE_HIGH();
				state = PRX_STATE;			// Switch to PRX state
	  		}
	  		
	  		break;
	  		
	  	case PROCESS_PACKET_STATE:	
			
		  	/*if(pHeader->checksum != calculateChecksum((uint8_t*)receivedData)){

				// Discard packet
				printf("Checksum error, packet dropped!\n");
				printf("checksum = %u, should be %u\n",calculateChecksum((uint8_t*)receivedData), pHeader->checksum);
				CE_HIGH();
				state = PRX_STATE;			// Switch to PRX state
			}*/	  		
  			//else{
				displayPacket(receivedData, pHeader);

				/* Update number of hops in routing table */
				routingTable[pHeader->sourceAddr + 11] = 255 - pHeader->TTL + 1;	
					
				/* Check if Ack is required */
	  			processHeader(hFlags, pHeader);
	  			
	  			if(hFlags->ackFlag == 1){
	  			
	  				state = ACK_STATE;
	  			}
	  			else{
	  				printf("No Ack packet required\n");
	  				CE_HIGH();
					state = PRX_STATE;			// Switch to PRX state	
	  			}
	  		//}
			break;
	  			
		case RELAY_PACKET_STATE:
			
			if(pHeader->TTL == 0){		  

				// Discard packet if TTL reached zero
				printf("TTL reached zero, packet dropped!\n");
				CE_HIGH();
				state = PRX_STATE;			// Switch to PRX state	
			}
			else{
				// Check if destination node is available in routing table
				if(routingTable[pHeader->destAddr - 1] == 0){
					printf("Unable to relay packet from node%u to node%u\n", pHeader->sourceAddr, pHeader->destAddr);
					CE_HIGH();
					state = PRX_STATE;			// Switch to PRX state
				}
				else{
					printf("packet relayed from node%u to node%u\n", pHeader->sourceAddr, pHeader->destAddr);
  					pHeader->gateway = routingTable[pHeader->destAddr - 1];	// Update gateway
  					pHeader->TTL--;						// Decrement TTL

					assemblePacket(receivedData, receivedPacket, pHeader); // Reassemble packet
					relayFlag = 1;
					state = PTX_STATE;
					
	  			}
	  		}

			break;
			
		case ACK_STATE:
			
			/* Prepare Ack packet */
			setHeaderValues(pHeader, pHeader->sourceAddr, routingTable[pHeader->sourceAddr - 1], MYADDRESS, 255, ACK, 0b0010, 0);
			assemblePacket(ackMessage, ackPacket, pHeader);
			ackTransmitFlag = 1;
			state = PTX_STATE;
			break;
			
						
			

		case PTX_STATE:
			
			if(broadcasting){
				
				transmitData(advPacket);        // Broadcast packet
				broadcasting = 0;               // Reset broadcasting Flag
			}
			else if(ackTransmitFlag){
				
				transmitData(ackPacket); 	// Transmit Ack packet	
				ackTransmitFlag = 0;
			}
			else if(relayFlag){
				
				transmitData(receivedPacket);	// Relay packet
				relayFlag = 0;
			}
			else if(dataTransmitFlag){
				
				transmitData(txPacket);		// Transmit data
				dataTransmitFlag = 0;
			}
			else{
				printf("Error\n");
			}
			
			PRX_Mode();
			CE_HIGH();
			state = PRX_STATE;			// Switch to PRX state
			break;
			
		default:
			PRX_Mode();
			CE_HIGH();
			state = PRX_STATE;			// Switch to PRX state
			break;
	}
}

ADD_TASK (PRX_Task, PRX_Init, NULL, 0, "				PRX Mode Task")



/***************Commands*********************/
/*
 *	Command for transmitting data
 */
ParserReturnVal_t CmdtransmitPacket(int mode){
  
	uint32_t rc;
	char *temp;					// Used for pointing to data to be transmitted
	uint16_t destAddr;
	uint16_t ack;					// Used to set Ack bit in packet header field
	struct packetHeader _pHeader;
	struct packetHeader *pHeader = &_pHeader;	// Pointer to instance


	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	memset(txData, 0, sizeof(txData));		// Clear data array
	temp = txData;

	rc = fetch_uint16_arg(&destAddr);		// Fetch destination node from user via terminal
  
  	if(rc) {
    		printf("Please specify destination node\n");
    		return CmdReturnBadParameter1;
  	}
  	
  	rc = fetch_uint16_arg(&ack);			// Fetch ack bit from user via terminal
  
  	if(rc) {
    		printf("Please set Ack bit\n");
    		return CmdReturnBadParameter1;
  	}
  	
	rc = fetch_string_arg(&temp);			// Fetch data from user via terminal
  
	if(rc) {
    		printf("Please supply data to be transmitted\n");
    		return CmdReturnBadParameter1;
	}
 
	setHeaderValues(pHeader, (uint8_t)destAddr, routingTable[destAddr - 1], MYADDRESS, 255, DATA, 0b0010 | ack, 0);
	assemblePacket(temp, txPacket, pHeader);
	
	dataTransmitFlag = 1;

	return CmdReturnOk;
}

ADD_CMD("txPacket",CmdtransmitPacket,"         Transmit Packet")



/*
 *	Command for transmitting advertisement packet (used for testing and troubleshooting purposes only)
 */
ParserReturnVal_t CmdtransmitAdvPacket(int mode){
  
	struct packetHeader _pHeader;
	struct packetHeader *pHeader = &_pHeader;	// Pointer to instance

	if(mode != CMD_INTERACTIVE) return CmdReturnOk;
 
	setHeaderValues(pHeader, 0, 0, MYADDRESS, 255, ADVERTISEMENT, 0b0010, 0);
	assemblePacket((char*)routingTable, txPacket, pHeader);
	transmitData(txPacket);				// Transmit data

	return CmdReturnOk;
}

ADD_CMD("txAdv",CmdtransmitAdvPacket,"         Transmit advertisement packet")


/*
 *	Command for reading all registers
 */

ParserReturnVal_t CmdReadAllReg(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	readAllRegisters();

	return CmdReturnOk;
}

ADD_CMD("readreg",CmdReadAllReg,"         read all registers")



/*
 *	Command for displaying the routing table
 */
ParserReturnVal_t CmddisplayRoutingTable(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	displayRoutingTable();

	return CmdReturnOk;
}

ADD_CMD("rt",CmddisplayRoutingTable,"         display Routing Table")

/*************************************Interrupts***************************************************/
//Function name: pTIM17_IRQHandler
//Description: Interrupt for sending an advertisement packet every 1 second and deleting inactive
//		nodes from routing table every 5 seconds
//Parameters: void
//Returns: void
void TIM17_IRQHandler(void)
{	
	secondsCounter++;	// Increment seconds counter 

	broadcasting = 1;
	 
	TIM17 -> SR &= 0xfffe;	// Reset the Update Interrupt Flag (UIF)                    
}

/*
 *
 */
void EXTI9_5_IRQHandler(void){

	HAL_GPIO_EXTI_IRQHandler(IRQ);
}

/*
 *	ISR executes whenever module receives a packet
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){

	if(GPIO_Pin == IRQ){
		
		uint8_t	rxPacket[32] = {0};
		hal_nrf_read_rx_pload(rxPacket);	// Read received packet
		enqueue(rxPacket);			// Enqueue received packet
	
		hal_nrf_get_clear_irq_flags();		// Clear data ready flag
	}
	else{
		__NOP();
	}
}
