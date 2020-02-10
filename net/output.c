#include "ns.h"

extern union Nsipc nsipcbuf;

#define REQVA		(0x0ffff000 - QUEUE_SIZE * PGSIZE)
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1){
		if (ipc_recv(0, (void*)&nsipcbuf.pkt, 0) == NSREQ_OUTPUT){
			sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		}
	}
}
