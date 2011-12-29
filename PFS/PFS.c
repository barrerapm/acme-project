#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
//#include "Struct.h"
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include "Addressing.h"
#include "Console.h"
#include "PFS.h"
#include "FAT.h"
#include "Socket.h"
#include "Disk.h"
#include "PFSPrint.h"
#include "FUSE.h"
#include "utils.h"
#include "Aux.h"
#include "log.h"
#include "Type.h"
#include "AddressPFS.h"
#include "Cache.h"
#include <ctype.h>

ConfigPFS config_pfs; // Estructura global config_ppd
BootSector* bs;
FSInfo* fs_info;
TablaFAT* FAT_table;
t_log* project_log;

// Declaramos vector de conexiones
Connection* connections_set;

// Creamos el semaforo para el conecction_set
sem_t* sem_cont_connections;

pthread_t consola_PFS;

t_list* caches;
int total_size;

int CLUS = 0; // Para pruebas de file list
static struct fuse_operations operaciones =
		{ .create = FUSE_create, .open = FUSE_open, .read = FUSE_read, .write = FUSE_write, .flush = FUSE_flush, .release = FUSE_release, .truncate = FUSE_truncate, .ftruncate = FUSE_ftruncate,
				.unlink = FUSE_unlink, .mkdir = FUSE_mkdir, .readdir = FUSE_readdir, .rmdir = FUSE_rmdir, .fgetattr = FUSE_fgetattr, .getattr = FUSE_getattr, .rename = FUSE_rename, };

int main(int argc, char** argv) {
	total_size = 0;
	caches = collection_list_create();
	project_log = log_create("Runner", "log.txt", DEBUG | INFO | WARNING | ERROR, M_CONSOLE_ENABLE);

	signal(SIGUSR1, (void*) &Cache_create_dump);

	// Config PFS
	PFS_inicializar_config();
	// PFSPrint_config_pfs(); // Prueba

	Aux_printf("ESTOY ANTES CONFIG\n");

	Aux_printf("ESTOY ANTES DEL SEMAFORO CONTADOR\n");
	sem_cont_connections = (sem_t*) malloc(sizeof(sem_t));
	sem_init(sem_cont_connections, 0, config_pfs.socket_config.max_connections);

	// Inicializamos vector de conexiones
	connections_set = (Connection*) malloc(sizeof(Connection) * config_pfs.socket_config.max_connections);

	// Cargamos el vector de conexiones
	int i;
	for (i = 0; i < config_pfs.socket_config.max_connections; i++) {
		Aux_printf("FOR cliclo: %d\n", i);
		connections_set[i].state = 0;

		//Abrimos el socket cliente
		if ((connections_set[i].client_fd = Socket_open_client_inet(config_pfs.socket_config, i + 1)) == -1) {
			Aux_printf(" %d\nFOR ERROR", i);
			log_error(project_log, "[main]", "%s", "Error al abrir un client inet");
			return -1; /* Agregar log de error */
		}

		// Realizamos la conexion con el server
		if (Socket_connect_to_server_inet(config_pfs.socket_config, connections_set[i].client_fd) == -1) {
			Aux_printf("VOY A MORIR, YEP YEP!\n");
			log_error(project_log, "[main]", "%s", "Error al conectar con el server inet");
			return -1;
		}

		//Handshake
		if (Addressing_disk_operation(connections_set, sem_cont_connections, TYPE_HANDSHAKE) == -1) {
			Aux_printf("VOY A SALIR, YEP YEP!\n");
			//PFS_exit();
			log_error(project_log, "[main]", "%s", "Error al realizar un Handshake de un cliente");
			return -1;
		}
	}

	// Boot Sector
	bs = (BootSector*) malloc(sizeof(BootSector));
	Disk_load_bootsector();

	// FS Info
	fs_info = (FSInfo*) malloc(sizeof(FSInfo));
	Disk_load_FSInfo();

	// FAT Region
	FAT_table = (TablaFAT*) malloc(sizeof(TablaFAT));
	FAT_table->entry = (uint32_t*) malloc(bs->fat32_sectors_per_fat * bs->bytes_per_sector/*TablaFAT*/);
	Disk_load_FAT();

	// Consola PFS
	pthread_create(&consola_PFS, NULL, (void*) &Console_PFS, NULL);

	//pthread_join(consola_PFS, NULL); /* POR AHORA QUEDA, DESPUES NO ESTOY SEGURO, PERO CAPAZ SE VA */

	return fuse_main(argc, argv, &operaciones);
}

void PFS_clean_cluster(Cluster* cluster) {
	int n_block, n_sector, n_byte;
	for (n_block = 0; n_block < config_pfs.blocks_per_cluster; n_block++)
		for (n_sector = 0; n_sector < config_pfs.sectors_per_block; n_sector++)
			for (n_byte = 0; n_byte < config_pfs.bytes_per_sector; n_byte++)
				cluster->block[n_block].sector[n_sector].byte[n_byte] = '\0';
}

void PFS_reload_entries(LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	PFS_destroy_entries(long_dir_entry, dir_entry);
	long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
}

int PFS_create_file(const char* path) {
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	int aux_entry, n_cluster_content;

	// Buscamos las entradas del archivo. Si ya existe, no lo creamos
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &aux_entry, &n_cluster_content) == 0) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_create_file]", "%s", "El archivo ya existe");
		return -1;
	}

	// Obtenemos 1 cluster libre para el contenido del archivo
	int *free_clusters = (int*) malloc(sizeof(int));
	if (FAT_get_free_clusters(1, &free_clusters) == -1) {
		free(free_clusters);
		log_warning(project_log, "[PFS_create_file]", "%s", "No se pudo obtener un cluster libre");
		return -1;
	}

	// Limpiamos los datos de las entradas, para reutilizarlas
	PFS_clean_entries(long_dir_entry, dir_entry);

	// Inicializamos las entradas del archivo
	unsigned char* path_name = (unsigned char*) PFS_path_name(path);
	PFS_init_entries(ATTR_ARCHIVE, path_name, free_clusters[0], long_dir_entry, dir_entry);
	free(path_name), free(free_clusters);

	// Escribimos las entradas en su carpeta
	if (PFS_write_entries(n_cluster_content, long_dir_entry, dir_entry) == -1) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_create_file]", "%s", "No se pudieron escribir las entradas del archivo");
		return -1;
	}

	free(long_dir_entry), free(dir_entry);
	log_info(project_log, "[PFS_create_file]", "%s %s", "Se creo el archivo", path);
	return 0;
}

