#include <inc/x86.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <kern/e1000.h>
#define NUMTD 64
#define NUMRD 128

struct tx_desc{
	uint64_t addr;
	uint16_t length;
	uint8_t  cso;
	uint8_t  cmd;
	uint8_t  status;
	uint8_t  css;
	uint16_t  special;
};

struct rx_desc {
    uint64_t addr;       /* Address of the descriptor's data buffer */
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

__attribute__((__aligned__(128)))
static struct tx_desc txdesc[NUMTD];

__attribute__((__aligned__(128)))
static struct rx_desc rxdesc[NUMRD];

static volatile char* e1000_regbase;

// LAB 6: Your driver code here
int e1000_enable(struct pci_func *func)
{
	pci_func_enable(func);
	e1000_regbase = mmio_map_region(func->reg_base[0], func->reg_size[0]);
	cprintf("status = %08x\n", *(uint32_t*)(e1000_regbase+E1000_STATUS));
	e1000_init();
	return 0;
}

void e1000_init()
{
	// follow the 14.5 Transmit Initialization from spec
	int i;
	for (i = 0; i < NUMTD; i++){
		txdesc[i].addr = page2pa(page_alloc(ALLOC_ZERO));
//	cprintf("txdesc=%08x\n", PADDR(txdesc+i));
//	cprintf("addr=%08x\n", txdesc[i].addr);
		txdesc[i].length = PGSIZE;
	}

	for (i = 0; i < NUMRD; i++){
		rxdesc[i].addr = page2pa(page_alloc(ALLOC_ZERO));
//	cprintf("rxdesc=%08x\n", PADDR(rxdesc+i));
//	cprintf("addr=%08x\n", rxdesc[i].addr);
		rxdesc[i].length = PGSIZE;
	}

	// set TDLEN
	*(volatile uint32_t*)(e1000_regbase + E1000_TDLEN) = (NUMTD*sizeof(struct tx_desc));

	// set TDBAL/TDBAH
	*(volatile uint32_t*)(e1000_regbase + E1000_TDBAL) = (uint32_t)PADDR(&txdesc[0]);
	*(volatile uint32_t*)(e1000_regbase + E1000_TDBAH) = 0;

	// set TDH/TDT
	*(volatile uint32_t*)(e1000_regbase + E1000_TDH) = 0;
	*(volatile uint32_t*)(e1000_regbase + E1000_TDT) = 0;

	// set TCTL.EN
	*(volatile uint32_t*)(e1000_regbase + E1000_TCTL) |= E1000_TCTL_EN;
	// set TCTL.PSP to 1
	*(volatile uint32_t*)(e1000_regbase + E1000_TCTL) |= E1000_TCTL_PSP & (0x1 << 3);
	// set TCTL.CT
	*(volatile uint32_t*)(e1000_regbase + E1000_TCTL) |= E1000_TCTL_CT  & (0x10 << 4);
	// set TCTL.COLD to 1
	*(volatile uint32_t*)(e1000_regbase + E1000_TCTL) |= E1000_TCTL_COLD & (0x40 << 12);

	// set TIPG
	*(volatile uint32_t*)(e1000_regbase + E1000_TIPG) = 10 | (8 << 10) | (6 << 20);

	// initialize receive side
	// set RAL/RAH, must set dword unit, not byte
	*(volatile uint32_t*)(e1000_regbase + E1000_RA) = 0x12005452;
	*(volatile uint32_t*)(e1000_regbase + E1000_RA+4) = 0x80005634;

	// set MTA
	*(volatile uint32_t*)(e1000_regbase + E1000_MTA) = 0;

	// set RDAL/RDAH
	*(volatile uint32_t*)(e1000_regbase + E1000_RDBAL) = (uint32_t)PADDR(&rxdesc[0]);
	*(volatile uint32_t*)(e1000_regbase + E1000_RDBAH) = 0;

	// set RDLEN
	*(volatile uint32_t*)(e1000_regbase + E1000_RDLEN) = (NUMRD*sizeof(struct rx_desc));

	// set RDH/RDT
	*(volatile uint32_t*)(e1000_regbase + E1000_RDH) = 0;
	*(volatile uint32_t*)(e1000_regbase + E1000_RDT) = NUMRD;

	// set RCTL.EN
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_EN;

	// set RCTL.LPE to 1
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_LPE;

	// set RCTL.LBM to none
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_LBM_NO;

	// set RCTL.BAM to 1
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_BAM;

	// set RCTL.BSIZE to 4096
//	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_BSEX;
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_SZ_2048;

	// set strip ethernet CRC
	*(volatile uint32_t*)(e1000_regbase + E1000_RCTL) |= E1000_RCTL_SECRC;
}

int transmit_data(uint8_t *p, uint32_t len)
{
	uint32_t tdh, tdt, tdbal, tdbah, send;
	int pkts = (len / PGSIZE) + (len % PGSIZE) ? 1 : 0;
	tdh = *(volatile uint32_t*)(e1000_regbase + E1000_TDH);
	tdt = *(volatile uint32_t*)(e1000_regbase + E1000_TDT);

	tdbal = *(volatile uint32_t*)(e1000_regbase + E1000_TDBAL);
	tdbah = *(volatile uint32_t*)(e1000_regbase + E1000_TDBAH);
	cprintf("tdh=%08x, tdt=%08x, tdbal=%08x, tdbah=%08x, len=%08x\n", tdh, tdt, tdbal, tdbah, len);

	for (int i = 0; i < pkts; i++){
		if ((tdt == tdh-1 || (!(txdesc[tdt].status & E1000_TXD_STAT_DD) && txdesc[tdt].cmd & E1000_TXD_CMD_RS)))
			panic("the transmit queue is full\n");

		txdesc[tdt].cmd |= E1000_TXD_CMD_RS;
		send = PGSIZE;
		if (i == pkts-1){
			txdesc[tdt].cmd |= E1000_TXD_CMD_EOP;
			send = len-i*PGSIZE;
		}
		txdesc[tdt].length = send;
		memcpy(KADDR(txdesc[tdt].addr), (void*)p + PGSIZE*i, send);
		// check if wrap
		if (tdt == NUMTD - 1)
			tdt = 0;
		else
			tdt++;
	}
	cprintf("tdh=%08x, tdt=%08x, tdbal=%08x, tdbah=%08x, len=%08x\n", tdh, tdt, tdbal, tdbah, len);
	*(volatile uint32_t*)(e1000_regbase + E1000_TDT) = tdt;
	return len;
}

int receive_data(uint8_t *p)
{
	uint32_t rdh, rdt, rdbal, rdbah, recv = 0, pos = 0;
	rdh = *(volatile uint32_t*)(e1000_regbase + E1000_RDH);
	rdt = *(volatile uint32_t*)(e1000_regbase + E1000_RDT);

	rdbal = *(volatile uint32_t*)(e1000_regbase + E1000_RDBAL);
	rdbah = *(volatile uint32_t*)(e1000_regbase + E1000_RDBAH);

	while (1){
		if ((rdt == NUMRD && rdh == 0) || rdt == rdh){
			cprintf("the queue is empty\n");
			break;
		}

		if (rdt == NUMRD)
			rdt = 0;

		if (rxdesc[rdt].status & E1000_RXD_STAT_DD){
			recv += rxdesc[rdt].length;
		} else {
			cprintf("no data is ready\n");
			break;
		}

	cprintf("p = 0x%x, rdh=%08x, rdt=%08x, rdbal=%08x, rdbah=%08x\n, length=0x%x", p, rdh, rdt, rdbal, rdbah, rxdesc[rdt].length);
		memcpy(p + pos, KADDR(rxdesc[rdt].addr), rxdesc[rdt].length);
		// check if wrap
		if (rdt == NUMTD - 1)
			rdt = 0;
		else
			rdt++;
		pos += recv;
	}
	cprintf("rdh=%08x, rdt=%08x, rdbal=%08x, rdbah=%08x, recv=%08x\n", rdh, rdt, rdbal, rdbah, recv);
	return recv;
}
