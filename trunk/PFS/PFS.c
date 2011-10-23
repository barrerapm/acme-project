#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "PFS.h"
#include "PPD.h"
#include "FAT.h"
//#include "FUSE.h"
#include "utils.h"
#define ROOT_ENTRY_BYTES 32
#define MAX_FILE_NAME_LENGHT 28
#define ROOT_CLUSTER 0
#define LAST_FILE_CLUSTER 0xFFFFFFF
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME 0x0F
#define ATTR_OFFSET 11

ConfigPFS config_pfs; // Estructura global config_ppd
BootSector* bs;
FSInfo* fs_info;
TablaFAT* FAT_table;
int CLUS = 3; // Para pruebas de file list y proximamente rename

int main(int argc, char** argv) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	Block* block = (Block*) malloc(sizeof(Block));
	bs = (BootSector*) malloc(sizeof(BootSector));
	fs_info = (FSInfo*) malloc(sizeof(FSInfo));
	FAT_table = (TablaFAT*) malloc(sizeof(TablaFAT));
	// Abrir disco
	if (Adressing_open_disk() != 0) {
		printf("No se puede abrir el Disco\n"); // Log
		return -1;
	}
	printf("> Disk open\n");

	// Config PFS
	PFS_inicializar_config();
	PFSPrint_config_pfs(); // Prueba

	// Boot Sector
	Disk_load_bootsector(bs);
	PFSPrint_bootsector(); // Prueba

	// FS Info
	Disk_load_FSInfo(fs_info);
	PFSPrint_fs_info(); // Prueba

	// FAT Region
	Disk_load_FAT(FAT_table);
	PFSPrint_FAT_table(); // Prueba

	// Prueba Block
	//Adressing_read_block(block,0);
	//PFSPrint_block(*block,0); // Prueba

	// Root Directory
	PFS_file_list();

	// Data Region
	printf("\n> Leer contenido de un archivo\n");
	PFS_read_file_content("/CarpetaA/CarpetaB/ArchC.txt");

	// Obtener clusters libres
	printf("\n> Obtener clusters libres\n");
	int i, *free_clusters, n_free_clusters = 5; //59458;
	free_clusters = (int*) malloc(sizeof(int));
	FAT_get_free_clusters(n_free_clusters, free_clusters);

	for (i = 0; i < n_free_clusters; i++)
		printf("Libre %d: %d\n", i, free_clusters[i]);

	// Escribir clusters libres
	printf("\n> Escribir clusters libres\n");
	FAT_write_FAT_entry(n_free_clusters, free_clusters);
	PFSPrint_FAT_table();

	free(free_clusters);

	// Listar directorio
	printf("\n> Listar directorio\n");
	char** file_names;
	int n_file_names = 0;
	file_names = (char**) malloc(sizeof(char*) * n_file_names);
	PFS_directory_list("/CarpetaA/", file_names, &n_file_names);
	//int k;
	for (i = 0; i < n_file_names; i++)
		printf("Name %d: %s\n", i, file_names[i]);
	//	for (k = 0; k<strlen(file_names[i]); k++)  // Un for más para mprimir por caracteres
	//		printf("Name %d: %c\n", i, file_names[i][k]);

	for (i = 0; i < n_file_names; i++)
		free(file_names[i]);
	free(file_names);

	// Cerrar disco
	Adressing_close_disk();

	//return fuse_main(argc, argv, &operaciones);
	return 0;
}

void PFS_file_list() { // Prueba para mostrar entradas de los archivos
	extern BootSector* bs;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int n_entry, attr;
	ln();
	printf("> File list\n");
	Adressing_read_cluster(cluster, CLUS - 2/*ROOT_CLUSTER*/); // Modificar cluster a mostrar
	for (n_entry = 0; n_entry < PFS_max_cluster_entries(); n_entry++) {
		attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, n_entry);
		printf("> %s", PFS_file_name(attr, long_dir_entry, dir_entry));
		PFSPrint_dir_entry(attr, long_dir_entry, dir_entry);
		ln();
	}
}

void PFS_rename_file(const char* path_name, const char* new_name) {
	extern BootSector* bs;
	extern TablaFAT* FAT_table;
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	if (PFS_search_file_entry(dir_entry, long_dir_entry, path_name,CLUS) == 0) { // Busco la carpeta "path" y me devuelve su info basica
		// Definir
	}
}