int PFS_create_dir(const char* path) {
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	int aux_entry, n_cluster_content;

	// Buscamos las entradas de la carpeta. Si existe, no la creamos
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &aux_entry, &n_cluster_content) == 0) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_create_file]", "%s", "La carpeta ya existe");
		return -1;
	}

	// Obtenemos 1 cluster libre para el contenido de la carpeta
	int *free_clusters = (int*) malloc(sizeof(int));
	if (FAT_get_free_clusters(1, &free_clusters) == -1) {
		free(free_clusters);
		log_warning(project_log, "[PFS_create_dir]", "%s", "No se pudo obtener un cluster libre");
		return -1;
	}

	// Limpiamos los datos de las entradas, para reutilizarlas
	PFS_clean_entries(long_dir_entry, dir_entry);

	// Inicializamos las entradas de la carpeta
	unsigned char* path_name = (unsigned char*) PFS_path_name(path);
	PFS_init_entries(ATTR_DIRECTORY, path_name, free_clusters[0], long_dir_entry, dir_entry);
	free(path_name);

	// Escribimos las entradas en su carpeta
	if (PFS_write_entries(n_cluster_content, long_dir_entry, dir_entry) == -1) {
		free(long_dir_entry), free(dir_entry), free(free_clusters);
		log_warning(project_log, "[PFS_create_dir]", "%s", "No se pudieron escribir las entradas de la carpeta");
		return -1;
	}

	// Limpiamos los datos de las entradas, para reutilizarlas
	PFS_clean_entries(long_dir_entry, dir_entry);

	// Preparamos la entrada .
	PFS_init_entries(ATTR_DIRECTORY, (unsigned char*) ".", free_clusters[0], long_dir_entry, dir_entry);

	// Escribimos la entrada . en su carpeta
	if (PFS_write_entries(free_clusters[0], long_dir_entry, dir_entry) == -1) {
		free(long_dir_entry), free(dir_entry), free(free_clusters);
		log_warning(project_log, "[PFS_create_dir]", "%s", "No se pudo escribir la entrada . de la carpeta");
		return -1;
	}

	// Limpiamos los datos de las entradas, para reutilizarlas
	PFS_clean_entries(long_dir_entry, dir_entry);

	// Preparamos la entrada ..
	PFS_init_entries(ATTR_DIRECTORY, (unsigned char*) "..", n_cluster_content, long_dir_entry, dir_entry);

	// Escribimos la entrada .. en su carpeta
	if (PFS_write_entries(free_clusters[0], long_dir_entry, dir_entry) == -1) {
		free(long_dir_entry), free(dir_entry), free(free_clusters);
		log_warning(project_log, "[PFS_create_dir]", "%s", "No se pudo escribir la entrada .. de la carpeta");
		return -1;
	}

	free(long_dir_entry), free(dir_entry), free(free_clusters);
	log_info(project_log, "[PFS_create_dir]", "%s %s", "Se creo la carpeta", path);
	return 0;
}

int PFS_rename_path(const char* path, const char* new_path) {
	int aux_content, aux_entry, n_cluster_content;

	// Obtenemos el numero de cluster de la carpeta del new_path
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	int new_entry;
	if (PFS_get_path_entries(new_path, long_dir_entry, dir_entry, &new_entry, &n_cluster_content) == -1) {
		Aux_printf("No se encontro el archivo %s, obtuve el cluster de la carpeta\n", new_path);
	} else {
		Aux_printf("El archivo %s ya existe, se reemplaza\n", new_path);
	}

	// Limpiamos las entradas para reutilizarlas
	PFS_clean_entries(long_dir_entry, dir_entry);

	// Obtenemos las entradas del archivo/carpeta path
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &aux_entry, &aux_content) == -1) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_rename_path]", "%s %s", "No se encontro la entrada del archivo", path);
		return -1;
	}
	int n_block, n_sector, offset_entry;

	Cluster* cluster;

	// Leo el cluster de la carpeta del archivo/carpeta
	cluster = (Cluster*) malloc(sizeof(Cluster));
	AddressPFS_read_cluster(cluster, aux_content);

	long_dir_entry->sequence_number = FREE_USED_ENTRY;
	uint8_t aux_attr = dir_entry->file_attributes;
	dir_entry->file_attributes = FREE_ENTRY;

	// Calculo en que byte de que sector de que bloque está la entrada long del archivo
	offset_entry = aux_entry;

	// Modifico el cluster, marcando las entradas como libres usadas y lo grabo en Data Region
	int entry_off = offset_entry * ENTRY_SIZE;
	PFS_n_block_n_sector_of_cluster_by_bytes(&entry_off, &n_block, &n_sector);
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[entry_off]), long_dir_entry, ENTRY_SIZE);
	entry_off = (offset_entry + 1) * ENTRY_SIZE;
	PFS_n_block_n_sector_of_cluster_by_bytes(&entry_off, &n_block, &n_sector);
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[entry_off]), dir_entry, ENTRY_SIZE);

	AddressPFS_write_cluster(aux_content, cluster);

	long_dir_entry->sequence_number = LAST_LONG_ENTRY;
	dir_entry->file_attributes = aux_attr;

	// Cambiarle nombre a las entradas y desmarcarlas como FREE_USED_ENTRY
	unsigned char* new_path_name = (unsigned char*) PFS_path_name(new_path);
	PFS_set_file_name(new_path_name, long_dir_entry, dir_entry);
	free(new_path_name);

	// Leo el cluster de la carpeta del archivo/carpeta
	AddressPFS_read_cluster(cluster, n_cluster_content);

	// Calculo en que byte de que sector de que bloque está la entrada long del archivo
	if (aux_content != n_cluster_content) {
		offset_entry = new_entry;
	} else {
		int x_cluster;
		LongDirEntry* aux_long = (LongDirEntry*) malloc(sizeof(LongDirEntry));
		DirEntry* aux_dir = (DirEntry*) malloc(sizeof(DirEntry));
		PFS_get_path_entries(path, aux_long, aux_dir, &offset_entry, &x_cluster);
		free(aux_long), free(aux_dir);
	}
	PFS_n_block_n_sector_of_cluster_by_bytes(&offset_entry, &n_block, &n_sector);
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[offset_entry * ENTRY_SIZE]), long_dir_entry, ENTRY_SIZE);
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[(offset_entry + 1) * ENTRY_SIZE]), dir_entry, ENTRY_SIZE);

	AddressPFS_write_cluster(n_cluster_content, cluster);

	free(long_dir_entry), free(dir_entry);
	log_info(project_log, "[PFS_rename_path]", "%s %s%s %s", "Se renombro el archivo", path, ". Ahora es", new_path);
	return 0;
}

