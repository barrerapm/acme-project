#include <stdlib.h>
#include "FUSE.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "PFS.h"
#include "Type.h"
#include "log.h"
#include "FAT.h"
#include "Cache.h"
#include "AddressPFS.h"
#include "Aux.h"
#include "PFSPrint.h"
#include <pthread.h>

extern t_log* project_log;
extern t_list* caches;
extern BootSector* bs;
extern ConfigPFS config_pfs;
extern int total_size;

// Create and open a file // OK!
int FUSE_create(const char *path, mode_t mode, struct fuse_file_info *info) {
	char* thread = "[FUSE_create]";
	char* function = "Create";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Crear el archivo", path);

	// Verifica que se recibe el path como argumento
	if (!path) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	PFS_create_file(path);

	return 0;
}

// File open operation // OK!
int FUSE_open(const char *path, struct fuse_file_info *info) {
	char* thread = "[FUSE_open]";
	char* function = "Open";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s%s", "Abrir el archivo ", path);

	// Verifica que se recibe el path como argumento
	if (!path) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	// Verificamos si no existe el archivo, lo creamos
	if ((info->flags & 0xC0) == O_CREAT) {
		PFS_create_file(path);
		log_warning(project_log, thread, "%s", "No existe el archivo, se creo");
	}

	if (config_pfs.cache_size > 0) {
		// Buscamos la cache del archivo
		Cache* this_cache;
		char* file_name = PFS_path_name(path);
		this_cache = collection_list_find(caches, Cache_contains_file_name, file_name);

		// Si el archivo no está abierto (no existe su cache)
		if (this_cache == NULL) {
			Aux_printf("No existe la cache del archivo, la creo y la agrego a la lista\n");
			// Creamos la cache del archivo
			Cache* new_cache = (Cache*) malloc(sizeof(Cache));
			new_cache->cluster_infos = collection_list_create();
			new_cache->file_name = file_name;
			new_cache->n_opens = 1;

			// La agrego a la lista de caches global
			collection_list_add(caches, new_cache);
		}
		// Si el archivo estaba abierto, al abrirlo ahora aumentamos el contador de aperturas
		else {
			Aux_printf("Existe la cache del archivo, aumento el numero de aperturas\n");
			(this_cache->n_opens)++;
			free(file_name);
		}
	}
	return 0;
}

// Read data from an open file // OK!
int FUSE_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
	char* thread = "[FUSE_read]";
	char* function = "Read";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Leer el archivo", path);

	// Verifica que se recibe el path y el buf como argumento
	if (!path || !buf) {
		log_error(project_log, thread, "%s", "Faltan argumentos");

		return -EINVAL;
	}

	// Leemos los clusters del contenido del archivo y los cargamos en buf
	if (PFS_read_file_content(path, offset, buf, size) == -1) {
		log_error(project_log, thread, "%s", "Error al leer el contenido del archivo");
		return -1;
	}

	Aux_printf("[PFS_read_file_content] El bufer se cargo con el contenido del archivo:\n");
	total_size++;
	//printf("[FUSE_read] size: %d offset: %d - TOTAL_SIZE: %d\n", size, offset, total_size);

	int j;
	for (j = 0; j < size; j++) {
		Aux_printf("%c", buf[j]);
	}
	Aux_printf("\n");

	return size;
}

