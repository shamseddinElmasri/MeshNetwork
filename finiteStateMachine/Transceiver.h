/*
 * Transceiver.h
 *
 *  Created on: Oct 12, 2020
 *      Author: Shamseddin Elmasri
 */



#ifndef INC_TRANSCEIVER_H_
#define INC_TRANSCEIVER_H_

#include "main.h"

#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"



#define	CE	GPIO_PIN_9
#define	CSN	GPIO_PIN_8
#define	IRQ	GPIO_PIN_0
#define PBUTTON	GPIO_PIN_1

#define	MYADDRESS	1
#define PACKETLENGTH	32


// Global TypeDefs
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef tim1;
extern TIM_HandleTypeDef tim17;

// Global Variables
extern	uint8_t	routingTable[24];
extern	uint8_t	advCounter[12];
extern	char	txData[25];
extern	char	ackMessage[25];
extern 	uint8_t	txPacket[32];
extern	uint8_t	ackPacket[32];
extern 	uint8_t advPacket[32];
extern	volatile uint8_t broadcasting;
extern	volatile uint8_t secondsCounter;
extern 	uint8_t	ackTransmitFlag;
extern	uint8_t relayFlag;
extern 	uint8_t dataTransmitFlag;

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
 * enum for node address
 */
enum nodeAddr{
	NODE_1	= 1,
	NODE_2,
	NODE_3,
	NODE_4,
	NODE_5,
	NODE_6,
	MAX_NODE
}gateway;

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

	DATA_RATE_250KBPS,	/**< Datarate set to 250 Kbps  */
	DATA_RATE_1MBPS,        /**< Datarate set to 1 Mbps  */
	DATA_RATE_2MBPS         /**< Datarate set to 2 Mbps  */
  
}dataRate;


// RF_SETUP register bit definitions for nRF24L01+
#define CONT_WAVE	7	/**< RF_SETUP register bit 7 */
#define RF_DR_LOW	5	/**< RF_SETUP register bit 5 */

// Low Level Functions
void CSN_LOW(void);
void CSN_HIGH(void);
void CE_LOW(void);
void CE_HIGH(void);
void spi_init(void);
void timer1Init(void);
void timer17Init(void);
void microDelay(uint32_t delay);
void transceiverInit(void);
uint8_t spiTransaction(uint8_t *tx, uint8_t *rx, uint8_t nOfBytes);
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
void displayPacket(uint8_t *receivedData, const struct packetHeader *_pHeader);
uint8_t calculateChecksum(const uint8_t *receivedData);
void setHeaderValues(struct packetHeader*, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void broadcastRoutingTable(struct packetHeader *pHeader, const uint8_t* rTable, uint8_t* _packet);
void updateRoutingTable(const uint8_t *rTable, uint8_t sourceAddr);
void displayRoutingTable(void);
void deleteInactiveNodes(void);
void transmitData(uint8_t*);

void PRX_Task(void *data);
void PRX_Init(void* data);



#endif /* INC_TRANSCEIVER_H_ */
