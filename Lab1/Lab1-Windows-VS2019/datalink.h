#ifndef DATALINK_H
#define DATALINK_H

/* FRAME kind */
#define FRAME_DATA 1
#define FRAME_ACK  2
#define FRAME_NAK  3

#define DATA_TIMER 2000

#include <stdio.h>
#include <string.h>
#include "protocol.h"

/*  
    DATA Frame
    +=========+========+========+===============+========+
    | KIND(1) | SEQ(1) | ACK(1) | DATA(240~256) | CRC(4) |
    +=========+========+========+===============+========+

    ACK Frame
    +=========+========+========+
    | KIND(1) | ACK(1) | CRC(4) |
    +=========+========+========+

    NAK Frame
    +=========+========+========+
    | KIND(1) | ACK(1) | CRC(4) |
    +=========+========+========+
*/

struct FRAME {
	unsigned char kind; /* FRAME_DATA */
	unsigned char ack;
	unsigned char seq;
	unsigned char data[PKT_LEN];
	unsigned int padding;   // crc
};

struct ACK_FRAME {
    unsigned char kind; /* FRAME_ACK */
    unsigned char ack;
    unsigned int padding;   // crc
};

struct NAK_FRAME {
    unsigned char kind; /* FRAME_NAK */
    unsigned char ack;
    unsigned int padding;   // crc
};

extern void stop_and_wait(void);
extern void go_back_n(void);
extern void go_back_n_piggypacking(void);
extern void selective_repeat(void);

#endif

