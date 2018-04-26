#pragma once


// Requires TWI_2M_ADDRESS, TWI_2M_MASTER_ADDRESS be defined


/* generate and register interrupt handlers */

#define REG_TWI_2M_HANDLER


/* use default I/O BUF_SIZEs and types */


#include <act-common/twi_2m.h>


inline __monitor void twi_init()
{
    TWAR = (TWI_2M_ADDRESS << TWA0);
    TWCR = (1 << TWEN) | (1 << TWIE);
}
