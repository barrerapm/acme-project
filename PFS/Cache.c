#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PFS.h"
#include "Cache.h"
#include "AddressPFS.h"
#include "Type.h"
#include "Struct.h"
#include "Aux.h"

extern t_list* caches;
extern ConfigPFS config_pfs;

int Cache_contains_file_name(void* cache, void* file_name) {
	Cache* aux_cache = (Cache*) cache;
	char* this_file_name = (char*) file_name;
	return !strcmp(aux_cache->file_name, this_file_name);
}

int Cache_contains_n_cluster(void* cluster_info, void* n_cluster) {
	ClusterInfo* aux_cluster_info = (ClusterInfo*) cluster_info;
	int* this_n_cluster = (int*) n_cluster;
	return aux_cluster_info->n_cluster == *this_n_cluster;
}

void Cache_read_cluster(Cluster* cluster, int n_cluster, char* file_name) {
	// Apuntamos a la cache del archivo (siempre existe)
	Cache* this_cache;
	Aux_printf("[Cache_read_cluster] Voy a buscar la cache\n");
	this_cache = collection_list_find(caches, Cache_contains_file_name, file_name);
	if (this_cache != NULL && config_pfs.cache_size > 0) {
		// Buscamos si esta el n_cluster en la lista de cluster_info de la cache
		ClusterInfo *this_cluster_info;
		Aux_printf("[Cache_read_cluster] Voy a buscar el n_cluster en la cache\n");
		this_cluster_info = collection_list_find(this_cache->cluster_infos, Cache_contains_n_cluster, &n_cluster);

		// Si encontre el n_cluster en la cache, cargo el cluster a devolver
		if (this_cluster_info != NULL) {
			Aux_printf("[Cache_read_cluster] Encontre el cluster, lo agarro de la cache\n");
			memcpy(cluster, this_cluster_info->cluster, sizeof(Cluster));
		}
		// Si no encontre el n_cluster en la cache, lo leo del disco y lo meto en la cache
		else {
			Aux_printf("[Cache_read_cluster] No encontre el cluster %d lo voy a leer\n",n_cluster);
			AddressPFS_read_cluster(cluster, n_cluster);
			// Creo el cluster info y lo agrego al final de la lista
			Aux_printf("[Cache_read_cluster] Lo agrego a la cache\n");
			Cache_add_new(this_cache->cluster_infos, cluster, n_cluster, 0);
		}
	} else {
		AddressPFS_read_cluster(cluster, n_cluster);
	}
}

void Cache_write_cluster(int n_cluster, Cluster* cluster, char* file_name) {
	// Apuntamos a la cache del archivo (siempre existe)
	Cache* this_cache;
	this_cache = collection_list_find(caches, Cache_contains_file_name, file_name);
	if (this_cache != NULL && config_pfs.cache_size > 0) {
		// Buscamos si esta el n_cluster en la lista de cluster_info de la cache y lo eliminamos
		collection_list_removeByClosure2(this_cache->cluster_infos, Cache_contains_n_cluster, &n_cluster);

		// Creo el cluster info y lo agrego al final de la lista
		Cache_add_new(this_cache->cluster_infos, cluster, n_cluster, 1);
	} else {
		AddressPFS_write_cluster(n_cluster, cluster);
	}
}

// Creo el cluster info y lo agrego al final de la lista
void Cache_add_new(t_list* cluster_infos_list, Cluster* cluster, uint32_t n_cluster, int flush) {
	ClusterInfo* new_cluster_info = (ClusterInfo*) malloc(sizeof(ClusterInfo));
	new_cluster_info->flag_flush = flush;
	new_cluster_info->n_cluster = n_cluster;
	new_cluster_info->cluster = (Cluster*) malloc(sizeof(Cluster));
	memcpy(new_cluster_info->cluster, cluster, sizeof(Cluster));
	// Agrego el cluster_info a la lista de cluster_infos
	collection_list_add(cluster_infos_list, new_cluster_info);
	// Si ya está completa la lista de cluster_infos, borra el primer elemento de la lista
	ClusterInfo* removed_cluster_info;
	if (cluster_infos_list->elements_count * config_pfs.blocks_per_cluster > config_pfs.cache_size) {
		Aux_printf("[Cache_add_new] Lista completa, sacamos 1 cluster y lo escribimos si tiene flush\n");
		removed_cluster_info = collection_list_remove(cluster_infos_list, 0);
		if (removed_cluster_info->flag_flush) {
			Aux_printf("[Cache_add_new] Tiene flush, lo escribo\n");
			AddressPFS_write_cluster(removed_cluster_info->n_cluster, removed_cluster_info->cluster);
		}
		free(removed_cluster_info->cluster);
		free(removed_cluster_info);
	}
}

void Cache_destroy_cluster_info(void* cluster_info) {
	ClusterInfo* aux_cluster_info = (ClusterInfo*) cluster_info;
	free(aux_cluster_info->cluster);
}

void Cache_flush(Cache* this_cache) {
	int i;
	pthread_mutex_lock(&(this_cache->cluster_infos->external_mutex));
	ClusterInfo* aux_cluster_info = NULL;
	// Recorro la lista de clusters de la cache y vuelco a disco los clusters con flush
	for (i = 0; i < config_pfs.cache_size / config_pfs.blocks_per_cluster; i++) {
		Aux_printf("[Cache_flush] Obtengo un cluster de la cache, si tiene flush lo escribo\n");
		aux_cluster_info = collection_list_get(this_cache->cluster_infos, i);
		if (aux_cluster_info != NULL) {
			if (aux_cluster_info->flag_flush) {
				Aux_printf("[Cache_flush] Tiene flush, lo escribo\n");
				AddressPFS_write_cluster(aux_cluster_info->n_cluster, aux_cluster_info->cluster);
				aux_cluster_info->flag_flush = 0;
			}
		}
	}
	pthread_mutex_unlock(&(this_cache->cluster_infos->external_mutex));
}

void Cache_create_dump() {
	FILE* fd;
	fd = fopen("cache_dump.txt", "ab");

	int i, j;
	Cache* this_cache = NULL;
	ClusterInfo* this_cluster_info = NULL;

	// Por cada cache, imprimo su info
	if (caches != NULL) {
		Aux_printf("[Cache_create_dump] Elementos de la cache: %d\n", caches->elements_count);
		for (i = 0; i < caches->elements_count; i++) {
			this_cache = collection_list_get(caches, i);
			Aux_printf("[Cache_create_dump] Cache %d: %s\n", i, this_cache->file_name);
			fprintf(fd, "_______________________________________________________________\n");
			fprintf(fd, ">> Timestamp: %s\n\n", Aux_date_string());
			fprintf(fd, ">> Archivo: %s\n\n", this_cache->file_name);
			fprintf(fd, ">> Tamaño de Bloque de la Cache: %dKBs\n\n", config_pfs.cache_size);
			fprintf(fd, ">> Cantidad de Bloques de la Cache: %d\n\n", this_cache->cluster_infos->elements_count);

			// Por cada cluster_info imprimo el contenido del cluster
			for (j = 0; j < this_cache->cluster_infos->elements_count; j++) {
				this_cluster_info = collection_list_get(this_cache->cluster_infos, j);
				fprintf(fd, ">> Contenido del Bloque %d de la Cache:\n", j);
				fwrite(this_cluster_info->cluster, sizeof(Cluster), 1, fd);
				fprintf(fd, "\n");
			}
		}
	}
}
