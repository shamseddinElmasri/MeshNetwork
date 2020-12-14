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
 * Function:	PRX_Init()
 * Description: Executes at the begining of the PRX_Task. It initializes the SPI,
 *		transceiver module, timer 2, timer 17 and queue.
 * Parameters: 	void* data: Points to optional data that can be passed
 * Returns:	Nothing
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
 * Function:	PRX_Task()
 * Description: Main task that contains the finite state machine
 * Parameters: 	void* data: Points to optional data that can be passed
 * Returns:	Nothing
 */ 
void PRX_Task(void *data){
	
	static uint8_t receivedPacket[32];			// Stores the received packet
	static char receivedData[25];				// Holds the payload portion of received packet

	static struct packetHeader _pHeader;			// Instance of packet header structure
	static struct packetHeader *pHeader = &_pHeader;	// Points to packet header structure
  	static struct headerFlags  _hFlags;			// Instance of packet header flags structure
	static struct headerFlags  *hFlags = &_hFlags;		// Points to packet header flags structure
	
	static enum nodeState state = PRX_STATE;		// Start FSM at primary receiver state
	
	
	
	/* Finite State Machine*/
	switch(state){

		/*Primary Receiver State*/
		case PRX_STATE:
		
			//* Check queue for new received packets */
			if(rxPacketsQ.Q_Counter > 0){
	
				memset (receivedPacket, 0, PACKETLENGTH);	// Clear array to accept new packet
				memset(receivedData, 0, 25);			// Clear data array
				EXTI->IMR &= 0xFFFFFF7F; 			// Disable EXTI Interrupt
				dequeue(receivedPacket);			// Dequeue received packet
				EXTI->IMR |= 0x00000080;			// Enable EXTI Interrupt
								
				state = CHECK_TYPE_STATE;			// Switch to CHECK_TYPE_STATE
				break;
			}
		
			/* Check if its time to broadcast routing table */
			if(broadcasting == 1){

				/* Check if 20-second period elapsed */
				if(secondsCounter >= 4){
					deleteInactiveNodes();  // Delete inactive nodes from routing table
					secondsCounter = 0;     // Reset counter
				}
      	
				broadcastRoutingTable(pHeader, routingTable, advPacket); // Prepare advertisement packet

				state = PTX_STATE;		// Switch to PTX_STATE
		    		break;
			}
			
			/* Check if there's a data packet to be transmitted*/
			if(dataTransmitFlag){
				
				state = PTX_STATE;		// Switch to PTX_STATE
			}	
	
			break;
		
		
		/* Check Packet Type State*/
		case CHECK_TYPE_STATE:
			
			disassemblePacket(pHeader, (uint8_t*)receivedData, receivedPacket); // Disassemble packet

			if(pHeader->type == ADVERTISEMENT){
	  					
				updateRoutingTable((uint8_t*)receivedData, pHeader->sourceAddr);
				  			
				displayRoutingTable();
				
				CE_HIGH();
				state = PRX_STATE;			// Switch back to PRX_STATE
	  		
 			}
	 		else if(pHeader->type == DATA || pHeader->type == ACK){
				
				state = CHECK_ADDRESS_STATE;		// Switch to CHECK_ADDRESS_STATE
				
 			}
			else{
				CE_HIGH();
				state = PRX_STATE;			// Switch back to PRX_STATE
			}
			break;
		
		
		/* Check Address State*/
		case CHECK_ADDRESS_STATE:
			
			/* Check destination address*/
			if(pHeader->destAddr == MYADDRESS){

				state = PROCESS_PACKET_STATE;		// Switch to PROCESS_PACKET_STATE
	  		}
	  		/* Check gateway address*/
	  		else if (pHeader->gateway == MYADDRESS){
	  		
	  			state = RELAY_PACKET_STATE;		// Switch to RELAY_PACKET_STATE
	  		}
  			/* Discard packet */
	  		else{
	  			CE_HIGH();
				state = PRX_STATE;			// Switch back to PRX_STATE
	  		}
	  		
	  		break;
	  	
	  	
	  	/* Process Packet State */
	  	case PROCESS_PACKET_STATE:	
			
		  	/*if(pHeader->checksum != calculateChecksum((uint8_t*)receivedData)){

				// Discard packet
				printf("Checksum error, packet dropped!\n");
				
				CE_HIGH();
				state = PRX_STATE;			// Switch back to PRX_STATE
			}*/	  		
  			//else{
				displayPacket(receivedData, pHeader);

				/* Update number of hops in routing table */
				routingTable[pHeader->sourceAddr + 11] = 255 - pHeader->TTL + 1;	
					
				/* Check if Ack is required */
	  			processHeader(hFlags, pHeader);
	  			
	  			if(hFlags->ackFlag == 1){
	  			
	  				state = ACK_STATE;		// Switch to ACK_STATE
	  			}
	  			/* No Ack required */
	  			else{
	  				CE_HIGH();
					state = PRX_STATE;		// Switch back to PRX_STATE
	  			}
	  		//}
			break;
	  	
	  	
	  	/* Relay Packet State */		
		case RELAY_PACKET_STATE:
			
			/* Discard packet if TTL reached zero */
			if(pHeader->TTL == 0){		  

				CE_HIGH();
				state = PRX_STATE;			// Switch back to PRX_STATE
			}
			/* Check if destination node is available in routing table */
			else{
				/* If gateway equals zero in routing table, then node is unable to relay packet*/
				if(routingTable[pHeader->destAddr - 1] == 0){
									
					CE_HIGH();
					state = PRX_STATE;		// Switch back to PRX_STATE
				}
				else{
  					pHeader->gateway = routingTable[pHeader->destAddr - 1];	// Update gateway in packet header
  					pHeader->TTL--;						// Decrement TTL

					assemblePacket(receivedData, receivedPacket, pHeader);	// Reassemble packet
					relayFlag = 1;						// Flag checked in PTX_STATE
					state = PTX_STATE;					// Switch to PTX_STATE
	  			}
	  		}

			break;
		
		
		/* Prepare Acknowledgement Packet State */	
		case ACK_STATE:
			
			/* Prepare Ack packet */
			setHeaderValues(pHeader, pHeader->sourceAddr, routingTable[pHeader->sourceAddr - 1], MYADDRESS, 255, ACK, 0b0010, 0);
			assemblePacket(ackMessage, ackPacket, pHeader);	// Assemble Ack packet to be transmitted
			ackTransmitFlag = 1;				// Flag checked in PTX_STATE
			state = PTX_STATE;				// Switch to PTX_STATE
			break;
		
									
		/* Transmission State */		
		case PTX_STATE:
			
			/* Check flags */
			if(broadcasting){
				
				transmitData(advPacket);        // Broadcast advertisement packet
				broadcasting = 0;               // Reset broadcasting Flag
			}
			else if(ackTransmitFlag){
				
				transmitData(ackPacket); 	// Transmit Ack packet	
				ackTransmitFlag = 0;		// Reset Ack transmit Flag
			}
			else if(relayFlag){
				
				transmitData(receivedPacket);	// Relay packet
				relayFlag = 0;			// Reset relay Flag
			}
			else if(dataTransmitFlag){
				
				transmitData(txPacket);		// Transmit data packet
				dataTransmitFlag = 0;		// Reset data transmit Flag
			}
			else{
				printf("Error\n");
			}
			
			PRX_Mode();				// Set transceiver to receiver mode
			CE_HIGH();
			state = PRX_STATE;			// Switch back to PRX_STATE
			break;
			
		default:
			PRX_Mode();				// Set transceiver to receiver mode
			CE_HIGH();
			state = PRX_STATE;			// Switch to PRX_STATE
			break;
	}
}

