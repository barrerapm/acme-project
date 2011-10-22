#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "utils.h"
#include "Disk.h"
#define FAT_ENTRY_BYTES 4
#define BOOT_SECTOR 0

void Disk_load_bootsector(BootSector* bs) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	PPD_read_sector(sector, BOOT_SECTOR);
	memcpy(bs, sector->byte, sizeof(BootSector));
}

void Disk_load_FSInfo(BootSector* bs, FSInfo* fs_info) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	PPD_read_sector(sector, bs->fat32_sector_num_fs_inf_sec);
	memcpy(fs_info, sector->byte, sizeof(FSInfo));
}

void Disk_load_FAT(BootSector* bs, TablaFAT* FAT_table) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	int i, FAT_sector = bs->reserved_sector_count;
	int last_FAT_sector = FAT_sector + bs->fat32_sectors_per_fat;
	int entries_per_sector = bs->bytes_per_sector / FAT_ENTRY_BYTES;
	for (i = 0; FAT_sector < last_FAT_sector; FAT_sector++, i++) {
		PPD_read_sector(sector, FAT_sector);
		memcpy(FAT_table->entry + entries_per_sector * i, sector->byte, 512);
		// Al agregar los 512 bytes del Sector, se van a cargar las proximas 512/4 entradas
		// Para que no se pisen las entradas cargadas, me voy corriendo de a 512/4 entradas (entradas que hay por sector)
	}
}
