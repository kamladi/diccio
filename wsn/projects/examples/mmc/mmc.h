/* ***********************************************************************
**
**  Copyright (C) 2006  Jesper Hansen <jesper@redegg.net> 
**
**
**  Interface functions for MMC/SD cards
**
**  File mmc_if.h
**
*************************************************************************
**
**  This program is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License
**  as published by the Free Software Foundation; either version 2
**  of the License, or (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software Foundation, 
**  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
*************************************************************************/

/** \file mmc_if.h
	Simple MMC/SD-card functionality
*/

/* Modified by Hsin-Mu (Michael) Tsai <hsinmu@cmu.edu> to address a "write busy" bug.      July 2008 */

#ifndef __MMC_IF_H__
#define __MMC_IF_H__


/** @name MMC/SD Card I/O ports
*/
//@{
#define SPI_PORT	PORTB
#define SPI_DDR		DDRB
#define SPI_PIN		PINB

#define MMC_CS_PORT	PORTE
#define MMC_CS_DIR	DDRE

#define MMC_DETECT_PORT	PORTB
#define MMC_DETECT_DDR  DDRB 
#define MMC_DETECT_PIN  PINB	
//@}

/** @name MMC/SD Card I/O lines in MMC mode
*/
#define SD_SCK		1	//!< Clock
#define SD_CMD		2
#define SD_DAT0		3

#define SD_DAT3		4
#define SD_DAT1		5
#define SD_DAT2		6
#define SD_CARD		7

/** @name MMC/SD Card I/O lines in SPI mode
*/
#define MMC_SCK		1
#define MMC_MOSI	2
#define MMC_MISO	3

#define MMC_CS		6
#define MMC_DETECT	4	



/** Helper structure.
	This simplify conversion between bytes and words.
*/
struct u16bytes
{
	uint8_t low;	//!< byte member
	uint8_t high;	//!< byte member
};

/** Helper union.
	This simplify conversion between bytes and words.
*/
union u16convert
{
	uint16_t value;			//!< for word access
	struct u16bytes bytes;	//!< for byte access
};

/** Helper structure.
	This simplify conversion between bytes and longs.
*/
struct u32bytes
{
	uint8_t byte1;	//!< byte member
	uint8_t byte2;	//!< byte member
	uint8_t byte3;	//!< byte member
	uint8_t byte4;	//!< byte member
};

/** Helper structure.
	This simplify conversion between words and longs.
*/
struct u32words
{
	uint16_t low;		//!< word member
	uint16_t high;		//!< word member
};

/** Helper union.
	This simplify conversion between bytes, words and longs.
*/
union u32convert 
{
	uint32_t value;			//!< for long access
	struct u32words words;	//!< for word access
	struct u32bytes bytes;	//!< for byte access
};





uint8_t mmc_card_detect();

/** Read MMC/SD sector.
 	Read a single 512 byte sector from the MMC/SD card
	\param lba	Logical sectornumber to read
	\param buffer	Pointer to buffer for received data
	\return 0 on success, -1 on error
*/
int mmc_readsector(uint32_t lba, uint8_t *buffer);


inline int mmc_writesector(uint32_t lba, uint8_t *buffer);  //non-pending
inline int mmc_writesector_pending(uint32_t lba, uint8_t *buffer);  //pending
int _mmc_writesector(uint32_t lab, uint8_t *buffer, uint8_t pending);


/** Init MMC/SD card.
	Initialize I/O ports for the MMC/SD interface and 
	send init commands to the MMC/SD card
	\return 0 on success, other values on error 
*/
uint8_t mmc_init(void);

/*
writes a bunch of dummy bytes to the MMC card at the end of the right cycle 
to ensure that the data is committed and then it releases the chip select 
for the card to free up the SPI bus
*/
void mmc_clock_and_release(void);
#endif
