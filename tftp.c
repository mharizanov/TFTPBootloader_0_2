/* Name: tftp.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: eboot
 * Function: tftp implementation and flasher
 * Version: 0.1 tftp / flashing functional
 */

#include <avr/boot.h>
#include <avr/pgmspace.h>

#include "net.h"
#include "w5100_reg.h"
#include "tftp.h"
#include "main.h"
#include "validate.h"

#include "debug.h"



// TFTP OP codes
#define ERROR_UNKNOWN     0
#define ERROR_INVALID     1
#define ACK               2
#define ERROR_FULL        3
#define FINAL_ACK         4   // Like an ACK, but for the final data packet.?????
#define INVALID_IMAGE     5

#define TFTP_DATA_SIZE    512

#define MAX_ADDR          0x7000 // For 328 with 2Kword bootloader

#define TFTP_HEADER_SIZE  12
#define TFTP_OPCODE_SIZE  2
#define TFTP_BLOCKNO_SIZE 2
#define TFTP_MAX_PAYLOAD  512
#define TFTP_PACKET_MAX_SIZE (  TFTP_HEADER_SIZE + TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE + TFTP_MAX_PAYLOAD )

uint16_t lastPacket = 0;
uint8_t DownloadStarted=0;

/* Opcode?: tftp operation is unsupported. The bootloader only supports 'put' */
#define TFTP_OPCODE_ERROR_LEN 12
char tftp_opcode_error_packet[]   PROGMEM = "\0\5"  "\0\4"  "Opcode?";

/* Full: Binary image file is larger than the available space. */
#define TFTP_FULL_ERROR_LEN 15
char tftp_full_error_packet[]     PROGMEM = "\0\5"  "\0\3"  "Flash Full";

/* General catch-all error for unknown errors */
#define TFTP_UNKNOWN_ERROR_LEN 10
char tftp_unknown_error_packet[]  PROGMEM = "\0\5"  "\0\0"  "Error";

/* Invalid image file: Doesn't look like a binary image file */
#define TFTP_INVALID_IMAGE_LEN 18
char tftp_invalid_image_packet[]  PROGMEM =  "\0\5"  "\0\0"  "Invalid image";

