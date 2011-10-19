#ifndef PPD_H_
#define PPD_H_

typedef struct config_ppd {
	char path[255];
	unsigned short bytes_per_sector;
	unsigned char heads;
	unsigned char cylinders;
	unsigned short sectors_per_track;
} ConfigPPD;

typedef struct sector {
	unsigned char byte[512];
} Sector;

typedef struct cluster {
	struct sector sector[8];
} Cluster;

typedef struct block {
	struct cluster cluster[2];
} Block;

int PPD_open_disk(void);
void PPD_read_sector(Sector* sector, int n_sector);
int PPD_write_sector(Sector* sector, int n_sector);
void PPD_close_disk(void);

// Prints para pruebas
void print_config_ppd(void);

#endif /* PPD_H_ */
