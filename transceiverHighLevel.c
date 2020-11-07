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

uint8_t routingTable[255] = {0};
uint8_t advCounter[255] = {0};


//static uint8_t txPacket[32];
//static char txData[25] = "Shamseddin Elmasri 12345";


/*************High Level Functions***************/
/*
 *	Function to set RF data rate
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
 *	Function for disassembling a received packet
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
 *	Function for packet assembly
 */
void assemblePacket(const char* payload, uint8_t* _packet, struct packetHeader pHeader){

	uint8_t packetSize = sizeof(_packet);
	
	memset(_packet, 0, packetSize);								// Clear packet

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
		pHeader.checksum += countSetBits(payload[i]);			// Calculate check sum
	}
	_packet[31] = pHeader.checksum;											// Attached checksum byte
}


/*
 *	Function for abstracting received packet header fields
 */
struct headerFlags processHeader(const struct packetHeader *_packetHeader){

	struct headerFlags _headerFlags;

	/* Check destination address */
	if(_packetHeader->destAddr != MYADDRESS){

		printf("This packet is not for this node.\n");
		printf("Need to check routing table.\n");
		}

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
		case DATA: 					printf("Packet type: Data\n");		break;
		case ADVERTISEMENT: printf("Packet type: Adv\n"); 		break;
		case ACK: 					printf("Packet type: Ack\n");			break;
		default: 						printf("Invalid packet type\n");	break;
	}

	/* Check packet flags */

	if((_packetHeader->packetFlags & 0x01) != 0){
		_headerFlags.ackFlag = 1;
		printf("Ack is required\n");
	}
	else{
		_headerFlags.ackFlag = 0;
		printf("No Ack is required\n");
	}

	if((_packetHeader->packetFlags & 0x02) != 0){
		_headerFlags.lastPacket = 1;
		printf("Last packet in data stream\n");
	}
	else{
		_headerFlags.lastPacket = 0;
		printf("More packets in data stream \n");
	}

	/* Check Packet ID */

	printf("Packet ID: %d\n", _packetHeader->PID);

	/* Display checksum */

	printf("Checksum: %d\n", _packetHeader->checksum);

	printf("\n");

	return _headerFlags;
}


/*
 *	Function for displaying recieved data
 */
void displayData(uint8_t *receivedData){

	/* Display data */
	printf("Packet received:\t");

	for(int i = 0; i < 25; i++){
		printf("%c", (char)receivedData[i]);
	}
	printf("\n\n\n");
}


/*
 *	Function for calculating checksum
 */

uint8_t calculateChecksum(const uint8_t *payload){

	uint8_t checksum = 0;

	for(int i = 0; i < 24; i++){
			checksum += countSetBits(payload[i]);					// Calculate checksum
		}

	return checksum;
}



/*
 *	Function for counting set bits for a given number
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
 *	Function for setting packet header fields
 */
struct packetHeader setHeaderValues(uint8_t destAddr, uint8_t gateway, uint8_t sourceAddr, uint8_t TTL, uint8_t type, uint8_t packetFlags, uint8_t PID, uint8_t checksum){

	struct packetHeader pHeader = {destAddr, gateway, sourceAddr, TTL, type, packetFlags, PID, checksum};

	return pHeader;
}


/*
 * Function for preparing packet for advertisement
 */
void broadcastRoutingTable(const uint8_t* rTable, uint8_t* _packet){

	struct packetHeader pHeader = {0, 0, MYADDRESS, 0, ADVERTISEMENT, 0b0000, 0, 0};

	uint8_t packetSize = sizeof(_packet);

	memset(_packet, 0, packetSize);						// Clear packet


	_packet[0] = pHeader.destAddr;
	_packet[1] = pHeader.gateway;
	_packet[2] = pHeader.sourceAddr;
	_packet[3] = pHeader.TTL;
	_packet[4] = pHeader.type;
	_packet[5] = pHeader.packetFlags;
	_packet[6] = pHeader.PID;

	_packet[31] = calculateChecksum(rTable);

	assemblePacket((char*)rTable, _packet, pHeader);
}


/*
 *	Function for updating routing table
 */
void updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr){

	routingTable[sourceAddr] = sourceAddr;		// Set neighbour as gateway to itself

	for(int i = 1; i < 25; i++){
		/* Skip entry if it contains MYADDRESS, sourceAddr, common neighbours or empty */
		if((i == MYADDRESS) || (i == sourceAddr) || (routingTable[i]  == rTable[i]) || (rTable[i-1] == 0)){
			continue;
		}
		else if(rTable[i-1] != 0){
			routingTable[i] = sourceAddr;					// Set neighbour as gateway to its non-common neighbours
		}
	}
	advCounter[sourceAddr]++;
}


/*
 *	Function for setting transceiver as transmitter
 */
void PTX_Mode(void){

	hal_nrf_flush_tx();												// Flush TX FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PTX);	// Set module in receiver mode

}


/*
 *	Function for setting transceiver as receiver
 */
void PRX_Mode(void){

	hal_nrf_flush_rx();												// Flush Rx FIFO

	hal_nrf_set_operation_mode(HAL_NRF_PRX);	// Set module in receiver mode

}


/*
 *
 */
void transmitData(void){
}



