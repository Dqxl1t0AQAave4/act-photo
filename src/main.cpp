#include "common.h"

#define IBUF_SIZE    16
#define OBUF_SIZE    32
#define BAUD_RATE    9600
#define FOSC         1000000
#define TC2_PRESCALE 1

#include "usart.h"
#include "pwm.h"

int main()
{
    iobuf_state = IBUF_NFULL | OBUF_NFULL;
    usart_init();
    init_tc2_wg();
    return 0;
}
