#ifndef STRUCT_H_
#define STRUCT_H_

#include <stdint.h>
#include "list.h"

typedef struct sector {
	uint8_t byte[512];
} Sector;

typedef struct block {
	Sector sector[2];
} Block;

typedef struct cluster {
	Block block[4];
} Cluster;

typedef struct sec_nsec {
	uint32_t n_sector;
	Sector sector;
} Sector_n_sector;

typedef struct chs_id {
	uint32_t cylinder;
	uint8_t head;
	uint16_t sector;
	t_list* disk_id_list;
} CHS_id;

typedef struct disk_id {
	char disk_id[25];
} DiskID;

typedef struct PPD_chs_id {
	uint32_t cylinder;
	uint8_t head;
	uint16_t sector;
	char disk_id[25];
} PPD_CHS_id;

#endif /* STRUCT_H_ */
