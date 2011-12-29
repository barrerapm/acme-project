#ifndef PPD_H_
#define PPD_H_

#include "Socket.h"
#include "Struct.h"
#include "Pack.h"

typedef struct sector_fis {
	uint32_t cylinder;
	uint16_t sector;
} Sector_Fis;

typedef struct info {
	char type;
	int fd;
	Sector_Fis sector_fis;
	Sector sector;
} Info;

typedef struct nodo {
	Info* info;
	struct nodo* sgte;
} Nodo;

typedef struct config_ppd {
	char path[255];
	uint16_t bytes_per_sector;
	uint8_t heads;
	uint32_t cylinders;
	uint16_t sectors_per_track;
	char algoritmo[15];
	uint16_t RPM;
	uint8_t T_Lectura;
	uint8_t T_Escritura;
	uint8_t T_Seek;
	char modo_inicio[15];
	char path_unix[255];
	char ID_Disco[25];
	uint8_t LOG;
	float Sectors_ms;
	uint32_t Retardo_R;
	uint32_t Retardo_W;
	SocketConfig socket_config_RAID;
	SocketConfig socket_config_PFS;
	char console_path[255];
} ConfigPPD;

typedef struct info_trace {
	Sector_Fis estado;
	Sector_Fis destino;
	Sector_Fis proximo;
	uint16_t sectors_per_track;
	double latency;
} Info_Trace;

int PPD_hilo_consumidor(void);

int PPD_open_disk(void);
int32_t PPD_read_sector(Sector*, int);
int32_t PPD_write_sector(Sector*, int);
int32_t PPD_close_disk(void);
int PPD_solve_read(Info*, NIPC*);
int PPD_solve_write(Info*, NIPC*);
int PPD_resolve(int, NIPC*, Pack*);
void PPD_queue_pop(Info* info, Info* prox);
int PPD_solve_trace(Info* info, Info* prox, NIPC* nipc);

// Prints para pruebas
void print_config_ppd(void);


void sector_log_fis(uint32_t, Sector_Fis*);
void sector_fis_log(Sector_Fis*, uint32_t*);

void inc_estado_RW(uint8_t); //AGREGADO MARTIN 11-11-30
void inc_estado(void); //ARREGLADO MARTIN 11-11-30
double latency_time(Sector_Fis*); //ARREGLADO MARTIN 11-11-30

void PPD_log_print(char* log_type, char* thread, char* info);

#endif /* PPD_H_ */
