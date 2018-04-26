#pragma once

// Use OCR2 to set current TOP value for Timer/Counter2
// Use TC2_PRESCALE to define prescaling you need (see 18.11.1 datasheet sec.)
// Requires TC2_PRESCALE be defined
// Requires TC2_RELOCPIN_SET(),
//          TC2_RELOCPIN_CLEAR() and
//          TC2_RELOCPIN_CONFIG() be defined
// if TC2_RELOCATE defined

#include <act-common/common.h>

#ifdef TC2_RELOCATE

#pragma vector=TIMER2_OVF_vect
__interrupt void tr2_ovf_interrupt_handler()
{
    TC2_RELOCPIN_SET();
}

#pragma vector=TIMER2_COMP_vect
__interrupt void tr2_comp_interrupt_handler()
{
    TC2_RELOCPIN_CLEAR();
}

inline __monitor void init_tc2_wg()
{
    /* Configure relocated pin */
    TC2_RELOCPIN_CONFIG();
    /* Enable Compare Match and Overflow interrupts */
    TIMSK |= (1<<OCIE2)|(1<<TOIE2);
    /* Setup non-inverting Fast PWM and prescale */
    TCCR2 = (1<<WGM21)|(1<<WGM20)|(TC2_PRESCALE<<CS20);
}

#elif

inline __monitor void init_tc2_wg()
{
    /* Configure PB2/OC2 pin */
    PORTB &= ~(1 << PORTB3);
    DDRB |= (1 << DDB3);
    /* Setup non-inverting Fast PWM and prescale */
    TCCR2 = (1<<WGM21)|(1<<WGM20)|(1<<COM21)|(0<<COM20)|(TC2_PRESCALE<<CS20);
}

#endif