// Escribe las entradas long y short en su carpeta y las graba en la FAT
// No se usa cuando esta el archivo abierto, no es necesario la cache
int PFS_write_entries(int n_cluster_content, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	int long_n_entry, entries_flag;

	// Leo el cluster de la carpeta donde vamos a grabar las entradas del archivo/carpeta
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	AddressPFS_read_cluster(cluster, n_cluster_content);

	// Obtengo entradas long y short vacias y el numero de la long vacia
	LongDirEntry* aux_long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	DirEntry* aux_dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	if ((entries_flag = PFS_get_file_entries(cluster, "", aux_long_dir_entry, aux_dir_entry, &long_n_entry)) == -1) {
		Aux_printf("No hay entradas vacias en el cluster\n");
		// Verificar en la FAT si hay para buscar en otro cluster
		return -1;
	}
	free(aux_long_dir_entry), free(aux_dir_entry);

	// Calculo los bloques, sectores y bytes del cluster donde empieza la entrada long
	int n_sector, n_block, offset_entry = long_n_entry;
	PFS_n_block_n_sector_of_cluster_by_bytes(&offset_entry, &n_block, &n_sector);

	// Escribo las entradas long y short en el cluster
	if (entries_flag != -3 || n_cluster_content == 0) {
		memcpy(&(cluster->block[n_block].sector[n_sector].byte[offset_entry * ENTRY_SIZE]), long_dir_entry, ENTRY_SIZE);
		memcpy(&(cluster->block[n_block].sector[n_sector].byte[(offset_entry + 1) * ENTRY_SIZE]), dir_entry, ENTRY_SIZE);
	}
	// Para el caso de una carpeta nueva, escribo una sola entrada short . o ..
	else {
		memcpy(&(cluster->block[n_block].sector[n_sector].byte[offset_entry * ENTRY_SIZE]), dir_entry, ENTRY_SIZE);
	}

	// Escribo el cluster modificado en el Data Region
	AddressPFS_write_cluster(n_cluster_content, cluster);
	free(cluster);

	// Escribo en la FAT la entrada del contenido del archivo/carpeta
	int* file_clusters = (int*) malloc(sizeof(int));
	file_clusters[0] = PFS_n_first_file_cluster(dir_entry);
	FAT_write_FAT_entry(1, file_clusters);
	free(file_clusters);

	return 0;
}

void PFS_reload_cluster(Cluster* cluster) {
	free(cluster);
	cluster = (Cluster*) malloc(sizeof(Cluster));
}

// Recibe el path de un archivo/carpeta, recorre las carpetas del path y carga las entradas long y short
// No sirve para path "/"

int32_t PFS_get_path_entries(const char* path, LongDirEntry* long_dir_entry, DirEntry* dir_entry, int* n_entry, int* n_cluster_content) {
	char** dir_names = (char**) malloc(sizeof(char*));
	int n_names, i = 0, result = 0;

	// Obtenemos las carpetas del path a las que hay que entrar para llegar a la que necesitamos
	Aux_get_tokens_by("/", path, &dir_names, &n_names);

	// Inicializamos el numero del cluster donde comenzamos a recorrer el path (desde "/")
	(*n_cluster_content) = 0;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	// Buscamos cada carpeta del path en su carpeta anterior hasta tener las entradas long y short del archivo
	while (i < n_names) {
		AddressPFS_read_cluster(cluster, *n_cluster_content);
		(*n_entry) = 0;
		//printf("Voy a buscar el archivo: %s en el cluster: %d\n", dir_names[i], *n_cluster_content);
		if (PFS_get_file_entries(cluster, dir_names[i++], long_dir_entry, dir_entry, n_entry) != 0) {
			log_warning(project_log, "[PFS_get_path_entries]", "%s", "No se pudieron obtener las entradas del archivo");
			result = -1;
			break;
		}
		// Si hay proxima iteracion, buscamos el proximo archivo/carpeta en esta carpeta encontrada
		// Si no, me quedé con el cluster de la carpeta que contiene al archivo/carpeta
		if (i < n_names) {
			(*n_cluster_content) = PFS_n_first_file_cluster(dir_entry);
			//PFS_clean_entries2(long_dir_entry,dir_entry);
		}
	}

	//PFSPrint_short_dir_entry(dir_entry);
	//PFSPrint_long_dir_entry(long_dir_entry);
	Aux_string_vec_destroy(&dir_names, n_names);
	free(dir_names), free(cluster);
	return result;
}

/*
 int32_t PFS_get_path_entries(const char* path, LongDirEntry* long_dir_entry, DirEntry* dir_entry, int* n_entry, int* n_cluster_content) {
 char** dir_names = (char**) malloc(sizeof(char*));
 int n_names, i = 0, result = 0;

 // Obtenemos las carpetas del path a las que hay que entrar para llegar a la que necesitamos
 Aux_get_tokens_by("/", path, &dir_names, &n_names);

 // Inicializamos el numero del cluster donde comenzamos a recorrer el path (desde "/")
 (*n_cluster_content) = 0;
 Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));

 // Buscamos cada carpeta del path en su carpeta anterior hasta tener las entradas long y short del archivo
 while (i < n_names) {
 AddressPFS_read_cluster(cluster, *n_cluster_content);
 (*n_entry) = 0;
 if (PFS_get_file_entries(cluster, dir_names[i++], long_dir_entry, dir_entry, n_entry) != 0) {
 log_warning(project_log, "[PFS_get_path_entries]", "%s", "No se pudieron obtener las entradas del archivo");
 result = -1;
 break;
 }
 // Si hay proxima iteracion, buscamos el proximo archivo/carpeta en esta carpeta encontrada
 // Si no, me quedé con el cluster de la carpeta que contiene al archivo/carpeta
 if (i < n_names) {
 (*n_cluster_content) = PFS_n_first_file_cluster(dir_entry);
 }
 }

 Aux_string_vec_destroy(&dir_names, n_names);
 free(dir_names), free(cluster);
 return result;
 }

 */
void PFS_clean_entries(LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	int i, zero = 0;
	for (i = 0; i < sizeof(DirEntry); i++) {
		memcpy(long_dir_entry, &zero, 1);
		memcpy(dir_entry, &zero, 1);
	}
}

void PFS_clean_entries2(LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	long_dir_entry->attributes = 0;
	long_dir_entry->checksum = 0;
	long_dir_entry->first_cluster_n = 0;
	long_dir_entry->name1[0] = 0;
	long_dir_entry->name1[1] = 0;
	long_dir_entry->name1[2] = 0;
	long_dir_entry->name1[3] = 0;
	long_dir_entry->name1[4] = 0;
	long_dir_entry->reserved = 0;
	long_dir_entry->sequence_number = 0;
	dir_entry->dos_file_extension[0] = 0;
	dir_entry->reserved = 0;
	dir_entry->file_attributes = 0;
	dir_entry->dos_file_name[0] = 0;
	dir_entry->dos_file_name[1] = 0;
	dir_entry->dos_file_name[2] = 0;
	dir_entry->dos_file_name[3] = 0;
	dir_entry->fst_cluster_low = 0;
	dir_entry->fst_cluster_high = 0;
	dir_entry->file_size = 0;
}

