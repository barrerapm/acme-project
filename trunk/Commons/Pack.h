#ifndef PACK_H_
#define PACK_H_
#define IP_MAX_SIZE 46

#include <stdint.h>
#include "log.h"

struct nipc {
	int8_t type;
	uint16_t length;
	int8_t* payload;
}__attribute__ ((packed));

struct pack {
	int8_t* serialized;
	uint16_t length;
}__attribute__ ((packed));

struct conection {
	int32_t socket;
	int32_t busy;
	int32_t position;
	int8_t ip[IP_MAX_SIZE];
}__attribute__ ((packed));

struct pending {
	int32_t socket;
	int8_t ip[IP_MAX_SIZE];
	int32_t busy;
	struct pending *next;
}__attribute__ ((packed));

typedef struct nipc NIPC;
typedef struct pack Pack;
typedef struct conection Conection;
typedef struct pending Pending;

void Pack_init_NIPC(NIPC*, uint8_t);
void Pack_set_NIPC(NIPC*, void *, int);
void Pack_serialized(NIPC*, Pack*);
void Pack_deserialized(NIPC*, Pack*);
void Pack_set_struct(NIPC*, void*);
int32_t Pack_recv(int, Pack*);
int32_t Pack_send(int, Pack*);
int Pack_process(int, NIPC*);
void Pack_reload_pack(Pack*);
void Pack_destroy(NIPC*, Pack*);


#endif /* PACK_H_ */
