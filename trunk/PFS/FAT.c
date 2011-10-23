#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "utils.h"
#include "FAT.h"
#define ROOT_ENTRY_BYTES 32
#define LAST_FILE_CLUSTER 0xFFFFFFF
#define FREE_CLUSTER 0x0000000

void FAT_get_file_clusters(int* file_clusters) {
	extern BootSector* bs;
	extern TablaFAT* FAT_table;
	int total_entries = (bs->bytes_per_sector * bs->sectors_per_cluster) / ROOT_ENTRY_BYTES;
	int n_entry = file_clusters[0] + bs->fat32_first_cluster;
	int n = 1;
	while (FAT_table->entry[n_entry] != LAST_FILE_CLUSTER && FAT_table->entry[n_entry] != 0xfffff8) { // Si el cluster no es el ultimo del archivo
		file_clusters = realloc(file_clusters, sizeof(int) * (n + 1));
		file_clusters[n++] = n_entry = FAT_table->entry[n_entry];
	}
}

int FAT_get_free_clusters(int n_free_clusters, int* free_clusters) {
	extern BootSector* bs;
	extern FSInfo* fs_info;
	extern TablaFAT* FAT_table;
	if (n_free_clusters > fs_info->free_clusters) { // Si necesitamos más clusters libres de los que hay
		return -1;
	}

	int n = 0, first_free_cluster = fs_info->most_recently_alloc_cluster - bs->fat32_first_cluster + 1;
	int n_cluster = free_clusters[n] = first_free_cluster;
	int last_entry = sizeof(FAT_table->entry) / 4;

	while ((n_free_clusters > n) && (n_cluster < last_entry)) { // Mientras no llegue a la cantidad de clusters pedidos y no llegue al final de la FAT
		n_cluster++;
		if (FAT_table->entry[n_cluster + bs->fat32_first_cluster] == FREE_CLUSTER) {
			free_clusters = realloc(free_clusters, sizeof(int) * ((++n) + 1));
			free_clusters[n] = n_cluster;
		}
	}
	n_cluster = 0;
	while (n_free_clusters > n) { // Si todavia faltan llenar clusters pedidos, entonces se llego al final de la FAT y arrancamos desde el principio
		n_cluster++;
		if (FAT_table->entry[n_cluster + bs->fat32_first_cluster] == FREE_CLUSTER) {
			free_clusters = realloc(free_clusters, sizeof(int) * (++n + 1));
			free_clusters[n] = n_cluster;
		}
	}
	return 0;
}

void FAT_write_FAT_entry(int n_free_clusters, int* free_clusters) {
	extern BootSector* bs;
	extern FSInfo* fs_info;
	extern TablaFAT* FAT_table;
	int i, n_entry;
	for (i = 0; i < n_free_clusters - 1; i++) {
		n_entry = free_clusters[i] + bs->fat32_first_cluster;
		FAT_table->entry[n_entry] = free_clusters[i + 1]; // Donde era 0x00 ahora le pongo el numero de su siguiente cluster
		fs_info->most_recently_alloc_cluster = free_clusters[i]; // El ultimo cluster con contenido ahora cambia
		--(fs_info->free_clusters); // Cantidad de clusters libres cambia
	}
	// Para el ultimo cluster debe tener 0xFF
	n_entry = free_clusters[i] + bs->fat32_first_cluster;
	FAT_table->entry[n_entry] = LAST_FILE_CLUSTER;
	fs_info->most_recently_alloc_cluster = free_clusters[i];
	--(fs_info->free_clusters);
}