// Write data to an open file // FALTA TERMINAR
// 29-11 AGREGADA EN FAT.c LA FUNCION FAT_x_n_cluster
int FUSE_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
	char* thread = "[FUSE_write]";
	char* function = "Write";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Escribir el archivo", path);

	Aux_printf("[FUSE_write] Contenido del buf:\n");
	int b;
	for (b = 0; b < size; b++) {
		Aux_printf("%c", buf[b]);
	}
	Aux_printf("\n");

	// Verifica que se recibe el path y el buf como argumento
	if (!path || !buf) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));

	// Verificamos que exista el archivo y obtenemos sus entradas
	int n_entry, n_cluster_content;
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &n_entry, &n_cluster_content) == -1) {
		log_error(project_log, thread, "%s%s", "No existe el archivo ", path);
		//PFS_destroy_entries(long_dir_entry, dir_entry);
		free(long_dir_entry);
		free(dir_entry);
		return -ENOENT;
	}

	int file_size = dir_entry->file_size;
	// Verificamos si hay que agrandar el tamaño del archivo
	if (file_size < offset + size) {
		off_t aux_offset = offset + size;
		Aux_printf("[FUSE_write] Truncar hasta %d, Desde donde escribir: %ld, Lo que hay que escribir: %lu\n", aux_offset, offset, size);
		FUSE_truncate(path, aux_offset);

		if (file_size == 0) {
			if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &n_entry, &n_cluster_content) == -1) {
				log_error(project_log, thread, "%s%s", "No existe el archivo ", path);
				//PFS_destroy_entries(long_dir_entry, dir_entry);
				free(long_dir_entry);
				free(dir_entry);
				return -ENOENT;
			}
		}
	}

	int first_file_n_cluster = PFS_n_first_file_cluster(dir_entry);

	// Calculamos a partir de cuantos clusters escribimos (redondea para arriba, asi que despues restamos 1)
	int offset_clusters = PFS_n_clusters_by_bytes(offset);
	if ((offset % sizeof(Cluster)) == 0) {
		offset_clusters++;
	}
	if (offset_clusters == 0) {
		offset_clusters = 1;
	}

	Aux_printf("[FUSE_write] Primer cluster del archivo: %d, Clusters que me corro para escribir: %d\n", first_file_n_cluster, offset_clusters - 1);

	// Preparamos el vector de clusters a escribir

	// Calculamos el primer cluster a escribir
	int first_cluster_to_wr = FAT_x_n_cluster(first_file_n_cluster, offset_clusters - 1);
	Aux_printf("[FUSE_write] Primer cluster a escribir: %d\n", first_cluster_to_wr);

	free(long_dir_entry);
	free(dir_entry);

	// Calculamos el offset del primer cluster a escribir
	int first_offset = offset;
	if (offset >= (offset_clusters - 1) * sizeof(Cluster)) {
		first_offset = offset - (offset_clusters - 1) * sizeof(Cluster);
	}
	Aux_printf("[FUSE_write] Primer cluster a escribir, comienzo a escribir su byte: %d\n", first_offset);

	// Escribimos los bytes necesarios del primer cluster
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
//	AddressPFS_read_cluster(cluster, first_cluster_to_wr);
	char* file_name = PFS_path_name(path);
	Cache_read_cluster(cluster, first_cluster_to_wr, file_name);
	int first_cluster_buf_size = size;
	if (first_offset + size > sizeof(Cluster)) {
		first_cluster_buf_size = sizeof(Cluster) - first_offset;
	}
	Aux_printf("[FUSE_write] Cantidad de bytes a escribir en el primer cluster a escribir: %d\n", first_cluster_buf_size);
	int n_sector, n_block;
	PFS_n_block_n_sector_of_cluster_by_bytes(&first_offset, &n_block, &n_sector);
	Aux_printf("[FUSE_write] Escribo en el primer cluster desde su byte %d, su sector %d y su bloque %d\n", first_offset, n_sector, n_block);
	memcpy(cluster->block[n_block].sector[n_sector].byte + first_offset, buf, first_cluster_buf_size);