int PFS_set_path_entries_free(const char* path) {
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int long_n_entry, n_cluster_content;

	// Obtenemos las entradas long y short del archivo/carpeta
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &long_n_entry, &n_cluster_content) == -1) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_set_path_entries_free]", "%s", "No existe el archivo");
		return -1;
	}

	char** file_names = (char**) malloc(sizeof(char*));
	int i = 0, n_file_names;
	char* aux_path;
	if (dir_entry->file_attributes == ATTR_DIRECTORY) {
		PFS_directory_list(path, &file_names, &n_file_names);
		while (i < n_file_names) {
			aux_path = Aux_strings_concatenate((char*) path, file_names[i++]);
			PFS_set_path_entries_free(aux_path);
			free(aux_path);
		}
		free(file_names);
	}

	// Leo el cluster de la carpeta del archivo/carpeta
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	AddressPFS_read_cluster(cluster, n_cluster_content);

	// Calculo en que byte de que sector de que bloque está la entrada long del archivo
	int n_block, n_sector, offset_entry = long_n_entry;
	PFS_n_block_n_sector_of_cluster_by_bytes(&offset_entry, &n_block, &n_sector);

	long_dir_entry->sequence_number = FREE_USED_ENTRY;
	dir_entry->file_attributes = FREE_ENTRY;
	// Modifico el cluster, marcando las entradas como libres usadas y lo grabo en Data Region
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[offset_entry * ENTRY_SIZE]), long_dir_entry, ENTRY_SIZE);
	memcpy(&(cluster->block[n_block].sector[n_sector].byte[(offset_entry + 1) * ENTRY_SIZE]), dir_entry, ENTRY_SIZE);

	AddressPFS_write_cluster(n_cluster_content, cluster);

	// Si el contenido del archivo/carpeta no tiene un cluster asignado, no modifico la FAT
	if (PFS_n_first_file_cluster(dir_entry) == -2) {
		log_info(project_log, "[PFS_set_path_entries_free]", "%s%s", "El archivo no tiene contenido, no está en la FAT ", path);
		free(cluster), free(long_dir_entry), free(dir_entry);
		return 0;
	}

	// Grabo las entradas del archivo en la FAT como libres usadas
	int n_file_clusters, *file_clusters = (int*) malloc(sizeof(int));
	file_clusters[0] = PFS_n_first_file_cluster(dir_entry);
	FAT_get_file_clusters(&file_clusters, &n_file_clusters);
	Aux_printf("[PFS_set_path_entries_free] Cantidad de clusters del archivo: %d\n", n_file_clusters);
	FAT_write_FAT_free_entry(n_file_clusters, file_clusters);

	free(long_dir_entry), free(dir_entry), free(file_clusters), free(cluster);
	log_info(project_log, "[PFS_set_path_entries_free]", "%s %s", "Se removio el archivo/carpeta", path);

	return 0;
}

// Recibe el cluster donde buscara las estradas que tengan nombre file_name
// Si lo encontro, devuelve las entradas cargadas y el numero de entrada de la long (return 0)
// Si no encontro el archivo y encontro una vacia, devuelve las entradas vacias y el numero de entrada de la 1º long vacia (return -2)
// Si no encontro el archivo y ninguna vacia, devuelve entradas vacias y el numero de entrada -1 (return -1)
// Si es una carpeta nueva, sin entradas . y .. return -3;
int PFS_get_file_entries(Cluster* cluster, const char* file_name, LongDirEntry* long_dir_entry, DirEntry* dir_entry, int* n_entry) {
	int attr = ATTR_LONG_NAME, attr_ant, aux_attr;
	(*n_entry) = 0;
	char* aux_file_name = (char*) malloc(sizeof(char));
	// Busco entradas y las voy contando hasta que encuentre la long o hasta que haya una vacia
	do {
		aux_attr = ATTR_LONG_NAME;
		free(aux_file_name);
		attr_ant = attr;
		attr = PFS_load_entry(cluster, (*n_entry)++, long_dir_entry, dir_entry);
		//PFSPrint_long_dir_entry(long_dir_entry);
		//printf("ATTR_ANT: %d, ATTR: %d-----\n", attr_ant, attr);
		if ((attr_ant == attr) && (attr == -ATTR_LONG_NAME)) {
			//printf("Entrada short!\n");
			aux_attr = -ATTR_LONG_NAME;
		}
		if (attr == -ATTR_LONG_NAME) {
			aux_attr = -ATTR_LONG_NAME;
		}
		aux_file_name = PFS_get_file_name(aux_attr, long_dir_entry, dir_entry);
		//printf("aux_file_name: %s, file_name: %s\n", aux_file_name, file_name);
	} while (strcmp(aux_file_name, file_name) && (long_dir_entry->sequence_number != FREE_ENTRY) && (long_dir_entry->sequence_number != FREE_USED_ENTRY)
	//(dir_entry->file_attributes != FREE_ENTRY) && (dir_entry->dos_file_name[0] != FREE_USED_ENTRY)
	);
	/*
	 printf("########\n");
	 if (aux_attr == -ATTR_LONG_NAME)
	 PFSPrint_short_dir_entry(dir_entry);
	 */
	// Si encontré la long del archivo, leo la short y devuelvo la posicion de la long
	//printf("Sec n: %d\n",long_dir_entry->sequence_number);
	if (!strcmp(aux_file_name, file_name) && (((long_dir_entry->sequence_number != FREE_ENTRY) && (long_dir_entry->sequence_number != FREE_USED_ENTRY))
	//|| ((dir_entry->file_attributes != FREE_ENTRY) && (dir_entry->dos_file_extension[0] != FREE_USED_ENTRY))
			)) {
		//printf("Nombre igual, sec_numb != free y used\n");
		if (aux_attr != -ATTR_LONG_NAME) {
			PFS_load_entry(cluster, *n_entry, long_dir_entry, dir_entry);
		}
		--(*n_entry);
		free(aux_file_name);
		return 0;
	}
	free(aux_file_name);
	// Si encontre una vacia, devuelvo la pos de la vacia
	if ((long_dir_entry->sequence_number == FREE_ENTRY) || (long_dir_entry->sequence_number == FREE_USED_ENTRY)) {
		--(*n_entry);
		// Si no tiene ni entradas . y .. aviso
		return ((*n_entry) < 2) ? -3 : -2;
	}
	// Si no encontre ni la long del archivo ni una vacia
	(*n_entry) = -1;
	return -1;
}

// Devuelve cargada una entrada short o long segun el caso y retorna que entrada cargo
int32_t PFS_load_entry(Cluster* cluster, int n_entry, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	int n_block = 0, n_sector = 0, entry_byte_offset = (n_entry * ENTRY_SIZE) + ATTR_OFFSET;

	PFS_n_block_n_sector_of_cluster_by_bytes(&entry_byte_offset, &n_block, &n_sector);

	//Aux_printf("[LOAD ENTRY] n_entry: %d, n_block: %d, n_sector: %d, entry_byte_offset: %d\n", n_entry, n_block, n_sector, entry_byte_offset);
	if (n_block > config_pfs.blocks_per_cluster) {
		// Buscar en el proximo cluster que indique la FAT
		Aux_printf("[PFS_load_entry] n_block del cluster es mayor que 4\n");
	}
	//}

	if (cluster->block[n_block].sector[n_sector].byte[entry_byte_offset] == FREE_ENTRY) {
		PFS_set_entry(cluster, n_entry, long_dir_entry);
		PFS_set_entry(cluster, n_entry, dir_entry);
		return ATTR_LONG_NAME;
	}
	if (cluster->block[n_block].sector[n_sector].byte[entry_byte_offset] == ATTR_LONG_NAME) {
		PFS_set_entry(cluster, n_entry, long_dir_entry);
		return ATTR_LONG_NAME;
	}
	PFS_set_entry(cluster, n_entry, dir_entry);
	return -ATTR_LONG_NAME;
}

