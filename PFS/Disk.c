#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "Struct.h"
#include "PFS.h"
#include "utils.h"
#include "Disk.h"
#include "Addressing.h"
#include "AddressPFS.h"
#include "log.h"
#define FAT_ENTRY_BYTES 4
#define BOOT_SECTOR 0

extern BootSector* bs;
extern FSInfo* fs_info;
extern TablaFAT* FAT_table;
extern t_log* project_log;
extern ConfigPFS config_pfs;

void Disk_load_bootsector() {
	Block* block = (Block*) malloc(sizeof(Block));
	int block_sector;

	printf("[Disk_load_bootsector] Antes de leer el bloque del BootSector\n");
	// Leo el bloque donde esta el boot sector
	AddressPFS_read_block(PFS_get_n_block_by_sector(BOOT_SECTOR, &block_sector), block);
	printf("[Disk_load_bootsector] Despues de leer el bloque del BootSector\n");

	// Cargo el sector necesario del bloque, en la estructura boot sector
	memcpy(bs, block->sector[block_sector].byte, sizeof(BootSector));

	log_info(project_log, "[Disk_load_bootsector]", "%s", "Se cargó el Boot Sector");
	free(block);
}

void Disk_load_FSInfo() {
	Block* block = (Block*) malloc(sizeof(Block));
	int block_sector;

	// Leo el bloque donde esta el fs_info
	AddressPFS_read_block(PFS_get_n_block_by_sector(bs->fat32_sector_num_fs_inf_sec, &block_sector), block);

	// Cargo el sector necesario del bloque, en la estructura fs_info
	memcpy(fs_info, block->sector[block_sector].byte, sizeof(FSInfo));

	log_info(project_log, "[Disk_load_FSInfo]", "%s", "Se cargó el FS Info");
	free(block);
}

void Disk_load_FAT() {
	Block* block = (Block*) malloc(sizeof(Block));
	int i = 0, FAT_block_sector, last_FAT_block_sector;
	int FAT_sector = bs->reserved_sector_count;
	int FAT_block = PFS_get_n_block_by_sector(FAT_sector, &FAT_block_sector);
	int last_FAT_sector = FAT_sector + bs->fat32_sectors_per_fat;
	int last_FAT_block = PFS_get_n_block_by_sector(last_FAT_sector, &last_FAT_block_sector);
	int entries_per_block = (bs->bytes_per_sector * config_pfs.sectors_per_block) / FAT_ENTRY_BYTES;

	// Si el FAT_sector es impar, cargamos solo el ultimo sector del bloque
	if ((FAT_sector % 2) != 0) {
		memcpy(FAT_table->entry, block->sector[FAT_block_sector].byte, sizeof(block->sector));
		i = 1;
	}

	// Cargamos las entradas de la fat hasta que el FAT_block sea mayor al ultimo
	do {
		AddressPFS_read_block(FAT_block++, block);
		memcpy(FAT_table->entry + entries_per_block * i++, block->sector, sizeof(*block));
		// Al agregar los 1024 bytes del Bloque, se van a cargar las proximas 1024/4 entradas
		// Para que no se pisen las entradas cargadas, me voy corriendo de a 1024/4 entradas (entradas que hay por bloque)
	} while (FAT_block < last_FAT_block);

	free(block);
	log_info(project_log, "[Disk_load_FAT]", "%s", "Se cargó la Tabla FAT");
}
