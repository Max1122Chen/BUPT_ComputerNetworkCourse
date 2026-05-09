#include "datalink.h"

#define MAX_SEQ 7
#define NR_BUFS 4

static unsigned char out_buf[NR_BUFS][PKT_LEN];
static int out_len[NR_BUFS];
static unsigned char ack_expected = 0;
static unsigned char next_frame_to_send = 0;
static unsigned char frame_expected = 0;
static unsigned char nbuffered = 0;
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

static void put_frame(unsigned char *frame, int len)
{
	*(unsigned int *)(frame + len) = crc32(frame, len);
	send_frame(frame, len + 4);
	phl_ready = 0;
}

static void send_data_frame(unsigned char seq)
{
	struct FRAME s;
	int idx = seq % NR_BUFS;
	int id = out_len[idx] >= 2 ? *(short *)out_buf[idx] : -1;

	s.kind = FRAME_DATA;
	s.seq = seq;
	s.ack = 0;
	memcpy(s.data, out_buf[idx], out_len[idx]);

	dbg_frame("Send DATA %d, ID %d\n", s.seq, id);

	put_frame((unsigned char *)&s, 3 + out_len[idx]);
	start_timer(seq, DATA_TIMER);
}

static void send_ack_frame(unsigned char ack)
{
	struct FRAME s;

	s.kind = FRAME_ACK;
	s.ack = ack;

	dbg_frame("Send ACK  %d\n", s.ack);

	put_frame((unsigned char *)&s, 2);
}

void go_back_n(void)
{
	int event, arg;
	struct FRAME f;
	int len = 0;

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
				break;
			}
			if (f.kind == FRAME_ACK) {
				dbg_frame("Recv ACK  %d\n", f.ack);
				while (nbuffered > 0 && between(ack_expected, f.ack, next_frame_to_send)) {
					stop_timer(ack_expected);
					nbuffered--;
					ack_expected = inc(ack_expected);
				}
			}
			if (f.kind == FRAME_DATA) {
				dbg_frame("Recv DATA %d %d, ID %d\n", f.seq, f.ack, *(short *)f.data);
				if (f.seq == frame_expected) {
					put_packet(f.data, len - 7);
					send_ack_frame(f.seq);
					frame_expected = inc(frame_expected);
				}
			}
			break;

		case DATA_TIMEOUT: {
			unsigned char seq = ack_expected;
			int i;

			dbg_event("---- DATA %d timeout\n", arg);
			for (i = 0; i < nbuffered; i++) {
				send_data_frame(seq);
				seq = inc(seq);
			}
			break;
		}
		}

		if (nbuffered < NR_BUFS && phl_ready)
			enable_network_layer();
		else
			disable_network_layer();
	}
}