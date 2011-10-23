#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "Adressing.h"

extern ConfigPFS config_pfs; // Estructura global config_ppd

void Adressing_read_block(Block* block, int n_block) {
	extern BootSector* bs;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	int i;
	for (i = 0; i < config_pfs.clusters_per_block; i++) {
		Adressing_read_cluster(cluster, n_block * config_pfs.clusters_per_block + i);
		block->cluster[i] = *cluster;
	}
}

void Adressing_read_cluster(Cluster* cluster, int n_cluster) {
	extern BootSector* bs;
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	int i, first_DataRegion_sector = bs->reserved_sector_count + bs->number_of_fats * bs->fat32_sectors_per_fat;
	int n_sector = first_DataRegion_sector + n_cluster * config_pfs.sectors_per_cluster;
	for (i = 0; i < config_pfs.sectors_per_cluster; i++, n_sector++) {
		PPD_read_sector(sector, n_sector);
		cluster->sector[i] = *sector;
	}
}

void Adressing_close_disk(void){
	PPD_close_disk();
}

int Adressing_open_disk(void){
	return PPD_open_disk();
}
