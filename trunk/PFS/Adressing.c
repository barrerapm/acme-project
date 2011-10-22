#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "Adressing.h"

extern ConfigPFS config_pfs; // Estructura global config_ppd

void Adressing_read_block(BootSector* bs, Block* block, int n_block) {
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	int i;
	for (i = 0; i < config_pfs.clusters_per_block; i++) {
		Adressing_read_cluster(bs, cluster, n_block * config_pfs.clusters_per_block + i);
		block->cluster[i] = *cluster;
	}
}

void Adressing_read_cluster(BootSector* bs, Cluster* cluster, int n_cluster) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	int i, first_DataRegion_sector = bs->reserved_sector_count + bs->number_of_fats * bs->fat32_sectors_per_fat;
	int n_sector = first_DataRegion_sector + n_cluster * config_pfs.sectors_per_cluster;
	for (i = 0; i < config_pfs.sectors_per_cluster; i++, n_sector++) {
		PPD_read_sector(sector, n_sector);
		cluster->sector[i] = *sector;
	}
}

