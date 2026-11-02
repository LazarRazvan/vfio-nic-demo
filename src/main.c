/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdlib.h>
#include <ctype.h>

#include "e1000.h"


/*****************************************************************************/

#define VFIO_CONTAINER_PATH				"/dev/vfio/vfio"
#define VFIO_GROUP_PATH					"/dev/vfio/3"
#define VFIO_DEVICE_ID					"0000:00:03.0"				


/*****************************************************************************/

static void pkt_hex_dump(uint8_t *buf, uint16_t len)
{
	if (!buf || !len)
		return;

	printf("pkt %p, len %u\n", buf, len);

	uint16_t i = 0;
	uint16_t j;

	while (i < len) {
		// offset
		printf("%04x |", i);

		// hex (or empty)
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				printf("%02x ", buf[j + i]);
			else
				printf("   ");
		}

		// separator
		printf("  |  ");

		// ASCII
		for (j = 0; j < 16 && j + i < len; j++) {
			char c = (char)buf[j + i];
			printf("%c", isprint(c) ? c : '.');
		}

		printf("\n");
		i += 16;
	}

	printf("\n");
}


/*****************************************************************************/

int main()
{
	int rv;
	uint16_t len;
	e1000_dev_t dev;
	uint8_t buf[4096];

	// open e1000 device
	if (e1000_dev_open(VFIO_CONTAINER_PATH, VFIO_GROUP_PATH, VFIO_DEVICE_ID,
					&dev))
		return EXIT_FAILURE;

	// packets pool
	while (1) {
		rv = e1000_dev_rx_poll(&dev, buf, 4096, &len);

		//
		switch (rv) {
		case E1000_NO_PKT:
			break;
		case E1000_PKT_FRGMT:
			fprintf(stderr, "Packet fragmented\n");
			break;
		case E1000_PKT_NO_MEM:
			fprintf(stderr, "Packet too big\n");
			break;
		case E1000_PKT_SUCCESS:
			fprintf(stdout, "Packet received\n");
			pkt_hex_dump(buf, len);
			break;
		default:
			break;
		}
	}

	// close e1000 device
	e1000_dev_close(&dev);

	return EXIT_SUCCESS;
}