//	AddressPFS_write_cluster(first_cluster_to_wr, cluster);
	int* file_clusters_to_wr = (int*) malloc(sizeof(int));
	file_clusters_to_wr[0] = first_cluster_to_wr;
	Cache_write_cluster(first_cluster_to_wr, cluster, file_name);

	// Clusters completos escritos (sin contar el primero escrito)
	int full_written = 0;

	// Clusters completos a escribir (sin contar el primero escrito)
	int first_cluster_written_size = sizeof(Cluster) - first_offset;
	int full_clusters_to_wr = PFS_n_clusters_by_bytes(size - first_cluster_written_size) - 1;
	if (full_clusters_to_wr == -1) {
		full_clusters_to_wr = 0;
	}

	// Proximo cluster a escribir
	int next_cluster_to_write;

	// Escribimos todos los clusters completos, menos el ultimo a escribir
	while (full_written <= full_clusters_to_wr && full_written != 0) {
		Aux_printf("[FUSE_write] Clusters completos escritos: %d Clusters completos a escribir: %d\n", full_written, full_clusters_to_wr);
		next_cluster_to_write = FAT_x_n_cluster(first_cluster_to_wr, full_written);
//		AddressPFS_read_cluster(cluster, next_cluster_to_write);
		Cache_read_cluster(cluster, next_cluster_to_write, file_name);
		memcpy(cluster, buf + first_offset + full_written * sizeof(Cluster), sizeof(Cluster));
//		AddressPFS_write_cluster(next_cluster_to_write, cluster);
		file_clusters_to_wr[full_written] = next_cluster_to_write;
		Cache_write_cluster(next_cluster_to_write, cluster, file_name);
		full_written++;
	}

	//int n_file_clusters_to_wr = 1 + full_clusters_to_wr;

	// Escribimos el ultimo cluster a escribir, solo si hay que escribir al menos 2 clusters
	// ya que si habia que escribir 1 cluster, el primero hubiese sido el ultimo
	if (PFS_n_clusters_by_bytes(size) >= 2) {
		//n_file_clusters_to_wr++;
		// Escribimos los bytes necesarios del ultimo cluster
		next_cluster_to_write = FAT_x_n_cluster(first_cluster_to_wr, full_written);
		int last_cluster_to_write_size = offset + size - (first_cluster_to_wr + full_clusters_to_wr * sizeof(Cluster));
		Aux_printf("[FUSE_write] Primer cluster a escribir: %d, Proximo cluster a escribir: %d, Clusters completos escritos: %d\n", first_cluster_to_wr, next_cluster_to_write, full_written);
//		AddressPFS_read_cluster(cluster, next_cluster_to_write);
		Cache_read_cluster(cluster, next_cluster_to_write, file_name);
		memcpy(cluster, buf + first_offset + full_written * sizeof(Cluster), last_cluster_to_write_size);
//		AddressPFS_write_cluster(next_cluster_to_write, cluster);
		file_clusters_to_wr[full_written] = next_cluster_to_write;
		Cache_write_cluster(next_cluster_to_write, cluster, file_name);
	}

	//FAT_write_FAT_entry(n_file_clusters_to_wr, file_clusters_to_wr);

	free(file_name);

	return size;
}

// Possibly flush cached data // FALTA DEFINIR
int FUSE_flush(const char *path, struct fuse_file_info *info) {
	char* thread = "[FUSE_flush]";
	char* function = "Flush";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Flush del archivo", path);
	if (config_pfs.cache_size > 0) {
		Cache* this_cache;
		// Busco la cache, si no existe no hago nada
		char* file_name = PFS_path_name(path);

		if ((this_cache = collection_list_find(caches, Cache_contains_file_name, file_name)) == NULL) {
			return 0;
		}

		free(file_name);

		Cache_flush(this_cache);
	}
	return 0;
}