int32_t PFS_directory_list(const char* path, char** file_names, int* n_file_names) {
	extern BootSector* bs;
	extern TablaFAT* FAT_table;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int i, n_entry, attr, first_file_entry = 2, clus;
	if (path == "/")
		clus = bs->fat32_first_cluster;
	else
		if (PFS_search_path_entry(dir_entry, long_dir_entry, path) < 1){ // Busco la carpeta y me devuelve su info basica
			printf("No existe el directorio %s\n", path); // Log
			return -1;
		}
		else
			clus = PFS_n_first_file_cluster(dir_entry);

	Adressing_read_cluster(cluster, clus - 2); // Leo el cluster donde estan las entradas de la carpeta

	for (i = 0, n_entry = first_file_entry; n_entry < PFS_max_cluster_entries(); n_entry++) { // Voy cargando los file_names
		attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, n_entry); // Leo una entrada de un archivo dentro de "dir_name"
		attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, ++n_entry);
		while(attr == 0x0F)
			attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, ++n_entry);
		if ((dir_entry->file_attributes == ATTR_DIRECTORY || dir_entry->file_attributes == ATTR_ARCHIVE) && long_dir_entry->sequence_number != 0xe5) {
			file_names = realloc(file_names, (i + 1) * sizeof(char*));
			file_names[i] = (char*) malloc(sizeof(char) * strlen(PFS_file_name(attr, long_dir_entry, dir_entry)));
			file_names[i++] = PFS_file_name(ATTR_LONG_NAME, long_dir_entry, dir_entry);
		}
	}
	*n_file_names = i;
	return 0;
}

void PFS_read_file_content(const char* path) {
	extern BootSector* bs;
	extern TablaFAT* FAT_table;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));

	if (PFS_search_path_entry(dir_entry, long_dir_entry, path) == 0) { // Busco el archivo "path" y me devuelve su info basica
		int i, n_entry, f_size_clusters = PFS_total_file_clusters(dir_entry);
		int *f_clusters;
		f_clusters = (int*) malloc(sizeof(int));

		n_entry = PFS_n_first_file_cluster(dir_entry); // Primer cluster del archivo
		f_clusters[0] = n_entry - bs->fat32_first_cluster;
		printf("Primer entrada en la FAT: %d\n", n_entry); // Prueba
		printf("Primer cluster en la Data Region: %d\n", f_clusters[0]); // Prueba

		FAT_get_file_clusters(f_clusters);

		printf("Cantidad de clusters del archivo: %d", f_size_clusters); // Prueba

		for (i = 0; i < f_size_clusters; i++) {
			Adressing_read_cluster(cluster, f_clusters[i]);
			PFSPrint_cluster(*cluster, f_clusters[i]);
		}
	} else
		printf("No se encontró el archivo para PFS_read_file_content\n");
}

int32_t PFS_search_path_entry(DirEntry* dir_entry, LongDirEntry* long_dir_entry, const char* path) {
	extern BootSector* bs;
	char** all_dir_names = (char**) malloc(sizeof(char*));
	int size_dir_names;
	PFS_path_dir_names(path,all_dir_names,&size_dir_names);
	int k, i=0, dir_n_first_cluster = 2;

	while(i< size_dir_names){
		if(PFS_search_file_entry(dir_entry,long_dir_entry,all_dir_names[i],dir_n_first_cluster)!=0)
			return -1;
		dir_n_first_cluster = PFS_n_first_file_cluster(dir_entry);
		i++;
	}
/*
	for (i = 0; i < sizeof(all_dir_names); i++)
		free(all_dir_names[i]);
	free(all_dir_names);
*/
	if(dir_entry->file_attributes == ATTR_ARCHIVE)
		return 0;
	else if(dir_entry->file_attributes == ATTR_DIRECTORY)
		return 1;
}

int32_t PFS_search_file_entry(DirEntry* dir_entry, LongDirEntry* long_dir_entry, const char* path, int n_cluster) {
	extern BootSector* bs;
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	int n_entry = 2, attr;

	Adressing_read_cluster(cluster, n_cluster-2); // Modificar para que busque en el cluster necesario
	attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, n_entry);
	PFS_load_entry(cluster, long_dir_entry, dir_entry, ++n_entry);
	while (strcmp(PFS_file_name(attr, long_dir_entry, dir_entry), path) && n_entry < 14 /*PFS_max_cluster_entries()*/) {
		attr = PFS_load_entry(cluster, long_dir_entry, dir_entry, n_entry);
		PFS_load_entry(cluster, long_dir_entry, dir_entry, ++n_entry);
	}
	if (strcmp(PFS_file_name(attr, long_dir_entry, dir_entry), path))
		return -1;
	else
		return 0;
}

