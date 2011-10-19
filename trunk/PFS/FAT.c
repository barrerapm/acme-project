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
	int n_entry = f_clusters[0]+2;
	int n = 1;
	while (FAT_table->entry[n_entry] != LAST_FILE_CLUSTER && FAT_table->entry[n_entry] != 0xfffff8) { // Si el cluster no es el ultimo del archivo
		f_clusters = realloc(f_clusters, sizeof(int)+n*sizeof(int));
		f_clusters[n] = n_entry = FAT_table->entry[n_entry];
		n++;
	}
}

void FAT_get_free_clusters() {
	// Definir
}

void FAT_write_FAT_entry() {
	// Definir
}