// Release an open file **/ // FALTA DEFINIR
int FUSE_release(const char *path, struct fuse_file_info *info) {
	char* thread = "[FUSE_release]";
	char* function = "Release";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Cerrar el archivo", path);

// Verifica que se recibe el path como argumento
	if (path == NULL) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	if (config_pfs.cache_size > 0) {
		// Apuntamos a la cache del archivo
		Cache* this_cache;
		char* file_name = PFS_path_name(path);
		this_cache = collection_list_find(caches, Cache_contains_file_name, file_name);

		if (this_cache == NULL) {
			Aux_printf("[FUSE_release] Archivo no abierto\n");
			free(file_name);
			return 0;
		}

		// Si el archivo está abierto y es el unico, borramos su cache
		if ((this_cache != NULL) && (this_cache->n_opens == 1)) {
			Aux_printf("[FUSE_release] El archivo esta abierto: %s, borramos su cache\n", this_cache->file_name);
			this_cache = collection_list_removeByClosure2(caches, Cache_contains_file_name, file_name);
			// Hacemos flush de la cache? por las dudas
			Cache_flush(this_cache);
			// Borramos la cache del archivo
			collection_list_destroy(this_cache->cluster_infos, Cache_destroy_cluster_info);
			free(this_cache->file_name);
			free(this_cache);
		}
		// Si el archivo estaba abierto mas de una vez, reducimos el contador de aperturas
		else if (this_cache != NULL) {
			Aux_printf("[FUSE_release] El archivo esta abierto varias veces, decrementamos las aperturas\n");
			(this_cache->n_opens)--;
		}
		free(file_name);
	}

	return 0;
}
// Changes the size of file **/ // FALTA TERMINAR
// 27-11 MODIFICADO EN FAT.c LA FUNCION GET_FILE_CLUSTERS EMPIEZA CON N_FILE_CL = 1
// Y TAMBIEN MODIFICADO GET_FILE_ENTRIES (VERIFICAR DEMAS FUNCIONES SI FUNCA OK)
int FUSE_truncate(const char *path, off_t offset) {
	char* thread = "[FUSE_truncate]";
	char* function = "Truncate";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Truncar el archivo", path);

	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	int long_n_entry, n_cluster_content, i, j;

	// Obtengo las entradas del archivo
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &long_n_entry, &n_cluster_content) == -1) {
		log_error(project_log, thread, "%s%s", "No existe el archivo ", path);
		//PFS_destroy_entries(long_dir_entry, dir_entry);
		free(long_dir_entry);
		free(dir_entry);
		return -ENOENT;
	}

	int file_size = dir_entry->file_size;
	Aux_printf("[FUSE_truncate] Tamaño del archivo: %d\n", file_size);
	Aux_printf("[FUSE_truncate] Cluster que contiene al archivo: %d\n", n_cluster_content);
	Aux_printf("[FUSE_truncate] Cantidad de entradas hasta la long: %d\n", long_n_entry);
	//PFSPrint_long_dir_entry(long_dir_entry);
	//PFSPrint_short_dir_entry(dir_entry);
	char* file_name = PFS_get_file_name(ATTR_LONG_NAME, long_dir_entry, dir_entry);
	Aux_printf("[FUSE_truncate] Nombre del archivo: %s\n", file_name);
	free(file_name);
	// Si quiero truncar el archivo al tamaño del archivo, no hago nada
	if (file_size == offset) {
		log_warning(project_log, thread, "%s", "El archivo ya tiene el tamaño al que se lo desea truncar");
		free(long_dir_entry);
		free(dir_entry);
		return 0;
	}

	// Le cargo a la entrada short el nuevo tamaño del archivo (el offset)
	dir_entry->file_size = offset;

	// Si el archivo tiene clusters, los obtengo todos (Ver si obtener solo clusters necesarios)
	int n_file_clusters = 1, *file_clusters = (int*) malloc(sizeof(int));
	int first_file_cluster = PFS_n_first_file_cluster(dir_entry);
	Aux_printf("[FUSE_truncate] first_file_cluster: %d\n", first_file_cluster);
	if (first_file_cluster >= 0) {
		file_clusters[0] = first_file_cluster;
		FAT_get_file_clusters(&file_clusters, &n_file_clusters);
		Aux_printf("[FUSE_truncate] Cantidad de clusters del archivo: %u\n", n_file_clusters);
	} else if (offset != 0) {
		Aux_printf("[FUSE_truncate] El archivo no tiene asignado un cluster\n");
		int *free_cluster = (int*) malloc(sizeof(int));
		// obtener 1 cluster libre para el contenido del archivo
		if (FAT_get_free_clusters(1, &free_cluster) == -1) {
			free(free_cluster);
			log_warning(project_log, "[FUSE_truncate]", "%s", "No se pudo obtener un cluster libre");
			return -1;
		}
		first_file_cluster = free_cluster[0];
		PFS_set_first_n_cluster_entry(first_file_cluster + bs->fat32_first_cluster, long_dir_entry, dir_entry);
		FAT_write_FAT_entry(1, free_cluster);
		free(free_cluster);

		// Si quiero truncar el archivo al tamaño del archivo, no hago nada
		if (file_size == offset) {
			log_warning(project_log, thread, "%s", "El archivo ya tiene el tamaño al que se lo desea truncar");
			free(long_dir_entry);
			free(dir_entry);
			return 0;
		}
	}
	Aux_printf("[FUSE_truncate] Primer cluster del archivo: %u\n", first_file_cluster);

	Cluster* empty_cluster;
	int* new_clusters;
	int bytes_last_cluster_to_offset = offset - (n_file_clusters * sizeof(Cluster));
	Aux_printf("[FUSE_truncate] Offset hasta donde truncar: %d, Cantidad de clusters del archivo: %d\n", offset, n_file_clusters);
	Aux_printf("[FUSE_truncate] Bytes desde el comienzo del ultimo cluster hasta el offset: %d\n", bytes_last_cluster_to_offset);
	// Si offset mayor que el tamaño de archivo, puede que haya que agregar clusters a la FAT
	if (offset > file_size) {
		Aux_printf("[FUSE_truncate] Offset hasta donde truncar > tamaño del archivo\n");
		// Me fijo si tengo que agregarle clusters al archivo
		if (bytes_last_cluster_to_offset > 0) {
			Aux_printf("[FUSE_truncate] Bytes desde el comienzo del ultimo cluster hasta el offset > 0\n");
			int cluster_to_add = PFS_n_clusters_by_bytes(bytes_last_cluster_to_offset);
			Aux_printf("[FUSE_truncate] Cluster a agregar al archivo: %d\n", cluster_to_add);
			int* free_clusters = (int*) malloc(sizeof(int));
			FAT_get_free_clusters(cluster_to_add, &free_clusters);
			Aux_printf("[FUSE_truncate] Obtuve %d clusters libres\n", cluster_to_add);
			//printf("[FUSE_trucate] free[0]: %d free[1]: %d\n", free_clusters[0], free_clusters[1]);
			// Verifico si no tenia clusters, entonces le agrego todos los free obtenidos
			if (file_size == 0) {
				//FAT_write_FAT_entry(cluster_to_add, free_clusters);
				//PFS_set_first_n_cluster_entry(/*free_clusters[0]*/first_file_cluster + bs->fat32_first_cluster, long_dir_entry, dir_entry);

				new_clusters = (int*) malloc(sizeof(int));
				new_clusters[0] = first_file_cluster;
				for (i = 1, j = 0; i < cluster_to_add + 1; i++, j++) {
					new_clusters = realloc(new_clusters, (1 + i) * sizeof(int));
					new_clusters[i] = free_clusters[j];
				}
				Aux_printf("[FUSE_trucate] Voy a escribir los clusters en la FAT, [0]:%d [1]:%d [2]:%d\n", new_clusters[0], new_clusters[1], new_clusters[2]);
				FAT_write_FAT_entry(cluster_to_add + 1, new_clusters);
				Aux_printf("[FUSE_trucate] Escribi los clusters en la FAT\n");
				free(new_clusters);
			}

			// Si no, ya tenia clusters. Hay que encadenarlos
			else if (file_size > 0) {
				new_clusters = (int*) malloc(sizeof(int));
				// Pongo como primer elemento el ultimo cluster que tenia el archivo
				new_clusters[0] = file_clusters[n_file_clusters - 1];
				Aux_printf("[FUSE_truncate] Primero de los nuevos clusters a agregar: %d\n", new_clusters[0]);
				empty_cluster = (Cluster*) malloc(sizeof(Cluster));
				PFS_clean_cluster(empty_cluster);
				for (i = 1, j = 0; i < cluster_to_add + 1; i++, j++) {
					new_clusters = realloc(new_clusters, (1 + i) * sizeof(int));
					new_clusters[i] = free_clusters[j];
					Aux_printf("[FUSE_truncate] new_clusters[%d]: %d\n", i, new_clusters[i]);
					AddressPFS_write_cluster(new_clusters[i], empty_cluster);
				}
				FAT_write_FAT_entry(cluster_to_add + 1, new_clusters);
				free(new_clusters);
			}
		}
	}

	// Si offset es 0, cluster high/low en 0 y todos sus clusters en la FAT como libres
	if (offset == 0) {
		PFS_set_first_n_cluster_entry(/*first_file_cluster*/0, long_dir_entry, dir_entry);
		FAT_write_FAT_free_entry(n_file_clusters, file_clusters);
	}

	// Grabo la entrada short modificada en la carpeta del archivo
	Cluster* entry_cluster = (Cluster*) malloc(sizeof(Cluster));
	AddressPFS_read_cluster(entry_cluster, n_cluster_content);
	int n_block, n_sector, offset_entry = long_n_entry;
	PFS_n_block_n_sector_of_cluster_by_bytes(&offset_entry, &n_block, &n_sector);
	memcpy(&(entry_cluster->block[n_block].sector[n_sector].byte[((offset_entry + 1) * ENTRY_SIZE)]), dir_entry, ENTRY_SIZE);
	AddressPFS_write_cluster(n_cluster_content, entry_cluster);
	free(entry_cluster);
	free(long_dir_entry);
	free(dir_entry);

	// Si offset menor que el tamaño del archivo, puede que haya que liberar clusters en la FAT
	if (offset < file_size && offset != 0) {
		int bytes_last_cluster_to_offset = (n_file_clusters - 1) * sizeof(Cluster) - offset;
		// Verifico si hay que borrar clusters de la FAT
		if (bytes_last_cluster_to_offset > 0) {
			int clusters_to_del = PFS_n_clusters_by_bytes(bytes_last_cluster_to_offset);
			int* del_clusters = (int*) malloc(sizeof(int));
			int i, j;
			for (i = n_file_clusters - clusters_to_del, j = 0; i < n_file_clusters; i++, j++) {
				del_clusters = realloc(del_clusters, (1 + j) * sizeof(int));
				del_clusters[j] = file_clusters[i];
			}
			FAT_write_FAT_free_entry(clusters_to_del, del_clusters);
			free(del_clusters);
			// PERO AHORA AL ULTIMO CLUSTER QUE QUEDO HAY QUE PONERLO EN FFFFFF

			int* last_cluster = (int*) malloc(sizeof(int));
			last_cluster[0] = file_clusters[n_file_clusters - clusters_to_del - 1];
			FAT_write_FAT_entry(1, last_cluster);
			free(last_cluster);

		}
	}

	free(file_clusters);

	return 0;
}

