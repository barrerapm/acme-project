#ifndef PRAID1_H_
#define PRAID1_H_

#include "Pack.h"
#include "Socket.h"
#include "queue.h"
#include "list.h"

typedef struct sync_PPD {
	pthread_t thread_id;
	int* n_sync_sectors;
	t_queue* sync_petitions_queue;
	sem_t* semaf;
} Sync_PPD;

typedef struct info_PPD {
	int fd;
	char reply;
	Sync_PPD sync_PPD;
	NIPC* nipc;
} Info_PPD;

typedef struct info_RAID {
	char disk_id[25];
	pthread_t thread_id;
	char state;
	t_queue* petition_list_queue;
} Info_RAID;

typedef struct hilo_PPD {
	int client_fd;
	t_queue* my_petitions_queue;
	char* state;
	pthread_t* thread_id;
	pthread_mutex_t ready;
} Hilo_PPD;

typedef struct config_praid1 {
	SocketConfig socket_config_PFS;
	SocketConfig socket_config_PPD;
	int console;
} ConfigPRAID1;

typedef struct nsectype {
	uint32_t n_sec;
	char type;
} NsecType;


int PRAID1_hilo_consumidor(void);
void PRAID1_queue_push(int, NIPC*);
int PRAID1_resolve(int, NIPC*);

void PRAID1_queue_push_writes(Info_PPD* info_ppd, Info_RAID* info_raid);

int Equal_stateOK (void*, void*);
int Equal_nsec_type(void* info, void* info2);
void PRAID1_console_print(char*,...);
void PRAID1_inicializar_config(void);
int Equal_nsec(void* info, void* info2);
int Equals_disk_id(void* info, void* info2);
char* get_disk_id(char* disk_id);
Hilo_PPD* get_hilo_PPD(int client_fd, Info_RAID* info_raid);
Info_RAID* get_info_RAID(char* disk_id, char);

#endif /* PRAID1_H_ */