void PFS_set_entry(Cluster* cluster, int n_entry, void* struct_entry) {
	char entry[ENTRY_SIZE];
	int i, n_block = 0, n_sector = 0, entry_byte_offset = n_entry * ENTRY_SIZE;
	PFS_n_block_n_sector_of_cluster_by_bytes(&entry_byte_offset, &n_block, &n_sector);
	for (i = 0; i < ENTRY_SIZE; i++)
		entry[i] = cluster->block[n_block].sector[n_sector].byte[entry_byte_offset++];
	memcpy(struct_entry, entry, ENTRY_SIZE);
}

void PFS_init_entries(int attr, const unsigned char* file_name, int first_file_cluster, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	int actual_date = 0, actual_time = 0;
	dir_entry->file_attributes = attr;
	// Las fechas no son necesarias para el TP
	dir_entry->create_date = dir_entry->last_acces_date = dir_entry->last_modified_date = actual_date;
	dir_entry->create_time = dir_entry->create_time2 = dir_entry->last_modified_time = actual_time;
	dir_entry->file_size = 0;
	dir_entry->reserved = 0;
	long_dir_entry->attributes = ATTR_LONG_NAME;
	long_dir_entry->reserved = 0;
	long_dir_entry->sequence_number = LAST_LONG_ENTRY;
	PFS_set_first_n_cluster_entry(first_file_cluster + bs->fat32_first_cluster, long_dir_entry, dir_entry);
	PFS_set_file_name(file_name, long_dir_entry, dir_entry);

}

unsigned char PFS_lfn_checksum(const unsigned char *pFcbName) {
	int i;
	unsigned char sum = 0;

	for (i = 11; i; i--)
		sum = ((sum & 1) << 7) + (sum >> 1) + *pFcbName++;
	return sum;
}

// Recibe el path de una carpeta y devuelve un vector con los nombres de sus archivos y su tamaño

int PFS_directory_list(const char* dir_path, char*** file_names, int* n_file_names) {
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int n_entry, aux_content, attr, n_cluster_content = 0;

	// Si no es el directorio "/" busco la carpeta que contiene a la carpeta dir_path
	if (strcmp(dir_path, "/")) {
		// Busco la carpeta
		if (PFS_get_path_entries(dir_path, long_dir_entry, dir_entry, &n_entry, &aux_content) == -1) {
			free(long_dir_entry), free(dir_entry);
			log_warning(project_log, "[PFS_directory_list]", "%s%s", "No existe el directorio ", dir_path);
			return -1;
		}
		// Obtengo el primer cluster del contenido de la carpeta, donde estan sus entradas
		n_cluster_content = PFS_n_first_file_cluster(dir_entry);
	}

	// Leo el bloque donde estan las entradas de la carpeta
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	AddressPFS_read_cluster(cluster, n_cluster_content);

	// Voy cargando el vector de nombres, con los nombres de las entradas que tenga la carpeta
	n_entry = 0;
	(*n_file_names) = 0;

	// Limpio las entradas para reutilizarlas
	//PFS_clean_entries(long_dir_entry, dir_entry);
	//free(long_dir_entry), free(dir_entry);
	//dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	// long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));

	dir_entry->dos_file_name[0] = FREE_USED_ENTRY;
	dir_entry->file_attributes = FREE_ENTRY;

	char* file_name;
	int entries_count;
	int attr_aux;
	do {
		// Cargo una entrada long
		attr = PFS_load_entry(cluster, n_entry, long_dir_entry, dir_entry);
		//printf("ATTR: %d -------------\n", attr);

		//if (dir_entry->dos_file_name[0] != 0) {
		//PFS_clean_entries(long_dir_entry, dir_entry);
		//}

		//Voy pisando la entrada long hasta que se cargue una short, me quedan las ultimas long y short necesarias
		entries_count = 0;
		do {
			attr_aux = ATTR_LONG_NAME;
			entries_count++;
			attr = PFS_load_entry(cluster, ++n_entry, long_dir_entry, dir_entry);
			//printf("ATTR: %d, entries_count: %d, n_entry: %d#\n", attr, entries_count, n_entry);
			if ((n_entry > 2) && (entries_count == 1) && (attr == -ATTR_LONG_NAME)) {
				//printf("Entrada short!\n");
				attr_aux = -ATTR_LONG_NAME;
			}
			if (strcmp(dir_path, "/") && (entries_count == 1) && (attr == -ATTR_LONG_NAME)) {
				//printf("Entrada short 2!\n");
				attr_aux = -ATTR_LONG_NAME;
			}

			//if (long_dir_entry->sequence_number == FREE_ENTRY && n_entry == 1) {
			//Aux_printf("No hay entradas!! \n");
			//break;
			// }

		} while ((attr == ATTR_LONG_NAME) && (dir_entry->file_attributes != FREE_ENTRY));

		// Si es una entrada de directorio, archivo y no libre, cargo el nombre en el vector
		if (PFS_is_visible_file_entry(long_dir_entry, dir_entry)) {
			//printf("visible\n");
			file_name = PFS_get_file_name(attr_aux, long_dir_entry, dir_entry);
			if (strcmp(file_name, "..      .   ")) {
				if ((*n_file_names) > 0) {
					*file_names = realloc(*file_names, sizeof(char*) * ((*n_file_names) + 1));
				}
				(*file_names)[(*n_file_names)++] = file_name;
			}
		}
		PFS_clean_entries(long_dir_entry, dir_entry);
	} while ((dir_entry->file_attributes != FREE_ENTRY) && (n_entry < PFS_max_cluster_entries()));
	free(cluster);

	// Si se buscaron en todas las entradas y todavia no termino
	if (dir_entry->file_attributes != FREE_ENTRY) {
		// Verificar en la FAT si hay otro cluster para seguir buscando
		// Sino, son las ultimas 2 entradas de la carpeta, no hacer nada
	}

	free(long_dir_entry), free(dir_entry);
	log_info(project_log, "[PFS_directory_list]", "%s%d%s%s", "Se listaron ", *n_file_names, " archivos en el directorio ", dir_path);
	return 0;
}
/*

 int PFS_directory_list(const char* dir_path, char*** file_names, int* n_file_names) {
 DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
 LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
 int n_entry, aux_content, attr, n_cluster_content = 0;

 // Si no es el directorio "/" busco la carpeta que contiene a la carpeta dir_path
 if (strcmp(dir_path, "/")) {
 // Busco la carpeta
 if (PFS_get_path_entries(dir_path, long_dir_entry, dir_entry, &n_entry, &aux_content) == -1) {
 free(long_dir_entry), free(dir_entry);
 log_warning(project_log, "[PFS_directory_list]", "%s%s", "No existe el directorio ", dir_path);
 return -1;
 }
 // Obtengo el primer cluster del contenido de la carpeta, donde estan sus entradas
 n_cluster_content = PFS_n_first_file_cluster(dir_entry);
 }

 // Leo el bloque donde estan las entradas de la carpeta
 Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
 AddressPFS_read_cluster(cluster, n_cluster_content);

 // Voy cargando el vector de nombres, con los nombres de las entradas que tenga la carpeta
 n_entry = 0;
 (*n_file_names) = 0;

 // Limpio las entradas para reutilizarlas
 PFS_clean_entries(long_dir_entry, dir_entry);

 char* file_name;

 do {
 // Cargo una entrada long
 PFS_load_entry(cluster, n_entry, long_dir_entry, dir_entry);
 if (dir_entry->dos_file_name[0] != 0) {
 PFS_clean_entries(long_dir_entry, dir_entry);
 }
 //Voy pisando la entrada long hasta que se cargue una short, me quedan las ultimas long y short necesarias
 do {
 attr = PFS_load_entry(cluster, ++n_entry, long_dir_entry, dir_entry);
 if (long_dir_entry->sequence_number == FREE_ENTRY && n_entry == 1) {
 Aux_printf("No hay entradas!! \n");
 break;
 }
 } while ((attr == ATTR_LONG_NAME) && (dir_entry->file_attributes != FREE_ENTRY));
 // Si es una entrada de directorio, archivo y no libre, cargo el nombre en el vector
 if (PFS_is_visible_file_entry(long_dir_entry, dir_entry)) {
 file_name = PFS_get_file_name(ATTR_LONG_NAME, long_dir_entry, dir_entry);
 if ((*n_file_names) > 0) {
 *file_names = realloc(*file_names, sizeof(char*) * ((*n_file_names) + 1));
 }
 (*file_names)[(*n_file_names)++] = file_name;
 }
 PFS_clean_entries(long_dir_entry, dir_entry);
 } while ((dir_entry->file_attributes != FREE_ENTRY) && (n_entry < PFS_max_cluster_entries()));
 free(cluster);

 // Si se buscaron en todas las entradas y todavia no termino
 if (dir_entry->file_attributes != FREE_ENTRY) {
 // Verificar en la FAT si hay otro cluster para seguir buscando
 // Sino, son las ultimas 2 entradas de la carpeta, no hacer nada
 }

 free(long_dir_entry), free(dir_entry);
 log_info(project_log, "[PFS_directory_list]", "%s%d%s%s", "Se listaron ", *n_file_names, " archivos en el directorio ", dir_path);
 return 0;
 }
 */