int FUSE_ftruncate(const char *path, off_t offset, struct fuse_file_info *info) {

	return FUSE_truncate(path, offset);
}

// Remove a file // OK!
int FUSE_unlink(const char *path) {
	char* thread = "[FUSE_unlink]";
	char* function = "Unlink";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Borrar el archivo", path);

	// Verifica que se recibe el path como argumento
	if (!path) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	// Grabar en disco las entradas como libres ya usadas (0xe5)
	if (PFS_set_path_entries_free(path) == -1) {
		log_error(project_log, thread, "%s %s", "Error al borrar el archivo", path);
		return -1;
	}

	return 0;
}

// Create a directory // OK!
int FUSE_mkdir(const char *path, mode_t mode) {
	char* thread = "[FUSE_mkdir]";
	char* function = "Mkdir";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Crear la carpeta", path);

	PFS_create_dir(path);

	return 0;
}

// Read directory // OK!
int FUSE_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info) {
	(void) offset;
	(void) info;
	char* thread = "[FUSE_readdir]";
	char* function = "Readdir";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Leer la carpeta", path);

	char** file_names = (char**) malloc(sizeof(char*));
	int i, n_file_names = 0;

	// Obtener los nombres de los archivos/carpetas que contenga el directorio
	if (PFS_directory_list(path, &file_names, &n_file_names) == -1) {
		log_error(project_log, thread, "%s %s", "Error al listar directorio", path);
	}

	// Pasarle al filler todos los nombres
	if(strcmp(path,"/")){
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for (i = 0; i < n_file_names; i++)
		filler(buf, file_names[i], NULL, 0);
	} else {
		for (i = 0; i < n_file_names; i++)
				filler(buf, file_names[i], NULL, 0);
	}

	for (i = 0; i < n_file_names; i++) {
		Aux_printf("Archivo: %s\n", file_names[i]);
	}

	Aux_string_vec_destroy(&file_names, n_file_names);
	free(file_names);

	return 0;
}

