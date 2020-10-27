/*
 * Transceiver.h
 *
 *  Created on: Oct 12, 2020
 *      Author: Shamseddin Elmasri
 */

#ifndef INC_TRANSCEIVER_H_
#define INC_TRANSCEIVER_H_

#include "main.h"

#include "Transceiver.h"
#include "hal_nrf.h"
#include "hal_nrf_reg.h"
#include "nordic_common.h"

#define	CE	GPIO_PIN_9
#define	CSN	GPIO_PIN_8
#define	IRQ	GPIO_PIN_0

#define	MYADDRESS	1
#define PACKETLENGTH 32

// RF_SETUP register bit definitions for nRF24L01+
#define CONT_WAVE	7		/**< RF_SETUP register bit 7 */
#define RF_DR_LOW 	5		/**< RF_SETUP register bit 5 */

extern SPI_HandleTypeDef hspi2;

struct packetHeader{
	uint8_t destAddr;					// Destination address
	uint8_t sourceAddr;					// Source address
	uint8_t TTL;						// Time to live
	uint8_t type;						// Packet type
	uint8_t packetFlags;				// Ack, Last packet, ... more to be added
	uint8_t PID;						// Packet ID
	uint8_t checksum;
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
    DATA_RATE_250KBPS,		  /**< Datarate set to 250 Kbps  */
	DATA_RATE_1MBPS,          /**< Datarate set to 1 Mbps  */
    DATA_RATE_2MBPS           /**< Datarate set to 2 Mbps  */
} dataRate;

uint8_t spiTransaction(uint8_t *tx, uint8_t *rx, uint8_t nOfBytes);
void readAllRegisters(void);
void transceiverPrxInit(void);
void transceiverPtxInit(void);
void CSN_LOW(void);
void CSN_HIGH(void);
void CE_LOW(void);
void CE_HIGH(void);
void setDataRate(dataRate dRate);
struct packetHeader disassemblePacket(uint8_t *receivedData, uint8_t *receivedPacket);
void assemblePacket(char* payload, uint8_t* _packet);
uint8_t countSetBits(uint8_t n);
void displayHeader(const struct packetHeader *_packetHeader);
void displayData(uint8_t *receivedData);

uint8_t hal_nrf_rw(uint8_t value);

#endif /* INC_TRANSCEIVER_H_ */
