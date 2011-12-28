#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "PFS.h"
#include "utils.h"
#include "FAT.h"
#include "log.h"
#include "Type.h"
#include "AddressPFS.h"
#include "Aux.h"

#define ROOT_ENTRY_BYTES 32
#define LAST_FILE_CLUSTER 0xFFFFFFF
#define LAST_FILE_CLUSTER2 0xffffff8
#define FREE_CLUSTER 0x0000000

extern t_log* project_log;
extern BootSector* bs;
extern TablaFAT* FAT_table;
extern FSInfo* fs_info;

// Recibe el vector con el primer n_cluster del contenido del archivo y carga el vector con cada n_cluster que ocupe el contenido
void FAT_get_file_clusters(int** file_clusters, int* n_file_clusters) {
	int i = 1, n_entry = (*file_clusters)[0] + bs->fat32_first_cluster;
	(*n_file_clusters) = 1;
	// Mientras que la entrada de la FAT no indique que es el ultimo cluster del archivo, cargo el vector con el n_cluster siguiente que me da la FAT
	Aux_printf("[FAT_get_file_clusters] n_entry: %d, FAT_table->entry[n_entry]: 0x%x\n", n_entry, FAT_table->entry[n_entry]);
	while (FAT_table->entry[n_entry] != LAST_FILE_CLUSTER && FAT_table->entry[n_entry] != LAST_FILE_CLUSTER2) {
		*file_clusters = realloc(*file_clusters, sizeof(int) * (i + 1));
		n_entry = FAT_table->entry[n_entry];
		(*file_clusters)[i++] = n_entry - bs->fat32_first_cluster;
		++(*n_file_clusters);
		//Aux_printf("[FAT_get_file_clusters] n_entry: %d, FAT_table->entry[n_entry]: %x\n", n_entry, FAT_table->entry[n_entry]);
	}
	//Aux_printf("[FAT_write_FAT_entry] La FAT esta asi: \n");
	//PFSPrint_FAT_table();
}

int FAT_get_free_clusters(int n_free_clusters, int** free_clusters) {
	// Si necesitamos mas clusters libres de los que hay
	if (n_free_clusters > fs_info->free_clusters) {
		log_warning(project_log, "[FAT_get_free_clusters]", "%s %d %s", "No hay disponible", n_free_clusters, "clusters libres");
		return -1;
	}

	int n = 0, first_free_cluster = fs_info->most_recently_alloc_cluster - bs->fat32_first_cluster + 1;
	int n_cluster = (*free_clusters)[n] = first_free_cluster;
	int last_entry = 130816; //sizeof(FAT_table->entry) / 4;
	n++;
	// Mientras no llegue a la cantidad de clusters pedidos y no llegue al final de la FAT
	while ((n_free_clusters > n) && (n_cluster < last_entry)) {
		n_cluster++;
		if (FAT_table->entry[n_cluster + bs->fat32_first_cluster] == FREE_CLUSTER) {
			*free_clusters = realloc(*free_clusters, sizeof(int) * (n + 1));
			(*free_clusters)[n] = n_cluster;
			n++;
		}
	}
	n_cluster = 0;
	// Si todavia faltan llenar clusters pedidos, entonces se llego al final de la FAT y arrancamos desde el principio
	while (n_free_clusters > n) {
		printf("[FAT_get_free_clusters] Si todavia faltan llenar clusters pedidos, entonces se llego al final de la FAT y arrancamos desde el principio\n");
		n_cluster++;
		if (FAT_table->entry[n_cluster + bs->fat32_first_cluster] == FREE_CLUSTER) {
			*free_clusters = realloc(*free_clusters, sizeof(int) * ((++n) + 1));
			(*free_clusters)[n] = n_cluster;
		}
	}

	log_info(project_log, "[FAT_get_free_clusters]", "%s%d%s", "Se obtuvieron ", n_free_clusters, " clusters libres");

	return 0;
}