ADD_TASK (PRX_Task, PRX_Init, NULL, 0, "				Main task that contains the finite state machine")



/***************Commands*********************/
/*
 *	Command for transmitting data
 */
ParserReturnVal_t CmdtransmitPacket(int mode){
  
	uint32_t rc;					// Used to store the returned value from the fetch
	char *temp;					// Used for pointing to data to be transmitted
	uint16_t destAddr;
	uint16_t ack;					// Used to set Ack bit in packet header field
	struct packetHeader _pHeader;			// Instance of packet header structure
	struct packetHeader *pHeader = &_pHeader;	// Points to packet header structure

	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	memset(txData, 0, sizeof(txData));		// Clear data array
	temp = txData;					// Point to data array

	rc = fetch_uint16_arg(&destAddr);		// Fetch destination node from user via terminal
  
  	if(rc) {
    		printf("Please specify destination node\n");
    		return CmdReturnBadParameter1;
  	}
  	
  	rc = fetch_uint16_arg(&ack);			// Fetch ack bit from user via terminal
  
  	if(rc) {
    		printf("Please set Ack bit, '0' for no Ack, '1' for Ack\n");
    		return CmdReturnBadParameter1;
  	}
  	
	rc = fetch_string_arg(&temp);			// Fetch payload (data) from user via terminal
  
	if(rc) {
    		printf("Please supply data to be transmitted (24 bytes max)\n");
    		return CmdReturnBadParameter1;
	}
 
	setHeaderValues(pHeader, (uint8_t)destAddr, routingTable[destAddr - 1], MYADDRESS, 255, DATA, 0b0010 | ack, 0);
	assemblePacket(temp, txPacket, pHeader);
	
	dataTransmitFlag = 1;			// Flag checked in PTX_STATE in FSM

	return CmdReturnOk;
}

