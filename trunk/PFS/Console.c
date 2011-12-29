#include <stdlib.h>
#include <stdio.h>
#include "PFS.h"
#include "Aux.h"
#include "FAT.h"
#include "FUSE.h"
#include "PFSPrint.h"
#include "Console.h"

extern ConfigPFS config_pfs;
extern BootSector* bs;
extern FSInfo* fs_info;

int n_commands = 19;
char* commands[] = { "fsinfo", "finfo", "bootsector", "fs_info", "fat", "create", "read", "write", "flush", "release", "truncate", "unlink", "mkdir", "readdir", "rmdir", "rename", "entries", "help",
		"exit" };
char* help_commands[] = { "fsinfo", "finfo path", "bootsector", "fs_info", "fat", "create path", "read x", "write x", "flush x", "release x", "truncate path offset", "unlink path", "mkdir path",
		"readdir path", "rmdir path", "rename path new_path", "entries", "help", "exit" };

void Console_PFS() {
	int (*functions[])(int, char**) = {
		Console_fsinfo, /*1*/
		Console_finfo, /*2*/
		Console_bootsector,/*3*/
		Console_fs_info, /*4*/
		Console_fat, /*5*/
		Console_create, /*6*/
		Console_read, /*7*/
		Console_write, /*8*/
		Console_flush, /*9*/
		Console_release, /*10*/
		Console_truncate, /*11*/
		Console_unlink, /*12*/
		Console_mkdir, /*13*/
		Console_readdir, /*14*/
		Console_rmdir, /*15*/
		Console_rename, /*16*/
		Console_entries, /*17*/
		Console_help, /*18*/
		Console_exit/*19*/};

	while (1) {
		printf(">> Ingrese el comando\n");
		printf(": ");
		if(Aux_do_command(functions, commands, n_commands)==2) {
			break;
		}
	}
}

int Console_fsinfo(int n_params, char **params) {
	printf(">> fsinfo\n");
	printf("Clusters ocupados: \t\t\t %u\n", PFS_n_clusters_by_bytes(bs->total_sectors2 * bs->bytes_per_sector) - fs_info->free_clusters);
	printf("Clusters libres: \t\t\t %u\n", fs_info->free_clusters);
	printf("Tamaño de un sector: \t\t\t %d Bytes\n", bs->bytes_per_sector);
	printf("Tamaño de un cluster: \t\t\t %d Bytes\n", config_pfs.blocks_per_cluster * config_pfs.sectors_per_block * bs->bytes_per_sector);
	printf("Tamaño de la FAT: \t\t\t %.2f KBs\n", (double) ((bs->fat32_sectors_per_fat) * (bs->bytes_per_sector)) / 1024.);
	return 0;
}

int Console_help(int n_params, char **params) {
	Aux_print_string_vec(help_commands, n_commands);
	return 0;
}

int Console_finfo(int n_params, char **params) {
	if (params[1] == NULL) {
		return -1;
	}
	int i, n_file_clusters, n_entry, n_cluster_content;
	DirEntry* dir_entry = (DirEntry*) malloc(sizeof(DirEntry));
	LongDirEntry* long_dir_entry = (LongDirEntry*) malloc(sizeof(LongDirEntry));
	if (PFS_get_path_entries(params[1], long_dir_entry, dir_entry, &n_entry, &n_cluster_content) == -1) {
		printf("No existe el archivo\n");
	} else {
		//PFS_destroy_entries(long_dir_entry, dir_entry);
		int *file_clusters = (int*) malloc(sizeof(int));
		printf(">> finfo\n");
		if ((file_clusters[0] = PFS_n_first_file_cluster(dir_entry)) == -2) {
			printf("El archivo no tiene clusters asignados\n");
		} else {
			FAT_get_file_clusters(&file_clusters, &n_file_clusters);
			printf("[%d", file_clusters[0]);
			for (i = 1; i < n_file_clusters; i++) {
				printf(", %d", file_clusters[i]);
				if ((i % 20) == 0) {
					printf("\n");
				}
			}
			printf("]\n");
			printf("Total: %d clusters\n",n_file_clusters);
		}
		free(file_clusters);
	}
	return 0;
}

// path
int Console_create(int n_params, char **params) {
	PFS_create_file(params[1]);
	return 0;
}
// path buf
int Console_read(int n_params, char **params) {
	/*
	 char *buf = (char*) malloc(sizeof(Cluster) - atoi(params[2]));
	 FUSE_read(params[1], buf, size, atoi(params[2]), info);
	 free(buf);
	 */
	return 0;
}
// path buf size offset
int Console_write(int n_params, char **params) {
	FUSE_write(params[1], params[2], atoi(params[3]), atoi(params[4]), NULL);
	return 0;
}
//
int Console_flush(int n_params, char **params) {

	return 0;
}
//
int Console_release(int n_params, char **params) {

	return 0;
}
// path offset
int Console_truncate(int n_params, char **params) {
	FUSE_truncate(params[1], atoi(params[2]));
	return 0;
}
// path
int Console_unlink(int n_params, char **params) {
	PFS_set_path_entries_free(params[1]);
	return 0;
}
// path
int Console_mkdir(int n_params, char **params) {
	PFS_create_dir(params[1]);
	return 0;
}
// path
int Console_readdir(int n_params, char **params) {
	if (params[1] == NULL) {
		return -1;
	}
	int n_file_names;
	char** file_names = (char**) malloc(sizeof(char*));
	PFS_directory_list(params[1], &file_names, &n_file_names);
	int i;
	for (i = 0; i < n_file_names; i++) {
		printf("Archivo: %s\n", file_names[i]);
	}
	Aux_string_vec_destroy(&file_names, n_file_names - 1);
	free(file_names);
	return 0;
}
// path
int Console_rmdir(int n_params, char **params) {
	Console_unlink(n_params, params);
	return 0;
}
// path new_path
int Console_rename(int n_params, char **params) {
	PFS_rename_path(params[1], params[2]);
	return 0;
}

int Console_bootsector(int n_params, char **params) {
	PFSPrint_bootsector();
	return 0;
}

int Console_fs_info(int n_params, char **params) {
	PFSPrint_fs_info();
	return 0;
}

int Console_fat(int n_params, char **params) {
	PFSPrint_FAT_table();
	return 0;
}
int Console_exit(int n_params, char **params) {
	PFS_exit();
	return 2;
}
int Console_entries(int n_params, char **params) {
	PFS_file_list();
	return 0;
}
