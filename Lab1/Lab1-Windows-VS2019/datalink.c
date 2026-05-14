#include "datalink.h"

int main(int argc, char **argv)
{
	protocol_init(argc, argv);
	lprintf("Designed by Jiang Yanjun, build: " __DATE__ "  "__TIME__"\n");

	disable_network_layer();

	switch (datalink_protocol) {
	case DATALINK_PROTO_STOP_WAIT:
		stop_and_wait();
		break;
	case DATALINK_PROTO_GBN_BASIC:
		go_back_n();
		break;
	case DATALINK_PROTO_GBN_ACK:
		go_back_n_piggypacking();
		break;
	case DATALINK_PROTO_SR:
	default:
		selective_repeat();
		break;
	}

	return 0;
}