// Remove a directory // Ok!
int FUSE_rmdir(const char *path) {
	char* thread = "[FUSE_rmdir]";
	char* function = "Rmdir";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s%s", "Remover el directorio", path);

	// Grabar en disco las entradas como libres ya usadas (0xe5)
	if (PFS_set_path_entries_free(path) == -1) {
		log_error(project_log, thread, "%s %s", "Error al borrar la carpeta", path);
		return -1;
	}

	return 0;
}

// Get file attributes. // OK!
int FUSE_getattr(const char *path, struct stat *st) {
	char* thread = "[FUSE_getattr]";
	char* function = "Getattr";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Obtener atributos del archivo", path);

	// Verifica que se recibe el path como argumento
	if (path == NULL || path == '\0') {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	int res = 0;
	memset(st, 0, sizeof(struct stat));

	if (!strcmp(path, "/")) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
		return res;
	}

	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));

	// Verificamos que exista el archivo y obtenemos sus entradas
	int n_entry, n_cluster_content;
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &n_entry, &n_cluster_content) == -1) {
		log_error(project_log, thread, "%s%s", "No existe el archivo ", path);
		free(long_dir_entry), free(dir_entry);
		return -ENOENT;
	}

	//PFSPrint_short_dir_entry(dir_entry);

	// Si es un archivo: cargar en stat permisos de archivo, tamaño y n_link = 1
	if (dir_entry->file_attributes == ATTR_DIRECTORY) {
		Aux_printf("%s es una Carpeta\n", path);
		st->st_mode = S_IFDIR | 0777/*0755*/;
		st->st_nlink = 2;
	}
	// Si es una carpeta o "/"?: cargar en stat permisos de carpeta y n_link = 2
	else if (dir_entry->file_attributes == ATTR_ARCHIVE) {
		Aux_printf("%s es un Archivo\n", path);
		st->st_mode = S_IFREG | 0777/*0444*/;
		st->st_nlink = 1;
		st->st_size = dir_entry->file_size;
	} else {
		Aux_printf("%s no es una Carpeta ni un Archivo\n", path);
		res = -ENOENT;
		log_error(project_log, thread, "%s %s", path, "No es un archivo o carpeta");
	}

	free(long_dir_entry), free(dir_entry);
	return res;

}

int FUSE_fgetattr(const char *path, struct stat *st, struct fuse_file_info *info) {
	return FUSE_getattr(path, st);
}

// Rename a file // OK!
int FUSE_rename(const char *path, const char *new_path) {
	char* thread = "[FUSE_rename]";
	char* function = "Rename";
	Aux_print_header(function, path);

	log_debug(project_log, thread, "%s %s", "Renombrar el archivo", path);

	// Verifica que se recibe el src y el dest como argumento
	if (path == NULL || new_path == NULL) {
		log_error(project_log, thread, "%s", "Faltan argumentos");
		return -EINVAL;
	}

	// Si origen/destino son iguales, ignorar la llamada*/
	if (!strcmp(path, new_path)) {
		log_warning(project_log, thread, "%s", "El path de origen y destino son iguales");
		return 0;
	}

	if (PFS_rename_path(path, new_path) == -1) {
		log_error(project_log, thread, "%s %s", "No se pudo renombrar el archivo", path);
	}

	return 0;
}