// Grabo en la FAT desde la entrada del free_clusters[0] todos encadenados hasta el free_clusters[n] que va a ser el ffffff
void FAT_write_FAT_entry(int n_free_clusters, int* free_clusters) {
	printf("[FAT_write_FAT_entry] n_free_clusters: %d\n", n_free_clusters);
	printf("[FAT_write_FAT_entry] free_clusters[0]: %d\n", free_clusters[0]);
	Block* block = (Block*) malloc(sizeof(Block));
	int *blocks_modified = (int*) malloc(sizeof(int));
	int last_modified;
	blocks_modified[0] = -1;
	int i, n_entry;
	// Grabo cada entrada en la FAT, excepto la ultima
	for (i = 0; i < n_free_clusters - 1; i++) {
		// Si es un cluster libre, como lo vamos a pisar con un usado disminuimos un libre. Si no era libre, no tenemos que disminuir
		if (FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] == FREE_CLUSTER || FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] == FREE_USED_ENTRY) {
			--(fs_info->free_clusters);
		}
		n_entry = free_clusters[i] + bs->fat32_first_cluster;
		// Donde era 0x00 ahora le pongo el numero de su siguiente cluster
		FAT_table->entry[n_entry] = free_clusters[i + 1] + bs->fat32_first_cluster;
		// El ultimo cluster con contenido ahora cambia
		fs_info->most_recently_alloc_cluster = free_clusters[i] + bs->fat32_first_cluster;
		// Cantidad de clusters libres cambia
		/*
		if ( //(FAT_table->entry[free_clusters[0] + bs->fat32_first_cluster] != LAST_FILE_CLUSTER) &&
		(i > 0) && FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] != FREE_CLUSTER && FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] != FREE_USED_ENTRY) {
			--(fs_info->free_clusters);
		}
		*/
		// Cargo en el vector de numeros de bloques modificados el bloque a grabar
		// Verifico que ese bloque a grabar no sea uno que ya esta en el vector
		if (i == 0) {
			blocks_modified[0] = PFS_n_block_by_FAT_entry(n_entry);
			last_modified = 0;
		} else if (blocks_modified[last_modified] != PFS_n_block_by_FAT_entry(n_entry)) {
			blocks_modified = realloc(blocks_modified, sizeof(int) * (1 + (++last_modified)));
			blocks_modified[last_modified] = PFS_n_block_by_FAT_entry(n_entry);
		}
	}
	// Si es un cluster libre, como lo vamos a pisar con un usado disminuimos un libre. Si no era libre, no tenemos que disminuir
	if (FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] == FREE_CLUSTER || FAT_table->entry[free_clusters[i] + bs->fat32_first_cluster] == FREE_USED_ENTRY) {
		--(fs_info->free_clusters);
	}
	// Para el ultimo cluster debe tener 0xFF
	n_entry = free_clusters[i] + bs->fat32_first_cluster;
	FAT_table->entry[n_entry] = LAST_FILE_CLUSTER;
	fs_info->most_recently_alloc_cluster = free_clusters[i] + bs->fat32_first_cluster;
	if (n_free_clusters == 1) {
		blocks_modified[0] = PFS_n_block_by_FAT_entry(n_entry);
		last_modified = 0;
	}

	i = 0;
	while (FAT_table->entry[i] != FREE_ENTRY)
		i++;

	fs_info->most_recently_alloc_cluster = i - 1;

	// Actualizo el most_recently_alloc_cluster (primer bloque: bs y fs_info)
	memcpy(block, bs, sizeof(BootSector));
	memcpy(block->sector + 1, fs_info, sizeof(FSInfo));
	AddressPFS_write_block(0, block);

	// Voy cargando y grabando los bloques de la FAT en disco
	printf("[FAT_write_FAT_entry] Voy cargando y grabando los bloques de la FAT en disco\n");
	for (i = 0; i < last_modified + 1; i++) {
		printf("[FAT_write_FAT_entry] Bloque %d modificado a escribir: %d\n", i, blocks_modified[i]);
		PFS_set_block_by_FAT_entry(blocks_modified[i], block);
		AddressPFS_write_block(blocks_modified[i], block);
	}

	log_info(project_log, "[FAT_write_FAT_entry]", "%s%d%s", "Se escribieron ", n_free_clusters, " clusters en la FAT");

	Aux_printf("[FAT_write_FAT_entry] La FAT quedo asi: \n");
	//PFSPrint_FAT_table();

	free(block);
	free(blocks_modified);
}

