#include "ns.h"

extern union Nsipc nsipcbuf;

static struct jif_pkt *pkt = (struct jif_pkt*)REQVA;
void
input(envid_t ns_envid)
{
	int r;
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	while (1){
		if ((r = sys_page_alloc(0, pkt, PTE_P|PTE_U|PTE_W)) < 0)
			panic("sys_page_alloc: %e", r);

		if ((pkt->jp_len = sys_net_recv(pkt->jp_data)) > 0){
			cprintf("pkt length=0x%x\n", pkt->jp_len);
			ipc_send(ns_envid, NSREQ_INPUT, (void*)pkt, PTE_P|PTE_U|PTE_W);
			sys_page_unmap(0, pkt);
			break;
		}
	}
}
