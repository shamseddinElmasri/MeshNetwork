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


// Global variables



/***************Tasks*********************/

/*
 * This function executes at the begining of the PRX_Task. It initializes the module and sets it in receiver mode
 */
void PRX_Init(void* data){

	transceiverInit();	
}


/*
 *
 */
void PRX_Task(void *data){

	uint8_t receivedPacket[32];
	uint8_t receivedData[25];

	struct packetHeader pHeader;
  struct headerFlags  hFlags;
  
  
	
	if(HAL_GPIO_ReadPin(GPIOC,IRQ) == 0){

		CE_LOW();																										// Enter standby-I mode

		memset(receivedPacket, 0, sizeof(receivedPacket));					// Clear packet array
		memset(receivedData, 0, sizeof(receivedData));							// Clear data array

		hal_nrf_read_rx_pload(receivedPacket);											// Read received packet
		hal_nrf_get_clear_irq_flags(); 															// Clear data ready flag

		/*	Check packet type	*/
		pHeader = disassemblePacket(receivedData, receivedPacket);	// Disassemble packet

		if(pHeader.type == ADVERTISEMENT){
	  		
			updateRoutingTable(receivedData, pHeader.sourceAddr);
			printf("Routing Table:\n");
	  			
			for(int i = 1; i < 25; i++){
	  			
				printf("%u\t%u",i, routingTable[i]);
				printf("\n");
 			}
	  			
 			printf("\n");
 			CE_HIGH();
	  			
 			return;
 		}
 		else if(pHeader.type == ACK){
			
 			printf("Ack packet received\n");
		}
 		else if(pHeader.type == DATA){

			/*	Check Address	*/
			if(pHeader.destAddr == MYADDRESS){

	  		/*	Process Packet	*/
	  		if(pHeader.checksum != calculateChecksum(receivedData)){

	  			// Discard packet
	  			printf("Checksum error, packet dropped!\n");
	  			CE_HIGH();
	  			return;
	  		}
	  		else{

	  			hFlags = processHeader(&pHeader);
	  			displayData(receivedData);

	  			if(hFlags.ackFlag == 1){
						// Prepare Ack packet here !!!!!
						// Send Ack packet here !!!!!!
						transmitData();
	  			}
	  			else{
	  				CE_HIGH();
						return;
	  			}
	  		}
	  	}
	  	else if(pHeader.gateway == MYADDRESS){
	  	
	  		/*	Relay Packet	*/
	  		if(pHeader.TTL == 0){

	  			// Discard packet
	  			printf("TTL reached zero, packet dropped!\n");
	  			CE_HIGH();
	  			return;
	  		}
	  		else{

	  			// Check if destination node is available in routing table
	  			if(routingTable[pHeader.destAddr] == 0){
	  				printf("Unable to relay packet from node%u to node%u\n", pHeader.sourceAddr, pHeader.destAddr);
	  			}
	  			else{
	  				// Update gateway field in packet header for next hop
	  				pHeader.gateway = routingTable[pHeader.destAddr];
	  				// Transmit packet here!!!!!!!
	  				transmitData();
	  			}
	  		}
	  	}
	  	else{

	  		// Discard packet
	  		printf("Wrong gateway, packet dropped!\n");
	  		CE_HIGH();
	  		return;
	  	}
 		}
	}
}

ADD_TASK (PRX_Task, PRX_Init, NULL, 0, "				PRX Mode Task")



/***************Commands*********************/
/*
 *	Command for transmitting data
 */
ParserReturnVal_t Cmdtransmitfun(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	transmitData();

	return CmdReturnOk;
}

ADD_CMD("txdata",Cmdtransmitfun,"         start transmitting")


/*
 *	Command for reading all registers
 */

ParserReturnVal_t CmdReadAllReg(int mode){
  
	if(mode != CMD_INTERACTIVE) return CmdReturnOk;

	readAllRegisters();

	return CmdReturnOk;
}

ADD_CMD("readreg",CmdReadAllReg,"         read all registers")