void FAT_write_FAT_free_entry(int n_clusters, int* clusters) {
	Block* block = (Block*) malloc(sizeof(Block));
	int i, n_entry;
	int *blocks_modified = (int*) malloc(sizeof(int));
	int last_modified = -1;
	blocks_modified[0] = -1;

	// Pongo todos los clusters como libres en la FAT
	for (i = 0; i < n_clusters; i++) {
		// Si era un cluster usado, como lo vamos a liberar aumentamos la cantidad de libres. Si era libre no hay que aumentar
		if (FAT_table->entry[clusters[i] + bs->fat32_first_cluster] != FREE_CLUSTER && FAT_table->entry[clusters[i] + bs->fat32_first_cluster] != FREE_USED_ENTRY) {
			++(fs_info->free_clusters);
		}
		n_entry = clusters[i] + bs->fat32_first_cluster;
		// Donde tenia un n_cluster ahora lo pongo como cluster libre usado
		FAT_table->entry[n_entry] = FREE_USED_ENTRY;
		// Si era el ultimo cluster con contenido? El ultimo alocado puede ser 0xe5?
		// if (fs_info->most_recently_alloc_cluster == clusters[i]);
		// Cantidad de clusters libres cambia
		if (i == 0) {
			blocks_modified[0] = PFS_n_block_by_FAT_entry(n_entry);
			last_modified = 0;
		} else if (blocks_modified[last_modified] != PFS_n_block_by_FAT_entry(n_entry)) {
			blocks_modified = realloc(blocks_modified, sizeof(int) * (1 + (++last_modified)));
			blocks_modified[last_modified] = PFS_n_block_by_FAT_entry(n_entry);
		}
	}

	// Actualizo el most_recently_alloc_cluster (primer bloque: bs y fs_info)
	memcpy(block, bs, sizeof(BootSector));
	memcpy(block->sector + 1, fs_info, sizeof(FSInfo));
	AddressPFS_write_block(0, block);

	// Voy cargando y grabando los bloques de la FAT en disco
	for (i = 0; i < last_modified + 1; i++) {
		PFS_set_block_by_FAT_entry(blocks_modified[i], block);
		AddressPFS_write_block(blocks_modified[i], block);
	}

	free(block);
	free(blocks_modified);

	Aux_printf("[FAT_write_FAT_free_entry] La FAT quedo asi: \n");
	//PFSPrint_FAT_table();

	log_info(project_log, "[FAT_write_FAT_free_entry]", "%s%d%s", "Se escribieron ", n_clusters, " clusters en la FAT");
}

// Recibe un numero de cluster y devuelve el numero de cluster que se encuentra a x_clusters del primero
int FAT_x_n_cluster(int first_file_cluster, int x_clusters) {
	//Aux_printf("[FAT_x_n_cluster] first_file_cluster: %d, x_clusters: %d\n", first_file_cluster, x_clusters);
	int last_n_entry = first_file_cluster + bs->fat32_first_cluster;
	while (x_clusters > 0 && FAT_table->entry[last_n_entry] != LAST_FILE_CLUSTER) {
		last_n_entry = FAT_table->entry[last_n_entry];
		--x_clusters;
		//Aux_printf("[FAT_x_n_cluster] last_n_entry: %d\n", last_n_entry);
	}
	return last_n_entry - bs->fat32_first_cluster;
}
