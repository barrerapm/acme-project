#define FUSE_USE_VERSION 25
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "PFS.h"
#include "FUSE.h"

// Create and open a file
int FUSE_create(const char *path, mode_t mode, struct fuse_file_info *info) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path como argumento
	if (!path) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// File open operation
int FUSE_open(const char *path, struct fuse_file_info *info) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path como argumento
	if (!path) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// Read data from an open file
int FUSE_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path y el buf como argumento
	if (!path || !buf) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// Write data to an open file
int FUSE_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path y el buf como argumento
	if (!path || !buf) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// Possibly flush cached data
int FUSE_flush(const char *path, struct fuse_file_info *info) {

	return 0;
}

// Release an open file **/
int FUSE_release(const char *path, struct fuse_file_info *info) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path como argumento
	if (!path) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// Changes the size of file **/
int FUSE_truncate(const char *path, off_t offset) {
	printf("Paso por aca"); // Log

	return 0;
}

int FUSE_ftruncate(const char *path, off_t offset, struct fuse_file_info *info) {
	return FUSE_truncate(path, offset);
}

// Remove a file
int FUSE_unlink(const char *path) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path como argumento
	if (!path) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

// Create a directory
int FUSE_mkdir(const char *path, mode_t mode) {
	printf("Paso por aca"); // Log

	return 0;
}

// Read directory
int FUSE_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info) {
	(void) offset;
	(void) info;
	printf("Paso por aca"); // Log

    if(strcmp(path, "/") != 0){
    	printf("");
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "" + 1, NULL, 0);

	return 0;
}

// Remove a directory
int FUSE_rmdir(const char *path) {
	printf("Paso por aca"); // Log

	return 0;
}

// Get file attributes.
int FUSE_getattr(const char *path, struct stat *st) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el path como argumento
	if (path == NULL) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	return 0;
}

int FUSE_fgetattr(const char *path, struct stat *st, struct fuse_file_info *info) {
	return FUSE_getattr(path, st);
}

// Rename a file
int FUSE_rename(const char *src, const char *dest) {
	printf("Paso por aca"); // Log

	// Verifica que se recibe el src y el dest como argumento
	if (src == NULL || dest == NULL) {
		printf("Faltan argumentos"); // Log
		return -EINVAL;
	}

	// Si origen/destino son iguales, ignorar la llamada*/
	if (!strcmp(src, dest)) {
		printf("El path de origen/destino son iguales."); // Log
		return 0;
	}

	return 0;
}

