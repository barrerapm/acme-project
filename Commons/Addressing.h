#ifndef Addressing_H_
#define Addressing_H_

#include "Struct.h"
#include "Socket.h"

int Addressing_read_sector(int, Sector*, uint32_t);
int Addressing_write_sector(int client_fd, Sector* sector, int n_sector);
int Addressing_disk_operation(Connection* connections_set, sem_t* sem_cont_connections, int type);

#endif /* Addressing_H_ */
