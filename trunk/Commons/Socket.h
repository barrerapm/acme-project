#ifndef SOCKET_H_
#define SOCKET_H_

#include <semaphore.h>

// No iria, ver cuando se usan unix como usar la otra estructura
typedef struct config_socket {
	int client_port;
	int server_port;
	int max_connections;
} ConfigSocket;

typedef struct socket_config {
	char ip[20];
	unsigned short server_port;
	unsigned short max_connections;
} SocketConfig;

typedef struct connection {
	char state;
	int client_fd;
} Connection;

int Socket_open_server_inet(SocketConfig);
int Socket_open_server_unix(char*);
int Socket_open_client_inet(SocketConfig, int);
int Socket_open_client_unix(void);
int Socket_connect_to_server_inet(SocketConfig, int);
int Socket_connect_to_server_unix(int,char*);
int Socket_accept_client(int);
void Socket_load_config(char*);
int Socket_init_client_unix(int*,char*);

int Socket_get_connection(Connection* connections_set, int* pos, sem_t* sem_cont_connections);
int Socket_release_connection(Connection* connections_set, int pos, sem_t* sem_cont_connections);

#endif /* SOCKET_H_ */
