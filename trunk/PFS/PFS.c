#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "PFS.h"
#include "PPD.h"
#include "FAT.h"
#include "utils.h"
#define ROOT_ENTRY_BYTES 32
#define MAX_FILE_NAME_LENGHT 28
#define ROOT_CLUSTER 0
#define LAST_FILE_CLUSTER 0xFFFFFFF
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATRR_LONG_NAME 0x0F

ConfigPFS config_pfs; // Estructura global config_ppd

int CLUS = 4;

int main(void) {
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	Block* block = (Block*) malloc(sizeof(Block));
	BootSector* bs = (BootSector*) malloc(sizeof(BootSector));
	FSInfo* fs_info = (FSInfo*) malloc(sizeof(FSInfo));
	TablaFAT* FAT_table = (TablaFAT*) malloc(sizeof(TablaFAT));
	// Abrir disco
	if (PPD_open_disk() != 0) {
		printf("No se puede abrir el Disco\n"); // Log
		return -1;
	}
	printf("> Disk open\n");

	// Config PFS
	PFS_inicializar_config();
	PFSPrint_config_pfs(); // Prueba

	// Boot Sector
	Disk_load_bootsector(bs);
	PFSPrint_bootsector(bs); // Prueba

	// FS Info
	Disk_load_FSInfo(bs, fs_info);
	PFSPrint_fs_info(fs_info); // Prueba

	// FAT Region
	Disk_load_FAT(bs, FAT_table);
	PFSPrint_FAT_table(*FAT_table); // Prueba

	// Prueba Block
	//Adressing_read_block(bs,block,0);
	//PFSPrint_block(bs,*block,0); // Prueba

	// Root Directory
	PFS_file_list(bs);

	// Data Region
	//PFS_read_DataRegion(bs);
	PFS_read_file_content(bs, FAT_table, "ArchC.txt");
	int *free_clusters;
	free_clusters= (int*) malloc(sizeof(int));
	int n_free_clusters = 59458; // Arreglar, porque si pido 1 cluster mas explota todo
	FAT_get_free_clusters(bs, fs_info, FAT_table, n_free_clusters, free_clusters);
	int i;
	for(i = 0; i < n_free_clusters ; i++)
		printf("Libre %d: %d\n",i,free_clusters[i]); // Prueba, muestra vector de clusters libres

	// Cerrar disco
	PPD_close_disk();

	//return fuse_main(argc, argv, &operaciones);
	return 0;
}

void PFS_file_list(BootSector* bs) { // Prueba para mostrar entradas de los archivos
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int n_entry, total_entries = (bs->bytes_per_sector * bs->sectors_per_cluster) / ROOT_ENTRY_BYTES;
	ln();
	printf("> File list\n");
	Adressing_read_cluster(bs, cluster, CLUS/*ROOT_CLUSTER*/); // Modificar cluster a mostrar
	for (n_entry = 0; n_entry < total_entries; n_entry++) {
		PFS_load_entry(cluster, dir_entry, long_dir_entry, n_entry);
		if (dir_entry->file_attributes == 0x20 || long_dir_entry->attributes == 0x20 || dir_entry->file_attributes == 0x10 || long_dir_entry->attributes == 0x10) {
			printf("> %s", PFS_file_name(dir_entry, long_dir_entry));
			PFSPrint_long_dir_entry(long_dir_entry); // Prueba (info basica)
			PFSPrint_dir_entry(dir_entry); // Prueba (info basica)
			ln();
		}
	}
}

void PFS_directory_list(BootSector* bs, TablaFAT* FAT_table, const char* path) {
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	int n_entry, total_entries = (bs->bytes_per_sector * bs->sectors_per_cluster) / ROOT_ENTRY_BYTES;

	if (PFS_search_file_entry(bs, dir_entry, long_dir_entry, path) == 0) { // Busco la carpeta "path" y me devuelve su info basica
		// Definir
	}
}

