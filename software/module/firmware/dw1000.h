#ifndef __DW1000_H
#define __DW1000_H

#include <stdint.h>
#include "system.h"

/******************************************************************************/
// General defines for the DW1000
/******************************************************************************/
// Configure how long after a reception the ack is sent
#define DW1000_ACK_RESPONSE_TIME 3

// Whether the OTP has been setup (or we expect it to be)
// This is where constants for calibration of a given device and its
// radios should go.
#define DW1000_USE_OTP 0

// The antenna delays are set to 0 because our other calibration takes care of this.
#define DW1000_ANTENNA_DELAY_TX 0
#define DW1000_ANTENNA_DELAY_RX 0

// How to split up the Tx+Rx delay
#define DW1000_DELAY_TX 44
#define DW1000_DELAY_RX 56

// Constant for the number of UWB channels
#define DW1000_NUM_CHANNELS 8

// Length of addresses in the system
#define EUI_LEN 8

// Default from original PolyPoint code
#define DW1000_DEFAULT_XTALTRIM 15

// Param for making sure the application doesn't deadlock.
// This is the number of times we try to read the status/ID register on the
// DW1000 before giving up and reseting the dw1000.
#define DW1000_NUM_CONTACT_TRIES_BEFORE_RESET 15

// Number of consecutive DW1000 interrupts we handle before resetting the chip.
// The DW1000 can get in a bad state and just continuously assert the interrupt
// line (this may happen because it thinks we switched the interrupt polarity).
#define DW1000_NUM_CONSECUTIVE_INTERRUPTS_BEFORE_RESET 10

// In case we don't have a value calculated and stored.
// This represents the sum of the TX and RX delays.
#define DW1000_DEFAULT_CALIBRATION 32900

// Print debug output (can mess with correctness of code due to introduction of delays)
//#define DEBUG_OUTPUT_UART 1

// Enable wake-up either over CS or the WAKE-UP pin
#define DW1000_WAKEUP_CS

/******************************************************************************/
// Timing defines for this particular MCU
/******************************************************************************/

#define APP_US_TO_DEVICETIMEU64(_microsecu) \
	((uint64_t) ( ((_microsecu) / (double) DWT_TIME_UNITS) / 1e6 ))

#define APP_DEVICETIME_TO_USU64(_dw_units) \
	((uint64_t) ( ((_dw_units) * (double) DWT_TIME_UNITS) * 1e6 ))

#define SPI_US_PER_BYTE        0.94	// 0.94 @ 8mhz, 0.47 @ 16mhz
#define SPI_US_BETWEEN_BYTES   0.25	// 0.25 @ 8mhz, 0.30 @ 16mhz
#define SPI_SLACK_US           350	// 200 @ 8mhz, 150 @ 16mhz
#define DW_DELAY_FROM_PKT_LEN(_len) \
	((uint32_t)(APP_US_TO_DEVICETIMEU64(SPI_US_PER_BYTE * (_len) + SPI_US_BETWEEN_BYTES * (_len) + SPI_SLACK_US + dw1000_preamble_time_in_us()) >> 8))


#define DW_DELAY_FROM_US(_us) ((uint32_t)(APP_US_TO_DEVICETIMEU64((_us)) >> 8))

#define ANC_FINAL_RX_PKT_TIME_US	398  // 8mhz: 398; 16mhz: 256
#define ANC_FINAL_RX_PKT_MEMCPY_TIME_US	 79  // 8mhz: 120
#define ANC_FINAL_RX_PKT_PRINTF_TIME_US	150
#define ANC_FINAL_RX_PKT_GUARD_US	100
#ifdef DW_DEBUG
#define ANC_FINAL_RX_TIME_ON_TAG	(\
		ANC_FINAL_RX_PKT_TIME_US +\
		ANC_FINAL_RX_PKT_MEMCPY_TIME_US +\
		ANC_FINAL_RX_PKT_PRINTF_TIME_US +\
		ANC_FINAL_RX_PKT_GUARD_US\
		)
#else
#define ANC_FINAL_RX_TIME_ON_TAG	(\
		ANC_FINAL_RX_PKT_TIME_US +\
		ANC_FINAL_RX_PKT_MEMCPY_TIME_US +\
		ANC_FINAL_RX_PKT_GUARD_US\
		)
#endif

#define ANC_FINAL_INITIAL_DELAY_HACK_VALUE 300	// See note in anchor.c

// How long it takes to go from SLEEP mode on the DW1000 to ready to range.
// 5 ms for clock stabilization and some other error.
#define DW1000_WAKEUP_DELAY_US 5100

