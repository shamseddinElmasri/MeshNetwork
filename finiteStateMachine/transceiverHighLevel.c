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


// Global TypeDefs
SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef tim2;
TIM_HandleTypeDef tim17;

// Global Variables
uint8_t routingTable[24] = {0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255};
volatile uint8_t advCounter[12]   = {0};
char 	ackMessage[25]   = "Ack: Packet Received!";
char 	txData[25];
uint8_t	txPacket[32];
uint8_t receivedPacket[32];
uint8_t	ackPacket[32];
uint8_t advPacket[32];
volatile uint8_t secondsCounter = 0; // Used for creating a 20-second period to delete inactive nodes from routing table

// Global Flags
volatile uint8_t broadcasting = 0;
volatile uint8_t ackTransmitFlag = 0;
volatile uint8_t relayFlag = 0;
volatile uint8_t dataTransmitFlag = 0;
volatile uint8_t receivedPacketFlag = 0;


/*************High Level Functions***************/
/*
 * Function:	setDataRate()
 * Description:	Sets RF data rate
 * Parameters: 	
 * 		dataRate dRate: Data rate to be set
 * Returns:	Nothing
 */
void setDataRate(dataRate dRate){

	if(dRate == DATA_RATE_250KBPS){
	
		hal_nrf_write_reg(RF_SETUP, ((hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR)) | (1<<RF_DR_LOW)));
	}
	else if(dRate == DATA_RATE_1MBPS){
	
		hal_nrf_write_reg(RF_SETUP, ((hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR_LOW)) & ~(1<<RF_DR)));
  	}
  	else{
    		hal_nrf_write_reg(RF_SETUP, ((hal_nrf_read_reg(RF_SETUP) & ~(1<<RF_DR_LOW)) | (1<<RF_DR)));
  	}
}



/*
 * Function:	disassemblePacket()
 * Description:	Disassembles received packet
 * Parameters: 	
 * 		struct packetHeader *_packetHeader: Points to the packet header structure of received packet 
 *		uint8_t *receivedData: Points to the payload of received packet
 *		uint8_t *receivedPacket: Points to the whole received packet
 * Returns:	Nothing
 */
void disassemblePacket(struct packetHeader *_packetHeader, uint8_t *receivedData, uint8_t *receivedPacket){

	
	/* Extract header fields */
	_packetHeader->destAddr	   = receivedPacket[0];
	_packetHeader->gateway     = receivedPacket[1];
	_packetHeader->sourceAddr  = receivedPacket[2];
	_packetHeader->TTL         = receivedPacket[3];
	_packetHeader->type        = receivedPacket[4];
	_packetHeader->packetFlags = receivedPacket[5];
	_packetHeader->PID         = receivedPacket[6];

	/* Extract data */
	for(int i = 0; i < 24; i++){
		receivedData[i] = receivedPacket[7 + i];	// Copying payload portion of the received packet
	}

	_packetHeader->checksum = receivedPacket[31];

}



/*
 * Function:	assemblePacket()
 * Description:	Assembles packet to be transmitted
 * Parameters: 	
 * 		const char* payload: Points to the payload portion of packet to be transmitted
 *		uint8_t* _packet: Points to the whole packet to be transmitted
 *		struct packetHeader *pHeader: Points to packet header structure of packet to be transmitted
 * Returns:	Nothing
 */
void assemblePacket(const char* payload, uint8_t* _packet, struct packetHeader *pHeader){
	
	memset(_packet, 0, PACKETLENGTH);		// Clear packet

	/* Assemble packet header fields */
	_packet[0] = pHeader->destAddr;
	_packet[1] = pHeader->gateway;
	_packet[2] = pHeader->sourceAddr;
	_packet[3] = pHeader->TTL;
	_packet[4] = pHeader->type;
	_packet[5] = pHeader->packetFlags;
	_packet[6] = pHeader->PID;

	pHeader->checksum = 0;
	/* Prepare payload field and calculate checksum */
	for(int i = 0; i < 24; i++){
		_packet[i + 7] = (uint8_t)payload[i];
		pHeader->checksum += countSetBits((uint8_t)payload[i]);	// Calculate check sum
	}
	_packet[31] = pHeader->checksum;				// Attach checksum byte
}



/*
 * Function:	processHeader()
 * Description:	Abstracts received packet header fields
 * Parameters: 	
 * 		struct headerFlags *_headerFlags: Points to packet header flags structure
 *		const struct packetHeader *_packetHeader: Points to packet header structure
 * Returns:	Nothing
 */