ADD_CMD("txPacket",CmdtransmitPacket,"         Transmit Packet. Specify destination then Ack bit then payload")



/*
 *	Command for transmitting advertisement packet (used for testing and troubleshooting purposes)
 */
ParserReturnVal_t CmdtransmitAdvPacket(int mode){
  
	struct packetHeader _pHeader;			// Instance of packet header structure
	struct packetHeader *pHeader = &_pHeader;	// Points to packet header structure

	if(mode != CMD_INTERACTIVE) return CmdReturnOk;
 
	setHeaderValues(pHeader, 0, 0, MYADDRESS, 255, ADVERTISEMENT, 0b0010, 0);
	assemblePacket((char*)routingTable, txPacket, pHeader);
	transmitData(txPacket);				// Transmit data

	return CmdReturnOk;
}

ADD_CMD("txAdv",CmdtransmitAdvPacket,"         Transmit advertisement packet")



/*
 *	Command for reading all registers in tranceiver module
 */

ParserReturnVal_t CmdReadAllReg(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	readAllRegisters();

	return CmdReturnOk;
}

ADD_CMD("readreg",CmdReadAllReg,"         read all registers in tranceiver module")



/*
 *	Command for displaying the routing table
 */
ParserReturnVal_t CmddisplayRoutingTable(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	displayRoutingTable();

	return CmdReturnOk;
}

ADD_CMD("rt",CmddisplayRoutingTable,"         Displays routing table")



/*************************************Interrupts***************************************************/
//Function: TIM17_IRQHandler()
//Description: Interrupt for sending an advertisement packet every 5 seconds and deleting inactive
//		nodes from routing table every 20 seconds
//Parameters: void
//Returns: void
void TIM17_IRQHandler(void)
{	
	secondsCounter++;	// Increment seconds counter 

	broadcasting = 1;	// Flag checked in PRX_STATE
	 
	TIM17 -> SR &= 0xfffe;	// Reset the Update Interrupt Flag (UIF)                    
}



//Function: EXTI9_5_IRQHandler()
//Description: Calls GPIO_EXTI interrput handler function
//Parameters: void
//Returns: void
void EXTI9_5_IRQHandler(void){

	HAL_GPIO_EXTI_IRQHandler(IRQ);
}



//Function: HAL_GPIO_EXTI_Callback()
//Description: 
//Parameters: 
//		uint16_t GPIO_Pin: GPIO pin that triggered the interrupt
//Returns: void
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

