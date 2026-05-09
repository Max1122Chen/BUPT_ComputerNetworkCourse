#include "datalink.h"
#include <string.h>

#define MAX_SEQ 7
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define ACK_TIMER 260

static unsigned char out_buf[NR_BUFS][PKT_LEN];
static unsigned char in_buf[NR_BUFS][PKT_LEN];
static int out_len[NR_BUFS];
static int in_len[NR_BUFS];
static unsigned char arrived[NR_BUFS];

static unsigned char ack_expected = 0;
static unsigned char next_frame_to_send = 0;
static unsigned char frame_expected = 0;
static unsigned char too_far = NR_BUFS;
static unsigned char nbuffered = 0;
static unsigned char no_nak = 1;
static int phl_ready = 0;

static unsigned char inc(unsigned char seq)
{
	return (unsigned char)((seq + 1) % (MAX_SEQ + 1));
}

static int between(unsigned char a, unsigned char b, unsigned char c)
{
	if (a <= c)
		return a <= b && b < c;
	return a <= b || b < c;
}

static unsigned char last_acked_frame(void)
{
	return (unsigned char)((frame_expected + MAX_SEQ) % (MAX_SEQ + 1));
}

static void put_frame(unsigned char *frame, int len)
{
	*(unsigned int *)(frame + len) = crc32(frame, len);
	send_frame(frame, len + 4);
	phl_ready = 0;
}

static void process_ack(unsigned char ack)
{
	while (nbuffered > 0 && between(ack_expected, ack, next_frame_to_send)) {
		stop_timer(ack_expected);
		nbuffered--;
		ack_expected = inc(ack_expected);
	}
}

static void send_data_frame(unsigned char seq)
{
	struct FRAME s;
	int idx = seq % NR_BUFS;
	int id = out_len[idx] >= 2 ? *(short *)out_buf[idx] : -1;

	s.kind = FRAME_DATA;
	s.seq = seq;
	s.ack = last_acked_frame();
	memcpy(s.data, out_buf[idx], out_len[idx]);

	dbg_frame("Send DATA %d %d, ID %d\n", s.seq, s.ack, id);

	put_frame((unsigned char *)&s, 3 + out_len[idx]);
	start_timer(seq, DATA_TIMER);
	stop_ack_timer();
}

static void send_ack_frame(void)
{
	struct ACK_FRAME s;

	s.kind = FRAME_ACK;
	s.ack = last_acked_frame();

	dbg_frame("Send ACK  %d\n", s.ack);

	put_frame((unsigned char *)&s, 2);
	stop_ack_timer();
}

static void send_nak_frame(void)
{
	struct NAK_FRAME s;

	s.kind = FRAME_NAK;
	s.ack = last_acked_frame();

	dbg_frame("Send NAK  %d\n", s.ack);

	put_frame((unsigned char *)&s, 2);
	stop_ack_timer();
}

void selective_repeat(void)
{
	int event, arg;
	struct FRAME f;
	int len = 0;
	int i;

	for (i = 0; i < NR_BUFS; i++)
		arrived[i] = 0;

	for (;;) {
		event = wait_for_event(&arg);

		switch (event) {
		case NETWORK_LAYER_READY: {
			int idx = next_frame_to_send % NR_BUFS;

			out_len[idx] = get_packet(out_buf[idx]);
			nbuffered++;
			send_data_frame(next_frame_to_send);
			next_frame_to_send = inc(next_frame_to_send);
			break;
		}

		case PHYSICAL_LAYER_READY:
			phl_ready = 1;
			break;

		case FRAME_RECEIVED:
			len = recv_frame((unsigned char *)&f, sizeof f);
			if (len < 5 || crc32((unsigned char *)&f, len) != 0) {
				dbg_event("**** Receiver Error, Bad CRC Checksum\n");
				if (no_nak) {
					send_nak_frame();
					no_nak = 0;
				}
				break;
			}

			if (f.kind == FRAME_ACK)
				dbg_frame("Recv ACK  %d\n", f.ack);
			else if (f.kind == FRAME_NAK)
				dbg_frame("Recv NAK  %d\n", f.ack);
			else if (f.kind == FRAME_DATA)
				dbg_frame("Recv DATA %d %d, ID %d\n", f.seq, f.ack, *(short *)f.data);

			if (f.kind == FRAME_DATA) {
				if (f.seq != frame_expected && no_nak) {
					send_nak_frame();
					no_nak = 0;
				} else {
					start_ack_timer(ACK_TIMER);
				}

				if (between(frame_expected, f.seq, too_far) && !arrived[f.seq % NR_BUFS]) {
					arrived[f.seq % NR_BUFS] = 1;
					in_len[f.seq % NR_BUFS] = len - 7;
					memcpy(in_buf[f.seq % NR_BUFS], f.data, len - 7);
				}

				while (arrived[frame_expected % NR_BUFS]) {
					put_packet(in_buf[frame_expected % NR_BUFS], in_len[frame_expected % NR_BUFS]);
					no_nak = 1;
					arrived[frame_expected % NR_BUFS] = 0;
					frame_expected = inc(frame_expected);
					too_far = inc(too_far);
					start_ack_timer(ACK_TIMER);
				}
			}

			if (f.kind == FRAME_NAK) {
				unsigned char seq = inc(f.ack);
				if (between(ack_expected, seq, next_frame_to_send))
					send_data_frame(seq);
			}

			process_ack(f.ack);
			break;

		case ACK_TIMEOUT:
			send_ack_frame();
			break;

		case DATA_TIMEOUT:
			dbg_event("---- DATA %d timeout\n", arg);
			send_data_frame((unsigned char)arg);
			break;
		}

		if (nbuffered < NR_BUFS && phl_ready)
			enable_network_layer();
		else
			disable_network_layer();
	}
}