int PFS_is_visible_file_entry(LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	return ((dir_entry->file_attributes == ATTR_DIRECTORY) || (dir_entry->file_attributes == ATTR_ARCHIVE)) && (dir_entry->file_attributes != FREE_ENTRY);
}

int PFS_read_file_content(const char* path, off_t offset, char* buf, size_t size) {
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int i, n_entry, n_cluster_content, first_file_cluster;
	// Busco el archivo y me devuelve su info basica
	if (PFS_get_path_entries(path, long_dir_entry, dir_entry, &n_entry, &n_cluster_content) == -1) {
		free(long_dir_entry), free(dir_entry);
		log_warning(project_log, "[PFS_read_file_content]", "%s%s", "No existe el archivo ", path);
		return -1;
	}
	//PFSPrint_long_dir_entry(long_dir_entry);
	//PFSPrint_short_dir_entry(dir_entry);

	Aux_printf("[PFS_read_file_content] Obtuve las entradas del archivo\n");

	// Cuantos clusters ocupa el archivo
	int f_size_clusters = PFS_n_clusters_by_bytes(size/*dir_entry->file_size*/);

	// Primer cluster del contenido del archivo
	first_file_cluster = PFS_n_first_file_cluster(dir_entry);

	free(long_dir_entry), free(dir_entry);

	// Si no tiene asignado un cluster
	if (first_file_cluster == -2) {
		log_warning(project_log, "[PFS_read_file_content]", "%s%s", "No tiene contenido el archivo ", path);
		return 0;
	}

	Aux_printf("[PFS_read_file_content] size: %d, clusters a leer: %d, primer cluster: %d\n", size, f_size_clusters, first_file_cluster);

	// Calculo cuantos clusters ocupa el offset
	int cluster_offset = 0;
	if (offset > 0) {
		// Cantidad de clusters que me corro desde el primero
		cluster_offset = PFS_n_clusters_by_bytes(offset) - 1;
		if (offset % sizeof(Cluster) == 0) {
			cluster_offset++;
		}
	}

	// Primer cluster a leer donde se encuentra el primer byte desde el cual leer (offset)
	int *f_clusters = (int*) malloc(sizeof(int));
	Aux_printf("[PFS_read_file_content] cluster_offset: %d, offset: %d\n", cluster_offset, offset);
	f_clusters[0] = FAT_x_n_cluster(first_file_cluster, cluster_offset);

	// Cargamos todos los clusters que sean necesarios para leer el contenido del archivo
	int n_file_clusters;
	Aux_printf("[PFS_read_file_content] Voy a pedir los clusters del archivo\n");
	FAT_get_file_clusters(&f_clusters, &n_file_clusters);

	// Primer cluster donde está el byte que tengo que empezar a leer (offset)
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
//	AddressPFS_read_cluster(cluster, f_clusters[0]);
	char* file_name = PFS_path_name(path);
	Aux_printf("[PFS_read_file_content] Voy a leer el cluster %d a traves de la cache\n", f_clusters[0]);
	Cache_read_cluster(cluster, f_clusters[0], file_name);
	// Cargar 'buf' a partir del 'offset'

	// Calcula en que bloques y sectores del cluster está el offset
	int n_block = 0, n_sector = 0, aux_offset = offset;
	PFS_n_block_n_sector_of_cluster_by_bytes(&aux_offset, &n_block, &n_sector);
	Aux_printf("[PFS_read_file_content] bloque %d, sector %d y byte %d del cluster a escribir\n", n_block, n_sector, aux_offset);
	long int bytes_to_copy = sizeof(Cluster) - (n_block * config_pfs.sectors_per_block * bs->bytes_per_sector + n_sector * bs->bytes_per_sector + aux_offset);
	if (bytes_to_copy > size) {
		Aux_printf("[PFS_read_file_content] Bytes a copiar > size\n");
		bytes_to_copy = size;
	}
	Aux_printf("[PFS_read_file_content] Voy a copiar al buffer: %d bytes, n_cluster: %d, desde su byte: %d sec: %d block: %d\n", bytes_to_copy, f_clusters[0], aux_offset, n_sector, n_block);
	memcpy(buf, &(cluster->block[n_block].sector[n_sector].byte[aux_offset]), bytes_to_copy);

	long int first_bytes_to_copy = bytes_to_copy;
	long int loaded_size = first_bytes_to_copy;

	free(cluster);
	// Leo cada clusters del vector, y voy cargando el buf con el contenido del archivo
	// cambiar f_size_clusters por n_file_clusters. /*Ver el -1*/
	for (i = 1; i < f_size_clusters; i++) {
		//AddressPFS_read_cluster(cluster, f_clusters[i]);
		cluster = (Cluster*) malloc(sizeof(Cluster));
		Aux_printf("[PFS_read_file_content] Voy a leer el cluster %d a traves de la cache, i: %d\n", f_clusters[i], i);
		Cache_read_cluster(cluster, f_clusters[i], file_name);
		// Cargar 'buf' con clusters completos
		bytes_to_copy = sizeof(Cluster);

		//Aux_printf("[PFS_read_file_content] Voy a copiar al buffer: %d bytes, n_cluster: %d, desde: %d\n", bytes_to_copy, f_clusters[i], first_bytes_to_copy/*+aux_offset*/+ ((i - 1) * bytes_to_copy));
		//Aux_printf("[PFS_read_file_content] LOADED SIZE: %ld, SIZE: %lu, BYTES_TO_COPY: %ld, N_cluster: %d\n", loaded_size, size, bytes_to_copy, f_clusters[i]);
		memcpy(&(buf[loaded_size]), cluster, bytes_to_copy);
		loaded_size += bytes_to_copy;
		free(cluster);
	}

	//Aux_printf("[PFS_read_file_content] Ultimo caso, size - loaded_size: %d\n", size - loaded_size);
	if (abs(size - loaded_size) > 0) {
		cluster = (Cluster*) malloc(sizeof(Cluster));
		Aux_printf("[PFS_read_file_content] Voy a leer el cluster %d a traves de la cache (ultimo caso) i:%d\n", f_clusters[i], i);
		Cache_read_cluster(cluster, f_clusters[i], file_name);
		//Aux_printf("[PFS_read_file_content] LOADED SIZE: %ld, SIZE: %lu, BYTES_TO_COPY: %ld, N_cluster: %d\n", loaded_size, size, bytes_to_copy, f_clusters[i]);
		memcpy(&(buf[loaded_size]), cluster, abs(size - loaded_size));
		loaded_size += size - loaded_size;
		free(cluster);
	}

	//Aux_printf("[PFS_read_file_content] LOADED SIZE: %d, SIZE: %d\n", loaded_size, size);

	free(f_clusters);
	//free(cluster);
	free(file_name);

	//sleep(3);
	Aux_printf("[PFS_read_file_content] El bufer se cargo con el contenido del archivo:\n");
	int j;
	for (j = 0; j < size; j++) {
		Aux_printf("%x", buf[j]);
	}
	Aux_printf("\n");

	log_info(project_log, "[PFS_read_file_content]", "%s%s", "Se leyó el archivo ", path);

	return 0;
}