void processHeader(struct headerFlags *_headerFlags, const struct packetHeader *_packetHeader){

	/* Check source address */
	if( (_packetHeader->sourceAddr >= MAX_NODE) || (_packetHeader->sourceAddr < 0)){
		printf("Packet received from unknown Node \n");
	}
	else{
		printf("Packet received from Node %d\n", _packetHeader->sourceAddr);
	}

	/* Read TTL */
	printf("TTL = %d\n", _packetHeader->TTL);

	/* Check packet type */

	switch(_packetHeader->type){
		case DATA:          printf("Packet type: Data\n");	break;
		case ADVERTISEMENT: printf("Packet type: Adv\n");	break;
		case ACK:	    printf("Packet type: Ack\n");	break;
		default:  	    printf("Invalid packet type\n");	break;
	}

	/* Check packet header flags */
	
	if((_packetHeader->packetFlags & 0x01) != 0){
		_headerFlags->ackFlag = 1;
		printf("Ack is required\n");
	}
	else{
		_headerFlags->ackFlag = 0;
		printf("No Ack is required\n");
	}

	if((_packetHeader->packetFlags & 0x02) != 0){
		_headerFlags->lastPacket = 1;
		printf("Last packet in data stream\n");
	}
	else{
		_headerFlags->lastPacket = 0;
		printf("More packets in data stream \n");
	}

	/* Check Packet ID */
	printf("Packet ID: %d\n", _packetHeader->PID);

	/* Display checksum */
	printf("Checksum: %d\n", _packetHeader->checksum);

	printf("\n");
}



/*
 * Function:	displayPacket()
 * Description:	Displays recieved data and its associated header field information
 * Parameters: 	
 * 		char *receivedData: Points to payload portion of received packet
 *		const struct packetHeader *pHeader: Points to packet header structure of received packet 
 * Returns:	Nothing
 */
void displayPacket(char *receivedData, const struct packetHeader *pHeader){

	/* Display data */
	printf("Packet received:\n Source: %u\tPacket type: %u\t"
				 "Number of hops: %u\t", pHeader->sourceAddr, pHeader->type, 255 -  pHeader->TTL + 1);
	printf("Data received:\t");

	printf("%s", receivedData);
	
	printf("\n\n\n");
}



/*
 * Function:	calculateChecksum()
 * Description:	Calculates checksum
 * Parameters: 	
 * 		const uint8_t *payload: Points to the payload portion of packet being process
 * Returns:	result of checksum
 */
uint8_t calculateChecksum(const uint8_t *payload){

	uint8_t checksum = 0;

	for(int i = 0; i < 24; i++){
		checksum += countSetBits(payload[i]);	// Calculate checksum
	}

	return checksum;
}



/*
 * Function:	countSetBits()
 * Description:	Counts the '1' bits for a given byte
 * Parameters: 	
 * 		uint8_t n: Number to be calculated
 * Returns:	Total count of '1' bits
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
 * Function:	setHeaderValues()
 * Description:	Sets packet header fields
 * Parameters: 	
 * 		struct packetHeader *pHeader: Points to packet header structure of packet to be transmitted
 *		uint8_t destAddr: Destination node
 *		uint8_t gateway: Gateway node
 *		uint8_t sourceAddr: Source node
 *		uint8_t TTL: Time to live 
 *		uint8_t type: Packet type
 *		uint8_t packetFlags:
 *		uint8_t PID: Packet ID
 * Returns:	Nothing
 */
void setHeaderValues(struct packetHeader *pHeader, uint8_t destAddr, uint8_t gateway, uint8_t sourceAddr, uint8_t TTL, uint8_t type, uint8_t packetFlags, uint8_t PID){

	pHeader->destAddr = destAddr;
	pHeader->gateway = gateway;
	pHeader->sourceAddr = sourceAddr;
	pHeader->TTL = TTL;
	pHeader->type = type;
	pHeader->packetFlags = packetFlags;
	pHeader->PID = PID;
	pHeader->checksum = 0;	// Set to zero since it will be calculated later
}



/*
 * Function:	broadcastRoutingTable()
 * Description:	Prepars packet for advertisement
 * Parameters: 	
 * 		struct packetHeader *pHeader: Points to packet header structure of advertisement packet to be broadcasted
 *		const uint8_t* rTable: Points to routing table
 *		uint8_t* _packet: Points to advertisement packet to be broadcasted
 * Returns:	Nothing
 */
void broadcastRoutingTable(struct packetHeader *pHeader, const uint8_t* rTable, uint8_t* _packet){

	pHeader->destAddr = 0;
	pHeader->gateway = 0;
	pHeader->sourceAddr = MYADDRESS;
	pHeader->TTL = 255;
	pHeader->type = ADVERTISEMENT;
	pHeader->packetFlags = 0b0000;
	pHeader->PID = 0;
	pHeader->checksum = 0;

	assemblePacket((char*)rTable, _packet, pHeader);
}



/*
 * Function:	updateRoutingTable()
 * Description:	Updates routing table
 * Parameters: 	
 * 		const uint8_t *rTable: Points to received routing table 
 *		uint8_t sourceAddr: Source node that broadcasted the routing table
 * Returns:	Nothing
 */
void updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr){

	uint8_t gWay[12]  = {0};
	uint8_t nHops[12] = {0};
	
	/* Disassembling Incoming Routing Table*/
	for(int i = 0; i < 24; i++){

		if(i < 12){
		
			gWay[i] = rTable[i];
		}
		else{
		
			nHops[i - 12] = rTable[i];
		}
	}
	routingTable[sourceAddr - 1] = sourceAddr;
	routingTable[sourceAddr - 1 + 12] = 1;
	
	for(int i = 0; i < 12; i++){
		
		/* Skip entry if it contains MYADDRESS, sourceAddr, common neighbours or empty */
		if((i == MYADDRESS - 1) || (i == sourceAddr - 1) || (routingTable[i]  == gWay[i])){
			continue;
		}
		else if(gWay[i] != 0){
			
			// Update gateway only if it provides less number of hops
			if(nHops[i] <= routingTable[i+12]){
	
				routingTable[i]   = sourceAddr;	// Set neighbour as gateway to the non-common neighbours
				routingTable[i+12]= nHops[i] + 1; // Update number of hops
			}		
		}
		
	}
		
	advCounter[sourceAddr - 1]++;	// Increment counter for advertising node
}



/*
 * Function:	displayRoutingTable()
 * Description:	Displays routing table on terminal
 *		advCounter is also displayed to trace the activity of neighbouring nodes
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
 void displayRoutingTable(void){

	printf("Routing Table:\nNode\tGateway\tHops\tadvCounter\n");
	
  for(uint8_t i = 0; i < 12; i++){
		
	printf("%d\t%d\t%d\t%d\n", i+1, routingTable[i], routingTable[i+12], advCounter[i]);

  }
  printf("\n");
}



/*
 * Function:	PTX_Mode()
 * Description:	Flushes TX FIFO and sets transceiver in transmitter mode
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void PTX_Mode(void){

	hal_nrf_flush_tx();				// Flush TX FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PTX);	// Set module in transmitter mode

}



/*
 * Function:	PRX_Mode()
 * Description:	Sets transceiver in receiver mode
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void PRX_Mode(void){

	//hal_nrf_flush_rx();				// Flush Rx FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PRX);	// Set module in receiver mode

}



/*
 * Function:	transmitData()
 * Description: Transmits packet	
 * Parameters: 	uint8_t* _packet: Points to packet to be transmitted
 * Returns:	Nothing
 */
void transmitData(uint8_t* _packet){

	CE_LOW();
	microDelay(200);
	PTX_Mode();						// Set transceiver to transmitter mode
	microDelay(200);
	hal_nrf_write_tx_pload(_packet , PACKETLENGTH);		// Load packet
	CE_HIGH();						// Transmit packet
	microDelay(50);						// Minmum 10 microseconds delay required
	CE_LOW();						// Switch to standby-I mode
	microDelay(200);
}




/*
 * Function:	deleteInactiveNodes()
 * Description: Deletes unreachable/inactive nodes from the routing table
 * Parameters: 	Nothing
 * Returns:	Nothing
 */
void deleteInactiveNodes(void){
	
	for (uint8_t i = 0; i < 12; i++){
	
    		/* Check for zero counts */	
		if (advCounter[i] == 0){
			
			/* Delete all nodes associated with inactive node */
			for(uint8_t j = 0; j < 12; j++){

				if(routingTable[j] == i+1){
                   
					routingTable[j] = 0;		 // Set gateway to zero
					routingTable[j+12] = 255;	 // Set number of hops to 255				
				}
			}
		}
  	}
	/* Reset counters */
	for (uint8_t i = 0; i < 12; i++){
		advCounter[i] = 0; 	
	}   
}




/*
 * Function:	enqueue()
 * Description: Puts a new received packet to the rear of the queue
 * Parameters: 	uint8_t *rxBytes: Points to the new received packet to be enqueued
 * Returns:	Nothing
 */ 
void enqueue (uint8_t *rxBytes) {
  
	if (rxPacketsQ.Q_Counter != MAX_QUEUE_SIZE){
      
        	for (int i=0; i < 32; i++){
        
            		*(rxPacketsQ.rear + i) = *(rxBytes + i);
	    	} 
 
        	rxPacketsQ.rear = rxPacketsQ.receivedBytes + (rxPacketsQ.rear - rxPacketsQ.receivedBytes + 32) % MAX_QUEUE_SIZE;
      
 
        	rxPacketsQ.Q_Counter += 32;
    	}
    	else{
    		printf ("Queue is full\n");
		return;
    	}
}



/*
 * Function:	dequeue()
 * Description: Retrieves a packet from the front of the queue
 * Parameters: 	uint8_t *rxBytes: Points to the packet at the front of the queue to be dequeued
 * Returns:	0 for success, -1 for failure
 */  
int8_t dequeue (uint8_t *rxBytes){

	if (rxPacketsQ.Q_Counter == 0){
      
		printf ("Queue is empty\n");
		return -1;
	}
	else{
		for (int i = 0; i < 32; i++){            
			*(rxBytes+i) = *(rxPacketsQ.front + i);
			*(rxPacketsQ.front + i) = 0;
		} 
        rxPacketsQ.front = rxPacketsQ.receivedBytes + (rxPacketsQ.front - rxPacketsQ.receivedBytes + 32) % MAX_QUEUE_SIZE;
        rxPacketsQ.Q_Counter -= 32;
    }
    return 0;
}
