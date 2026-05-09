#include "datalink.h"

int main(int argc, char **argv)
{
	protocol_init(argc, argv);
	lprintf("Designed by Jiang Yanjun, build: " __DATE__ "  "__TIME__"\n");

	disable_network_layer();

	selective_repeat();
}