uint8_t processPacket(uint16_t packetSize) {
  uint8_t buffer[TFTP_PACKET_MAX_SIZE];
  uint16_t readPointer;
  uint16_t readPointerBase;
  uint16_t writeAddr;
    // Transfer entire packet to RAM
  uint8_t* bufPtr = buffer;
  uint16_t count;
  trace("----------------------------------------------------------\r\n");
  trace("Starting ProcessPacket\r\n");
  trace("Data in buffer: ");
  tracenum(packetSize);
  trace(" bytes\r\n");
  if(packetSize==0x800) trace("OVERFLOW!!!\r\n");
  readPointer=netReadWord(REG_S3_RX_RD0);
  readPointerBase=readPointer;
  //readPointer = readPointerOrg + S3_RX_START;//Get the read pointer (first byte) from W5100
  if (readPointer > 0x800){ //2048
	  trace("Applying modulo to readPointer\r\n");
	  trace("Read Pointer:");
	   tracenum(readPointer);
	   trace("\r\n");
	   readPointer=readPointer & 0x7FF;
  }

  trace("Read Pointer:");
  tracenum(readPointer+ S3_RX_START);
  uint16_t UDPPacketSize = (netReadReg(readPointer+ S3_RX_START+6)<<8) + netReadReg(readPointer+ S3_RX_START+7)+8;
  trace("\r\n");
  trace("Address for len:");
  tracenum(readPointer+ S3_RX_START+6);
  trace("\r\n");
  trace("TFTP Packet Size:");
  tracenum(UDPPacketSize-12);
  trace("\r\n");

  //Read whole packet
  for (count=UDPPacketSize+1; count--;) {
	  *bufPtr++ = netReadReg( S3_RX_START+readPointer);//Read value from W5100
	  readPointer++;
    if (readPointer == 0x800) readPointer = 0; //If end of pointer reached, initialize pointer
  }
  trace("New Read Pointer:");
  tracenum(UDPPacketSize+readPointerBase);
  trace("\r\n");
  netWriteWord(REG_S3_RX_RD0,UDPPacketSize+readPointerBase); // Write back new pointer, -1 because of ++ at end of for loop
  netWriteReg(REG_S3_CR,CR_RECV);
  while(netReadReg(REG_S3_CR));
  trace("bytestoread:");
  tracenum(netReadWord(REG_S3_RX_RSR0));
  trace("\r\n");

  // Dump packet
  bufPtr = buffer;

  for (count=UDPPacketSize; count--;) {
    uint16_t val = *bufPtr++;
    val |= (*bufPtr++) << 8;
  }


  // Set up return IP address and port
  uint8_t i;
  for (i=0; i<6; i++){
	  netWriteReg(REG_S3_DIPR0+i,buffer[i]);
  }

  // Parse packet
  uint16_t tftpOpcode = (buffer[8]<<8) + buffer[9];
  uint16_t tftpBlock = (buffer[10]<<8) + buffer[11];
  TRACE("This is block n°");
  tracenum(tftpBlock);
  TRACE("\r\n");
  uint8_t returnCode = ERROR_UNKNOWN;
  uint16_t packetLength;      
  trace("Opcode=");
  tracenum(tftpOpcode);
  trace("\r\n");

  switch (tftpOpcode) {
    case TFTP_OPCODE_RRQ: // Read request
    default:
      tracenum(tftpOpcode);
      TRACE("is invalid opcode\r\n");
      // Invalid - return error
      returnCode = ERROR_INVALID;
      break;
      
    case TFTP_OPCODE_WRQ: // Write request
      TRACE("Write request\r\n");
      netWriteReg(REG_S3_CR,CR_RECV);
      netWriteReg(REG_S3_CR,CR_CLOSE);
      do {
        netWriteReg(REG_S3_MR,MR_UDP);
        netWriteReg(REG_S3_CR,CR_OPEN);
        netWriteWord(REG_S3_PORT0,(buffer[4]<<8) | ~buffer[5] ); // Generate a 'random' TID (RFC1350)
        if (netReadReg(REG_S3_SR) != SOCK_UDP)
          netWriteReg(REG_S3_CR,CR_CLOSE);
      } while (netReadReg(REG_S3_SR) != SOCK_UDP);
      TRACE("Changed to port ");
      tracenum((buffer[4]<<8) | (buffer[5]^0x55));
      TRACE("\r\n");
      lastPacket = 0;
      returnCode = ACK; // Send back acknowledge for packet 0
      break;
      
    case TFTP_OPCODE_DATA:
      packetLength = UDPPacketSize;
      lastPacket = tftpBlock;
      writeAddr = (tftpBlock-1) << 9; // Flash write address for this block
      TRACE("Writing Data for block n°");
      tracenum(lastPacket);
      TRACE("\r\n");

      
      if ((writeAddr+packetLength) > MAX_ADDR) {
        // Flash is full - abort with an error before a bootloader overwrite occurs
        //
        // Application is now corrupt, so do not hand over.
        //
    	  TRACE("Flash is full!\r\n");
        returnCode = ERROR_FULL;
      } else {
        TRACE("Writing data from address ");
        tracenum(writeAddr);
        TRACE("\r\n");
        
        uint8_t* pageBase = buffer + 12; // Start of block data
        uint16_t offset = 0; // Block offset

        // Round up packet length to a full flash sector size
        while (packetLength % SPM_PAGESIZE) packetLength++;

        if (writeAddr == 0) {
          // First sector - validate
          if (!validImage(pageBase)) {
            returnCode = INVALID_IMAGE;
            trace("Invalid Image!\r\n");
            break;
          }
        }

        // Flash packets
        for (offset=0; offset < packetLength;) {
          //TRACE("Should be writing...");
          uint16_t writeValue = (pageBase[offset]) | (pageBase[offset+1]<<8);
          boot_page_fill(writeAddr+offset,writeValue);
          offset+=2;
          if (offset % SPM_PAGESIZE == 0) {
            boot_page_erase(writeAddr + offset - SPM_PAGESIZE);
            boot_spm_busy_wait();
            boot_page_write(writeAddr + offset - SPM_PAGESIZE);
            boot_spm_busy_wait();
            boot_rww_enable();
          }
        }

        if ((UDPPacketSize-12) < TFTP_DATA_SIZE) {
          // Flash is complete
          // Hand over to application
        	TRACE("Flash is complete\r\n");
          returnCode = FINAL_ACK;
        } else {
          returnCode = ACK;
        }
      }
      break;
      
    case TFTP_OPCODE_ACK: // Acknowledgement
      TRACE("Ack\r\n");
      break;
    
    case TFTP_OPCODE_ERROR: // Error signal
      TRACE("Error\r\n");
      break;
    
  }
  return returnCode;
}