// Retorna el nombre de una entrada long o short segun el parametro attr
char* PFS_get_file_name(int attr, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	if (attr == ATTR_LONG_NAME
	)
		return PFS_get_long_file_name(long_dir_entry);

	return (char*) PFS_get_short_file_name(dir_entry);
}

// Retorna armado del nombre de una entrada long
char* PFS_get_long_file_name(LongDirEntry* long_dir_entry) {
	int size_name = sizeof(long_dir_entry->name1) / 2 + sizeof(long_dir_entry->name2) / 2 + sizeof(long_dir_entry->name3) / 2 + 1;
	char* name = (char*) malloc(size_name * sizeof(char));
	int i, k;

	for (k = 0, i = 0; i < sizeof(long_dir_entry->name1) / 2; i++, k++)
		name[i] = long_dir_entry->name1[k];
	for (k = 0; k < sizeof(long_dir_entry->name2) / 2; i++, k++)
		name[i] = long_dir_entry->name2[k];
	for (k = 0; k < sizeof(long_dir_entry->name3) / 2; i++, k++)
		name[i] = long_dir_entry->name3[k];
	name[i] = '\0';

	return name;
}

// Retorna armado del nombre de una entrada short
unsigned char* PFS_get_short_file_name(DirEntry* dir_entry) {
	int size_name = strlen((char*) dir_entry->dos_file_name) + strlen((char*) dir_entry->dos_file_extension) + 1;
	unsigned char* name = (unsigned char*) malloc(sizeof(char) * size_name);
	int i, k;

	for (k = 0, i = 0; i < sizeof(dir_entry->dos_file_name); i++, k++)
		name[i] = dir_entry->dos_file_name[k];
	name[i++] = '.';
	for (k = 0; k < sizeof(dir_entry->dos_file_extension); i++, k++)
		name[i] = dir_entry->dos_file_extension[k];
	name[i] = '\0';

	return name;
}

void PFS_set_first_n_cluster_entry(int first_n_cluster_entry, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	dir_entry->fst_cluster_high = floor(first_n_cluster_entry / 256);
	dir_entry->fst_cluster_low = first_n_cluster_entry - dir_entry->fst_cluster_high;
}

int32_t PFS_n_first_file_cluster(DirEntry* dir_entry) {
	return (dir_entry->fst_cluster_high /** 256*/+ dir_entry->fst_cluster_low) - bs->fat32_first_cluster;
}

void PFS_set_file_name(const unsigned char* file_name, LongDirEntry* long_dir_entry, DirEntry*dir_entry) {
	// Definir
	int size_lfname_utf16 = sizeof(long_dir_entry->name1) + sizeof(long_dir_entry->name2) + sizeof(long_dir_entry->name3);
	char* lfname_utf16 = (char*) malloc(size_lfname_utf16);
	int i, j;
	for (i = 0, j = 0; i < size_lfname_utf16; i++) {
		if (j < strlen((const char*) file_name))
			lfname_utf16[i++] = file_name[j++];
		else
			lfname_utf16[i++] = '\0';
		lfname_utf16[i] = '\0';
	}

	memcpy(long_dir_entry->name1, lfname_utf16, sizeof(long_dir_entry->name1));
	memcpy(long_dir_entry->name2, lfname_utf16 + sizeof(long_dir_entry->name1), sizeof(long_dir_entry->name2));
	memcpy(long_dir_entry->name3, lfname_utf16 + sizeof(long_dir_entry->name1) + sizeof(long_dir_entry->name2), sizeof(long_dir_entry->name3));
	free(lfname_utf16);

	unsigned char* sfname_dos = (unsigned char*) malloc(8);
	char* sfext_dos = (char*) malloc(3);

	// Para archivos
	if (dir_entry->file_attributes == ATTR_ARCHIVE) {
		i = 0;
		do {
			sfname_dos[i] = toupper(file_name[i]);
			i++;
		} while (file_name[i] != '.' && i < 8);
		if (file_name[j = i++] == '.')
			while (j < 8)
				sfname_dos[j++] = '\40';
		else
			while (file_name[i++] != '.')
				;
		j = 0;
		do
			sfext_dos[j++] = toupper(file_name[i++]);
		while (j < 3 && i < strlen((const char*) file_name));
		while (j < 3)
			sfext_dos[j++] = '\40';
	}

	// Para carpetas
	if (dir_entry->file_attributes == ATTR_DIRECTORY) {
		for (i = 0; i < 8; i++) {
			if (i < strlen((const char*) file_name))
				sfname_dos[i] = toupper(file_name[i]);
			else
				sfname_dos[i] = '\40';
		}

		for (i = 0; i < 3; i++) {
			sfext_dos[i] = '\40';
		}
	}

	memcpy(dir_entry->dos_file_name, sfname_dos, 8);
	memcpy(dir_entry->dos_file_extension, sfext_dos, 3);

	unsigned char* short_file_name = PFS_get_short_file_name(dir_entry);
	// checksum: No es necesario para el TP, pero verificar si funca con 0
	// sino realizar cuenta http://en.wikipedia.org/wiki/Fat32#Long_file_names_2
	long_dir_entry->checksum = PFS_lfn_checksum((const unsigned char*) short_file_name);

	free(short_file_name);
	free(sfname_dos);
	free(sfext_dos);
}

