#include "common.h"

#define IBUF_SIZE    16
#define OBUF_SIZE    32
#define BAUD_RATE    9600
#define FOSC         1000000
#define TC2_PRESCALE 1

#define MAX_M        512

#include "usart.h"
#include "pwm.h"


inline __monitor void init()
{
    iobuf_state = IBUF_NFULL | OBUF_NFULL;
    usart_init();
    init_tc2_wg();
}
    
    
/* Common variables */


sbyte kp = 1,       /* proportional factor */
      ki = -7,      /* memory factor */
      km = 7,       /* memory size */
      ks = 0        /* scale factor */
      ;

bool  pause = false;/* USART async/sync owrite operation */
bool  sync  = true; /* USART async/sync owrite operation */

byte  adc1, adc2;   /* ADC 1st and 2nd channel */

sbyte err[MAX_M];   /* e[i] */
int   err_idx = 0;

    
/* Message Loop variables */


byte command;
bool command_present = false;


/* Commands */


#define COMMAND_SET_COEFS  byte(0xFC)
#define COMMAND_SET_SYNC   byte(0xFD)
#define COMMAND_SEND_DATA  byte(0xFE)
#define COMMAND_PAUSE      byte(0xFF)
#define _COMMAND_SET_COEFS byte(~0xFC)
#define _COMMAND_SET_SYNC  byte(~0xFD)
#define _COMMAND_SEND_DATA byte(~0xFD)
#define _COMMAND_PAUSE     byte(~0xFF)


void answer(byte *data, byte size)
{
    if (!sync)
    {
        for (int i = 0; i < size; i++)
        {
            owrite(data[i]);
        }
    }
    else
    {
        for (int i = 0; i < size; i++)
        {
            while (!owrite(data[i])) // wait in sync mode
              ;
        }
    }
}


#define ANSWER(arr,size,cmd, ncmd) \
  arr[0] = cmd; arr[1] = cmd; arr[size-1] = ncmd; arr[size-2] = ncmd; answer(arr, size);


void process_command()
{
    byte args[5];
    byte answ[4];
    switch(command)
    {
    case COMMAND_SET_COEFS:
        if (isize() < 5) return; // S-kpki-kmks-St-St
        for (int i = 0; i < 5; i++) iread(args[i]);
        // validate data packet
        if ((args[0] != COMMAND_SET_COEFS)  ||
            (args[3] != _COMMAND_SET_COEFS) ||
            (args[4] != _COMMAND_SET_COEFS)) break;
        // unpack data
        kp = ((args[1] >> 4) & 0x7); if ((args[1] >> 7) == 1) kp = -kp;
        ki = (args[1] & 0x7); if(((args[1] >> 3) & 0x1) == 1) ki = -ki;
        km = ((args[2] >> 4) & 0xf); // positive only
        ks = -(args[2] & 0xf);       // negative only
        // answer
        ANSWER(answ, 4, COMMAND_SET_COEFS, _COMMAND_SET_COEFS)
        break;
    case COMMAND_SET_SYNC:
        if (isize() < 4) return; // S-sync-St-St
        for (int i = 0; i < 4; i++) iread(args[i]);
        // validate data packet
        if (args[0] != COMMAND_SET_SYNC  ||
            args[2] != _COMMAND_SET_SYNC ||
            args[3] != _COMMAND_SET_SYNC) break;
        // unpack data
        sync = args[1];
        // answer
        ANSWER(answ, 4, COMMAND_SET_SYNC, _COMMAND_SET_SYNC)
        break;
    case COMMAND_PAUSE:
        if (isize() < 3) return; // S-St-St
        for (int i = 0; i < 3; i++) iread(args[i]);
        // validate data packet
        if (args[0] != COMMAND_PAUSE  ||
            args[1] != _COMMAND_PAUSE ||
            args[3] != _COMMAND_PAUSE) break;
        // execute
        pause = !pause;
        // answer
        ANSWER(answ, 4, COMMAND_SET_SYNC, _COMMAND_SET_SYNC)
        break;
    }
    command_present = false;
}


void do_computations()
{
}


int main()
{
  
  
    /* Initialization here */
    
    
    init();
    
    
    /* Program Loop */
    
    
    for(;;)
    {
        
        
        /* Message Loop */
        
        
        if (command_present)
        {
            process_command();
        }
        else
        {
            command_present = iread(command);
        }
        
        
        /* Program Body */
        
        
        if (!pause)
        {
            do_computations();
        }
        
        
    }
    
    
    return 0;
}
