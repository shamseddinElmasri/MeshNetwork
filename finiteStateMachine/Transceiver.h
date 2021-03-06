/*
 * Transceiver.h
 *
 *  Created on: Oct 12, 2020
 *  Last modified: Dec 13, 2020
 *      Author: Shamseddin Elmasri
 */



#ifndef INC_TRANSCEIVER_H_
#define INC_TRANSCEIVER_H_

#include "main.h"

#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"

/************** Defines ******************/

// MCU/Transceiver Interface
#define	IRQ	GPIO_PIN_7
#define	CSN	GPIO_PIN_8
#define	CE	GPIO_PIN_9

#define	MYADDRESS	1	// Unique for each node
#define PACKETLENGTH	32
#define MAX_QUEUE_SIZE 	320	// 10 32-byte packets
#define MAX_NODE	12	// Maximum node address

// RF_SETUP register bit definitions for nRF24L01+
#define CONT_WAVE	7	// RF_SETUP register bit 7
#define RF_DR_LOW	5	// RF_SETUP register bit 5



/*********** Global TypeDefs **************/

extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef tim2;
extern TIM_HandleTypeDef tim17;



/************ Global Variables ************/

extern	uint8_t	routingTable[24];
extern	volatile uint8_t advCounter[12];	// Used to count advertising nodes activity
extern	char	txData[25];			// Payload to be transmitted (Data packet)
extern	char	ackMessage[25];			// Payload to be transmitted for Ack (Ack Packet)
extern 	uint8_t	txPacket[32];			// Data packet to be transmitted
extern	uint8_t	ackPacket[32];			// Ack packet to be transmitted
extern 	uint8_t advPacket[32];			// Advertisement packet to be transmitted



/************ Global Flags ************/

extern	volatile uint8_t broadcasting;		// Flag to indicate time for broadcasting
extern	volatile uint8_t secondsCounter;	// To create 20-sec period to delete inactive nodes
extern 	volatile uint8_t ackTransmitFlag;	// Raised when Ack is required by source node
extern	volatile uint8_t relayFlag;		// Raised after reassembling received packet in RELAY_PACKET_STATE
extern 	volatile uint8_t dataTransmitFlag;	// Raised in txPacket command when data packet is ready



/************ Structures ************/

struct packetHeader{
	uint8_t destAddr;	// Destination address
	uint8_t gateway;	// Gateway node address
	uint8_t sourceAddr;	// Source address
	uint8_t TTL;		// Time to live
	uint8_t type;		// Packet type
	uint8_t packetFlags;	// Ack, Last packet, ... more to be added
	uint8_t PID;		// Packet ID
	uint8_t checksum;
};


struct headerFlags{
	uint8_t ackFlag;
	uint8_t lastPacket;
};


struct queue 
{
    uint8_t Q_Counter;
    uint8_t * front;
    uint8_t * rear;
    uint8_t receivedBytes[320];
}rxPacketsQ;




/************ Enums ************/

/*
 *  enum for node state
 */
enum nodeState{
	START_STATE,
	PRX_STATE,
	CHECK_ADDRESS_STATE,
	CHECK_TYPE_STATE,
	RELAY_PACKET_STATE,
	PROCESS_PACKET_STATE,
	ACK_STATE,
	PTX_STATE
};


/*
 * enum for packet type
 */
enum packetType{

	DATA = 0,
	ADVERTISEMENT,
	ACK,
	PACKET_TYPE_MAX	
};


/*
 * enum for data rate (made for nRF24L01+)
 */
typedef enum {

	DATA_RATE_250KBPS,	// Datarate set to 250 Kbps
	DATA_RATE_1MBPS,        // Datarate set to 1 Mbps  
	DATA_RATE_2MBPS         // Datarate set to 2 Mbps  
  
}dataRate;


/************ Private Function Prototypes ************/

// Low Level Functions
void CSN_LOW(void);
void CSN_HIGH(void);
void CE_LOW(void);
void CE_HIGH(void);
void spi_init(void);
void timer2Init(void);
void timer17Init(void);
void microDelay(uint32_t delay);
void transceiverInit(void);
void spiTransaction(uint8_t *tx, uint8_t *rx, uint8_t nOfBytes);
void readAllRegisters(void);
void PTX_Mode();
void PRX_Mode();
void setDataRate(dataRate dRate);
uint8_t hal_nrf_rw(uint8_t value);

// High Level Functions
void disassemblePacket(struct packetHeader *_packetHeader, uint8_t *receivedData, uint8_t *receivedPacket);
void assemblePacket(const char* payload, uint8_t* _packet, struct packetHeader *pHeader);
uint8_t countSetBits(uint8_t n);
void processHeader(struct headerFlags *_headerFlags, const struct packetHeader *_packetHeader);
void displayPacket(char *receivedData, const struct packetHeader *_pHeader);
uint8_t calculateChecksum(const uint8_t *receivedData);
void setHeaderValues(struct packetHeader*, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void broadcastRoutingTable(struct packetHeader *pHeader, const uint8_t* rTable, uint8_t* _packet);
void updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr);
void displayRoutingTable(void);
void deleteInactiveNodes(void);
void transmitData(uint8_t*);
void enqueue (uint8_t *rxBytes);
int8_t dequeue (uint8_t *rxBytes);
void display_queue (void);

// Main Task Functions
void PRX_Init(void* data);
void PRX_Task(void *data);




#endif /* INC_TRANSCEIVER_H_ */