void PFS_read_file_content(BootSector* bs, TablaFAT* FAT_table, const char* path) {
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));

	if (PFS_search_file_entry(bs, dir_entry, long_dir_entry, path) == 0) { // Busco el archivo "path" y me devuelve su info basica
		int i, n_entry, f_size_clusters = PFS_total_file_clusters(bs, dir_entry);
		//int f_clusters[f_size_clusters];
		int *f_clusters;
		f_clusters= (int*) malloc(sizeof(int));

		n_entry = (dir_entry->fst_cluster_high * 256 + dir_entry->fst_cluster_low); // Primer cluster del archivo
		f_clusters[0] = n_entry-2;
		printf("Primer entrada en la FAT: %d\n", n_entry);
		printf("Primer cluster en la Data Region: %d\n", f_clusters[0]);

		FAT_get_file_clusters(bs, FAT_table, f_clusters);

		printf("Cantidad de clusters del archivo: %d",f_size_clusters);

		for (i = 0; i < f_size_clusters; i++) {
			Adressing_read_cluster(bs, cluster, f_clusters[i]);
			PFSPrint_cluster(bs, *cluster, f_clusters[i]);
		}
	} else
		printf("No se encontró el archivo");
}

int PFS_total_file_clusters(BootSector* bs, DirEntry* dir_entry) {
	return ceil((double) dir_entry->file_size / (double) (bs->bytes_per_sector * bs->sectors_per_cluster));
}

int PFS_search_file_entry(BootSector* bs, DirEntry* dir_entry, LongDirEntry* long_dir_entry, const char* path) {
	Cluster* cluster = (Cluster*) malloc(sizeof(Cluster));
	int n_entry = 0;
	Adressing_read_cluster(bs, cluster, CLUS); // Modificar para que busque en el cluster necesario
	PFS_load_entry(cluster, dir_entry, long_dir_entry, n_entry);
	while (strcmp(PFS_file_name(dir_entry, long_dir_entry), path) && n_entry < 10 /*total_entries*/) {
		PFS_load_entry(cluster, dir_entry, long_dir_entry, ++n_entry);
	}
	if (strcmp(PFS_file_name(dir_entry, long_dir_entry), path))
		return -1;
	else
		return 0;
}

char* PFS_file_name(DirEntry* dir_entry, LongDirEntry* long_dir_entry) {
	char name[MAX_FILE_NAME_LENGHT];
	int i, k;
	if (long_dir_entry->attributes == ATRR_LONG_NAME) { // Si es nombre largo
		for (k = 0, i = 0; i < sizeof(long_dir_entry->name1)/2; i++, k++)
			name[i] = long_dir_entry->name1[k];
		for (k = 0; k < sizeof(long_dir_entry->name2)/2; i++, k++)
			name[i] = long_dir_entry->name2[k];
		for (k = 0; k < sizeof(long_dir_entry->name3)/2; i++, k++)
			name[i] = long_dir_entry->name3[k];
		name[i] = '\0';
	} else {
		for (k = 0, i = 0; i < sizeof(dir_entry->dos_file_name); i++, k++)
			name[i] = dir_entry->dos_file_name[k];
		name[i++] = '.';
		for(k=0;k<sizeof(dir_entry->dos_file_extension);i++,k++)
			name[i]=dir_entry->dos_file_extension[k];
		name[i] = '\0';
	}
	char *pName = (char*) malloc(sizeof(name)*sizeof(char));
	strcpy(pName, name);
	return pName;
}

void PFS_load_entry(Cluster* cluster, DirEntry* dir_entry, LongDirEntry* long_dir_entry, int n_entry) {
	char entry[ROOT_ENTRY_BYTES];
	int i, j = n_entry * ROOT_ENTRY_BYTES;
	for (i = 0; i < ROOT_ENTRY_BYTES; i++, j++)
		entry[i] = cluster->sector[0].byte[j];
	memcpy(long_dir_entry, entry, sizeof(entry));
	for (i = 0; i < ROOT_ENTRY_BYTES; i++, j++)
		entry[i] = cluster->sector[0].byte[j];
	memcpy(dir_entry, entry, sizeof(entry));
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
