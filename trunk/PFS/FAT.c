#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "utils.h"
#include "FAT.h"
#define ROOT_ENTRY_BYTES 32
#define LAST_FILE_CLUSTER 0xFFFFFFF

void FAT_get_file_clusters(BootSector* bs, TablaFAT* FAT_table, int* f_clusters) {
	int total_entries = (bs->bytes_per_sector * bs->sectors_per_cluster) / ROOT_ENTRY_BYTES;
	int n_entry = f_clusters[0]+bs->fat32_cluster_num_dir_start;
	int n = 1;
	while (FAT_table->entry[n_entry] != LAST_FILE_CLUSTER && FAT_table->entry[n_entry] != 0xfffff8) { // Si el cluster no es el ultimo del archivo
		f_clusters = realloc(f_clusters, sizeof(int)*(n+1));
		f_clusters[n] = n_entry = FAT_table->entry[n_entry];
		n++;
	}
}

int FAT_get_free_clusters(BootSector* bs, FSInfo* fs_info, TablaFAT* FAT_table, int n_free_clusters, int* free_clusters) {

	if (n_free_clusters > fs_info->free_clusters){
		return -1;
	}
	int n = 0;
	int cluster = free_clusters[n] = fs_info->most_recently_alloc_cluster + 1;
	int max_entry = sizeof(FAT_table->entry)/4;

	while((n_free_clusters > n) &&( cluster < max_entry)){
		if(FAT_table->entry[cluster + bs->fat32_cluster_num_dir_start] == 0){ // Es cluster libre
			free_clusters = realloc(free_clusters, sizeof(int)*((++n)+1));
			free_clusters[n] = cluster;
		}
		cluster++;
	}
	cluster = 0;
	while (n_free_clusters > n){
		if(FAT_table->entry[cluster + bs->fat32_cluster_num_dir_start] == 0){ // Es cluster libre
					free_clusters = realloc(free_clusters, sizeof(int)*(++n+1));
					free_clusters[n] = cluster;
				}
				cluster++;
	}
	return 0;
}

void FAT_write_FAT_entry() {
	// Definir
}

