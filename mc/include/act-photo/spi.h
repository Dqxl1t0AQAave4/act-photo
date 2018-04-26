#pragma once


// Requires SPI_DORD, SPI_CPOL, SPI_CPHA, SPI_CR be defined


/* generate and register interrupt handlers */

#define REG_SPI_2M_HANDLER


/* use default I/O BUF_SIZEs and types */


#include <act-common/spi_2m.h>


inline __monitor void spi_init()
{
    SPCR = (1 << SPIE) | (1 << SPE) | (SPI_DORD << DORD) |
           (SPI_CPOL << CPOL) | (SPI_CPHA << CPHA) | (SPI_CR << SPR0);
    DDRB |= (1 << DDB2) | (1 << DDB3); /* SS and MOSI for the master */
}