/******************************************************************************/
// Constants
/******************************************************************************/

#define SPEED_OF_LIGHT 299702547.0

/******************************************************************************/
// Data structs for 802.15.4 packets
/******************************************************************************/

struct ieee154_header_broadcast {
	uint8_t frameCtrl[2];          //  frame control bytes 00-01
	uint8_t seqNum;                //  sequence_number 02
	uint8_t panID[2];              //  PAN ID 03-04
	uint8_t destAddr[2];
	uint8_t sourceAddr[EUI_LEN];
};

struct ieee154_header_unicast {
	uint8_t frameCtrl[2];          //  frame control bytes 00-01
	uint8_t seqNum;                //  sequence_number 02
	uint8_t panID[2];              //  PAN ID 03-04
	uint8_t destAddr[EUI_LEN];
	uint8_t sourceAddr[EUI_LEN];
};

struct ieee154_footer {
	uint8_t fcs[2];                // We allow space for the CRC as it is
                                   // logically part of the message. However
                                   // DW100 TX calculates and adds these bytes.
};


/******************************************************************************/
// Helper macros for working with the DW1000
/******************************************************************************/

#define DW_TIMESTAMP_TO_UINT64(dwts) \
	(((uint64_t) dwts[0]) << 0)  | \
	(((uint64_t) dwts[1]) << 8)  | \
	(((uint64_t) dwts[2]) << 16) | \
	(((uint64_t) dwts[3]) << 24) | \
	(((uint64_t) dwts[4]) << 32)


/******************************************************************************/
// Structs and what-not for control flow throughout the DW1000 code
/******************************************************************************/

// Return values for our DW1000 library errors
typedef enum {
	DW1000_NO_ERR = 0,
	DW1000_COMM_ERR,
	DW1000_BUSY,
	DW1000_WAKEUP_ERR,
	DW1000_WAKEUP_SUCCESS,
} dw1000_err_e;


/******************************************************************************/
// Structs for data stored in the flash
/******************************************************************************/

#define PROGRAMMED_MAGIC 0x77AA38F9
#define OFFSET_EUI     ( sizeof(uint32_t) )
#define OFFSET_CALIB   ( OFFSET_EUI + 8 * sizeof(uint8_t))

typedef struct {
	uint32_t magic;                    // Known special magic value that verifies this struct was written
	uint8_t  eui[EUI_LEN];
	uint16_t calibration_values[3][3]; // TX+RX delays for each channel
} __attribute__ ((__packed__)) dw1000_programmed_values_t;


/******************************************************************************/
// Function prototypes
/******************************************************************************/

// Utility
int  dwtime_to_millimeters (double dwtime);
void insert_sorted (int arr[], int new, unsigned end);
uint16_t dw1000_preamble_time_in_us();
uint32_t dw1000_packet_data_time_in_us(uint16_t data_len);

// Helpers
void uart_write(uint32_t length, const uint8_t* tx);
void uart_write_message(uint32_t length,const char* msg);
void uart_write_debug(uint32_t length,const char* msg);

// Main API
dw1000_err_e  dw1000_init ();
void          dw1000_spi_fast ();
void          dw1000_spi_slow ();
dw1000_err_e  dw1000_configure_settings ();
void          dw1000_reset_soft ();
void		  dw1000_reset_hard ();
void          dw1000_choose_antenna (uint8_t antenna_number);
void          dw1000_read_eui (uint8_t *eui_buf);
uint64_t      dw1000_get_tx_delay (uint8_t channel_index, uint8_t antenna_index);
uint64_t      dw1000_get_rx_delay (uint8_t channel_index, uint8_t antenna_index);
uint8_t*      dw1000_get_txrx_delay_raw ();
void          dw1000_sleep ();
dw1000_err_e  dw1000_wakeup ();
dw1000_err_e  dw1000_force_wakeup ();
void          dw1000_enterINIT ();
void          dw1000_exitINIT ();
void          dw1000_update_channel (uint8_t chan);
void          dw1000_reset_configuration ();
uint64_t      dw1000_correct_timestamp(uint64_t dw_timestamp);
uint64_t      dw1000_readrxtimestamp();
uint64_t      dw1000_setdelayedtrxtime(uint32_t delay_time);
uint64_t      dw1000_gettimestampoverflow();
void      	  dw1000_calculatediagnostics();
uint32_t 	  dw1000_get_status_register();

// for main.c
void          dw1000_interrupt_fired ();

#endif