void sendResponse(uint16_t response) {
  uint8_t txBuffer[100];
  uint8_t *txPtr = txBuffer;
  uint8_t packetLength;
  uint16_t writePointer;
  writePointer = netReadWord(REG_S3_TX_WR0) + S3_TX_START;

  switch (response) {
    default:
    case ERROR_UNKNOWN:
      // Send unknown error packet
      packetLength = TFTP_UNKNOWN_ERROR_LEN;
      memcpy_P(txBuffer,tftp_unknown_error_packet,packetLength);
      break;

    case INVALID_IMAGE:
        // Send bad image packet
          packetLength = TFTP_INVALID_IMAGE_LEN;
          memcpy_P(txBuffer,tftp_invalid_image_packet,packetLength);
          break;

    case ERROR_INVALID:
      // Send invalid opcode packet
      packetLength = TFTP_OPCODE_ERROR_LEN;
      memcpy_P(txBuffer,tftp_opcode_error_packet,packetLength);
      break;

    case ERROR_FULL:
      // Send unknown error packet
      packetLength = TFTP_FULL_ERROR_LEN;
      memcpy_P(txBuffer,tftp_full_error_packet,packetLength);
      break;

    case ACK:
    	TRACE("Send ACK (Not Final)\r\n");
    case FINAL_ACK:
    	TRACE("Send ACK ");
      tracenum(lastPacket);
      TRACE("\r\n");
      packetLength = 4;
      *txPtr++ = TFTP_OPCODE_ACK >> 8;
      *txPtr++ = TFTP_OPCODE_ACK & 0xff;
      *txPtr++ = lastPacket >> 8; //lastPacket is block code
      *txPtr = lastPacket & 0xff;
      break;
  }
  txPtr = txBuffer;
  while (packetLength--) {
    netWriteReg(writePointer++, *txPtr++);
    if (writePointer == S3_TX_END) writePointer = S3_TX_START;
  }
  netWriteWord(REG_S3_TX_WR0, writePointer - S3_TX_START);
  netWriteReg(REG_S3_CR,CR_SEND);
  while (netReadReg(REG_S3_CR));
  TRACE("Response sent\r\n");
}


void tftpInit() {
  // Open socket (Loop until success)
  do {
	//Write TFTP Port
    netWriteWord(REG_S3_PORT0,TFTP_PORT);
    //Write mode
    netWriteReg(REG_S3_MR,MR_UDP);
    //Open Socket
    netWriteReg(REG_S3_CR,CR_OPEN);
    //Read Status
    if (netReadReg(REG_S3_SR) != SOCK_UDP)
     //Close Socket if it wasn't initialized correctly
      netWriteReg(REG_S3_CR,CR_CLOSE);
  } while (netReadReg(REG_S3_SR) != SOCK_UDP); //if socket correctly opened continue
}

uint8_t tftpPoll() {
  uint8_t response = ACK;
  uint16_t packetSize = netReadWord(REG_S3_RX_RSR0);
  if (packetSize) {

    response = processPacket(packetSize); //Process Packet and get TFTP response code
    sendResponse(response); //send the response
    DownloadStarted=1;
    eeprom_write_byte(EEPROM_IMG_STAT,EEPROM_IMG_BAD_VALUE); //Flagging the firmware in flash as bad for the moment
    ResetTick(); //Reset tick counter
    if ((response == FINAL_ACK)) {
        netWriteReg(REG_S3_CR,CR_CLOSE);
        eeprom_write_byte(EEPROM_IMG_STAT,EEPROM_IMG_OK_VALUE); //Flagging the firmware in flash as good
  	  trace("img_stat");
  	  tracenum(eeprom_read_byte(EEPROM_IMG_STAT));
  	  trace("\r\n");
        trace("TFTP Complete\r\n");
        return 0; // Complete and start app

      }
  }


  //Will timeout if download has failed
    if (timedOut() && DownloadStarted) {
        netWriteReg(REG_S3_CR,CR_CLOSE);
        trace("Download interrupted!\r\n");
        trace("Re-initialising TFTP server...\r\n");
        DownloadStarted=0;
        boot_page_erase_safe(0);//erase first page because the user app is corrupted
        tftpInit();// Restart TFTP
        return 1; // tftp continues

      }

  //Will never timeout if download is in progress
  if (timedOut() && !DownloadStarted && eeprom_read_byte(EEPROM_IMG_STAT) == EEPROM_IMG_OK_VALUE) {
      netWriteReg(REG_S3_CR,CR_CLOSE);
      trace("Timeout!\r\n");
      return 0; // Complete and start app

    }
  return 1; // tftp continues
}