// Recibe un path y retorna solo el nombre del archivo/carpeta
char* PFS_path_name(const char* path) {
	char** dir_names = (char**) malloc(sizeof(char*));
	int n_names;
	Aux_get_tokens_by("/", path, &dir_names, &n_names);
	char* path_name = (char*) malloc(strlen(dir_names[n_names - 1]) + 1);
	strcpy(path_name, dir_names[n_names - 1]);
	Aux_string_vec_destroy(&dir_names, n_names);
	free(dir_names);
	return path_name;
}

int32_t PFS_max_cluster_entries() {
	return (bs->bytes_per_sector * bs->sectors_per_cluster) / ENTRY_SIZE;
}

int32_t PFS_max_block_entries() {
	return (bs->bytes_per_sector * config_pfs.sectors_per_block) / ENTRY_SIZE;
}

int32_t PFS_max_sector_entries() {
	return bs->bytes_per_sector / ENTRY_SIZE;
}

// Recibe el n_sector que desea y retorna el n_block que representa, devolviendo por parametro si es el sector 0 o 1 del bloque
int PFS_get_n_block_by_sector(int n_sector, int* block_sector) {
	if ((n_sector % 2) == 0)
		(*block_sector) = 0;
	else
		(*block_sector) = 1;
	return PFS_n_block_by_sector(n_sector);
}

void PFS_n_block_n_sector_of_cluster_by_bytes(int* offset, int* n_block, int* n_sector) {
	(*n_sector) = 0, (*n_block) = 0;
	while (*offset >= bs->bytes_per_sector) {
		*offset -= bs->bytes_per_sector;
		(*n_sector)++;
		if ((*n_sector) >= config_pfs.sectors_per_block) {
			(*n_sector) = 0;
			(*n_block)++;
			if ((*n_block) >= config_pfs.blocks_per_cluster) {
				//printf("[PFS_n_block_n_sector_of_cluster_by_bytes] Reseteo en ultimo if con offset: %d, siendo n_sector: %d, n_block: %d\n", *offset, *n_sector, *n_block);
				//*offset -= bs->bytes_per_sector;
				(*n_block) = 0;
			}
		}
	}
}

int PFS_n_block_by_sector(int n_sector) {
	return floor(n_sector / config_pfs.sectors_per_block);
}

// (22-12) Modificado de int a long int el parametro
uint32_t PFS_n_clusters_by_bytes(uint32_t bytes) {
	return ceil((double) bytes / (double) (bs->bytes_per_sector * bs->sectors_per_cluster));
}

int32_t PFS_inicializar_config(void) {
	FILE* fi;
	char var[255];
	if ((fi = fopen("ConfigPFS.txt", "rb")) == NULL) {
		log_error(project_log, "File System", "Message error: %s", "No se puede cargar la Config");
		exit(-EIO);
	}
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.bytes_per_sector));
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.blocks_per_cluster));
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.sectors_per_block));
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.cache_size));
	fscanf(fi, "%s = %s\n", var, config_pfs.socket_config.ip);
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.socket_config.server_port));
	fscanf(fi, "%s = %hu\n", var, &(config_pfs.socket_config.max_connections));

	fclose(fi);

	log_info(project_log, "[PFS_inicializar_config]", "%s", "Se cargó la Config");

	return 0;
}

// Cierra la conexion del server y del cliente, libera el log y sale del main
void PFS_exit() {

	int i;
	if (sem_cont_connections->__align > 0) {
		for (i = 0; i < config_pfs.socket_config.max_connections; i++) {
			Addressing_disk_operation(connections_set, sem_cont_connections, TYPE_END_CONECTION);
			close(connections_set[i].client_fd);
		}
	}
}

// Prueba para mostrar entradas de los archivos
void PFS_file_list() {
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	int n_entry, attr;
	ln();
	printf("> File list\n");
	AddressPFS_read_cluster(cluster, CLUS);

	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	//for (n_entry = 0; n_entry < PFS_max_cluster_entries(); n_entry++) {
	n_entry = 0;
	do {
		attr = PFS_load_entry(cluster, n_entry++, long_dir_entry, dir_entry);
		printf("> %s", PFS_get_file_name(attr, long_dir_entry, dir_entry));
		PFSPrint_dir_entry(attr, long_dir_entry, dir_entry);
		ln();
	} while (long_dir_entry->sequence_number != FREE_ENTRY);
	free(cluster);
	PFS_destroy_entries(long_dir_entry, dir_entry);
}

void PFS_destroy_entries(LongDirEntry*long_dir_entry, DirEntry*dir_entry) {
	free(dir_entry);
	free(long_dir_entry);
}

// Recibe el n_block a cargar, y devuelve el bloque cargado con sus entradas de la FAT
void PFS_set_block_by_FAT_entry(int n_block, Block* block) {
	memcpy(block, FAT_table->entry + PFS_first_FAT_n_entry_in_block(n_block), PFS_entries_per_block());
}

int PFS_first_FAT_n_entry_in_block(int n_block) {
	return (n_block - PFS_first_FAT_block()) * PFS_entries_per_block();
}
/*
 int PFS_n_blocks_by_FAT_entries(int FAT_entries) {
 return ceil((double) (FAT_entries * FAT_ENTRY_SIZE) / (double) (bs->bytes_per_sector * config_pfs.sectors_per_block));
 }*/

int PFS_n_block_by_FAT_entry(int n_entry) {
	return floor((n_entry + (PFS_first_FAT_block() * PFS_entries_per_block())) / PFS_entries_per_block());
}

int PFS_entries_per_block() {
	return (config_pfs.sectors_per_block * bs->bytes_per_sector) / FAT_ENTRY_SIZE;
}

int PFS_first_FAT_block() {
	return (bs->reserved_sector_count / config_pfs.sectors_per_block);
}

int PFS_n_block_by_n_cluster(int n_cluster) {
	int first_DataRegion_sector = bs->reserved_sector_count + bs->number_of_fats * bs->fat32_sectors_per_fat;
	int n_sector = first_DataRegion_sector + n_cluster * bs->sectors_per_cluster;
	int n_block = PFS_n_block_by_sector(n_sector);
	return n_block;
}
