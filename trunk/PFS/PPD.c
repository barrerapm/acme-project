#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "PPD.h"

ConfigPPD config_ppd; // Estructura global config_ppd
int fd;

void PPD_inicializar_config(void){
	FILE* fi;
	char var[255];
	fi=fopen("ConfigPPD.txt","rb");
	fscanf(fi,"%s = %s\n", var, config_ppd.path);
	fscanf(fi,"%s = %d\n", var, &config_ppd.bytes_per_sector);
	fscanf(fi,"%s = %d\n", var, &config_ppd.heads);
	fscanf(fi,"%s = %d\n", var, &config_ppd.cylinders);
	fscanf(fi,"%s = %d\n", var, &config_ppd.sectors_per_track);
	fclose(fi);
}

int32_t PPD_open_disk(){
	PPD_inicializar_config();
	print_config_ppd(); // Prueba
	if((fd=open(config_ppd.path,O_RDWR))==-1){
		return -1;
	}
 return 0;
}

void PPD_read_sector(Sector* sector,int n_sector){
	lseek(fd,SEEK_SET+(n_sector*config_ppd.bytes_per_sector),SEEK_SET);
	read(fd,sector->byte,config_ppd.bytes_per_sector);
}

int32_t PPD_write_sector(Sector* sector,int n_sector){
	lseek(fd,SEEK_SET+(n_sector*config_ppd.bytes_per_sector),SEEK_SET);
	write(fd,sector->byte,config_ppd.bytes_per_sector);
	return 0;
}

void PPD_close_disk(void){
	close(fd);
}

// Prints para pruebas

void print_config_ppd(void){
	printf("> Config PPD: ");
	printf("%s ", config_ppd.path);
	printf("%d ", config_ppd.bytes_per_sector);
	printf("%d ", config_ppd.heads);
	printf("%d ", config_ppd.cylinders);
	printf("%d ", config_ppd.sectors_per_track);
	ln();
}