void PFS_path_dir_names(const char* pathx, char** all_dir_names, int* size_dir_names){
	char path[1024]; // Tamaño maximo de un path ?
	strcpy(path, pathx);
	char *dir_name;
	int i,k;
	for (i=0, dir_name = strtok(path, "/"); dir_name != NULL; dir_name = strtok(NULL, "/"), i++){
		all_dir_names = realloc(all_dir_names, sizeof(char**)*(i+1));
		all_dir_names[i] = (char*) malloc(sizeof(char)*strlen(dir_name));
		strcpy(all_dir_names[i] , dir_name);
	}
	*size_dir_names = i;
}

char* PFS_file_name(int attr, LongDirEntry* long_dir_entry, DirEntry* dir_entry) {
	if (attr == ATTR_LONG_NAME
		)
		return PFS_long_file_name(long_dir_entry);
	else
		return PFS_short_file_name(dir_entry);
}

char* PFS_long_file_name(LongDirEntry* long_dir_entry) {
	char name[MAX_FILE_NAME_LENGHT];
	int i, k;

	for (k = 0, i = 0; i < sizeof(long_dir_entry->name1) / 2; i++, k++)
		name[i] = long_dir_entry->name1[k];
	for (k = 0; k < sizeof(long_dir_entry->name2) / 2; i++, k++)
		name[i] = long_dir_entry->name2[k];
	for (k = 0; k < sizeof(long_dir_entry->name3) / 2; i++, k++)
		name[i] = long_dir_entry->name3[k];
	name[i] = '\0';

	char *pName = (char*) malloc(sizeof(name) * sizeof(char));
	strcpy(pName, name);
	return pName;
}

char* PFS_short_file_name(DirEntry* dir_entry) {
	char name[MAX_FILE_NAME_LENGHT];
	int i, k;

	for (k = 0, i = 0; i < sizeof(dir_entry->dos_file_name); i++, k++)
		name[i] = dir_entry->dos_file_name[k];
	name[i++] = '.';
	for (k = 0; k < sizeof(dir_entry->dos_file_extension); i++, k++)
		name[i] = dir_entry->dos_file_extension[k];
	name[i] = '\0';

	char *pName = (char*) malloc(sizeof(name) * sizeof(char));
	strcpy(pName, name);
	return pName;
}

int32_t PFS_load_entry(Cluster* cluster, LongDirEntry* long_dir_entry, DirEntry* dir_entry, int n_entry) {
	if (cluster->sector[0].byte[(n_entry * ROOT_ENTRY_BYTES) + ATTR_OFFSET] == ATTR_LONG_NAME) {
		PFS_load_long_entry(cluster, long_dir_entry, n_entry);
		return ATTR_LONG_NAME;
	}
	PFS_load_short_entry(cluster, dir_entry, n_entry);
	return -ATTR_LONG_NAME;
}

void PFS_load_long_entry(Cluster* cluster, LongDirEntry* long_dir_entry, int n_entry) {
	char entry[ROOT_ENTRY_BYTES];
	int i, j = n_entry * ROOT_ENTRY_BYTES;
	for (i = 0; i < ROOT_ENTRY_BYTES; i++, j++)
		entry[i] = cluster->sector[0].byte[j];
	memcpy(long_dir_entry, entry, sizeof(entry));
}

void PFS_load_short_entry(Cluster* cluster, DirEntry* dir_entry, int n_entry) {
	char entry[ROOT_ENTRY_BYTES];
	int i, j = n_entry * ROOT_ENTRY_BYTES;
	for (i = 0; i < ROOT_ENTRY_BYTES; i++, j++)
		entry[i] = cluster->sector[0].byte[j];
	memcpy(dir_entry, entry, sizeof(entry));
}

int32_t PFS_max_cluster_entries() {
	extern BootSector* bs;
	return (bs->bytes_per_sector * bs->sectors_per_cluster) / ROOT_ENTRY_BYTES;
}

int32_t PFS_n_first_file_cluster(DirEntry* dir_entry) {
	return (dir_entry->fst_cluster_high * 256 + dir_entry->fst_cluster_low);
}

int32_t PFS_total_file_clusters(DirEntry* dir_entry) {
	extern BootSector* bs;
	return ceil((double) dir_entry->file_size / (double) (bs->bytes_per_sector * bs->sectors_per_cluster));
}

void PFS_inicializar_config(void) {
	FILE* fi;
	char var[255];
	fi = fopen("ConfigPFS.txt", "rb");
	fscanf(fi, "%s = %d\n", var, &config_pfs.bytes_per_sector);
	fscanf(fi, "%s = %d\n", var, &config_pfs.sectors_per_cluster);
	fscanf(fi, "%s = %d\n", var, &config_pfs.clusters_per_block);
	fclose(fi);
}
