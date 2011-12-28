#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include "Socket.h"
#include "Pack.h"

ConfigSocket config_socket;

#define SOCK_PATH "/home/pablo/workspace2/Socket_unix"

//// SOCKET UNIX ////
//Abre un socket servidor de tipo AF_UNIX. Devuelve el descriptor del socket o -1 si hay problemas.
int Socket_open_server_unix(char* path_unix) {
	//Inicializamos la estructura direccion del servidor y su descriptor
	struct sockaddr_un* local_address = malloc(sizeof(struct sockaddr_un));
	int server_fd;

	//Abrimos el socket servidor
	if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return -1;

	//Se rellenan los campos de la estructura direccion del servidor, necesaria para ligar
	local_address->sun_family = AF_UNIX;
	strcpy(local_address->sun_path, path_unix);
	unlink(local_address->sun_path);
	int socket_length = strlen(local_address->sun_path) + sizeof(local_address->sun_family);

	int32_t optval;
	optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	if (bind(server_fd, (struct sockaddr *) local_address, socket_length) == -1) {
		close(server_fd);
		return -1;
	}

//Se avisa al sistema que comience a atender llamadas de clientes
	if (listen(server_fd, config_socket.max_connections) == -1) {
		close(server_fd);
		return -1;
	}

	free(local_address); // agregado 12-11
	return server_fd;
}

//Abre el cliente y lo conecta con el server. Recibe y devuelve cargado el descriptor del cliente. Retorna -1 en caso de error
int Socket_init_client_unix(int* client_fd, char* path_unix) {
//Abrimos el socket cliente
	if ((*client_fd = Socket_open_client_unix()) == -1)
		return -1;

// Realizamos la conexion con el server
	if (Socket_connect_to_server_unix(*client_fd, path_unix) == -1)
		return -1;

	return 0;
}

//Abre un socket cliente de tipo UNIX. Devuelve el descriptor del socket o -1 si hay problemas
int Socket_open_client_unix(void) {
//Inicializamos la estructura direccion del cliente y su descriptor
	struct sockaddr_un* local_address = malloc(sizeof(struct sockaddr_un));
	int client_fd;

//Abrimos el socket cliente
	if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return -1;

	int32_t optval;
	optval = 1;
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	free(local_address); // agregado 12-11
	return client_fd;
}

//Conecta con un servidor remoto a traves de socket UNIX. Recibe el descriptor del cliente y devuelve -1 si hay error
int Socket_connect_to_server_unix(int client_fd, char* path_unix) {
//Inicializamos las estructura direccion
	struct sockaddr_un* remote_address = malloc(sizeof(struct sockaddr_un));

//Direccion de donde me quiero conectar
	remote_address->sun_family = AF_UNIX;
	strcpy(remote_address->sun_path, path_unix);
	int socket_length = strlen(remote_address->sun_path) + sizeof(remote_address->sun_family);

	if (connect(client_fd, (struct sockaddr *) remote_address, socket_length) == -1) {
		perror("Error connect socket unix");
		printf("Path %s\n", remote_address->sun_path);
		return -1;
	}
	free(remote_address); // agregado 12-11
	return 0;
}
//// SOCKET INET ////

//Abre un socket servidor de tipo AF_INET. Devuelve el descriptor del socket o -1 si hay problemas
int Socket_open_server_inet(SocketConfig socket_config) {
	Aux_print_header("Open server inet", socket_config.ip);

	//Inicializamos la estructura direccion del servidor y su descriptor
	struct sockaddr_in* local_address = malloc(sizeof(struct sockaddr_in));
	int server_fd;

	//Abrimos el socket servidor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	//Se rellenan los campos de la estructura direccion del servidor, necesaria para ligar
	local_address->sin_family = AF_INET;
	local_address->sin_port = htons(socket_config.server_port);
	local_address->sin_addr.s_addr = inet_addr(socket_config.ip);

	int32_t optval;
	optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	if (bind(server_fd, (struct sockaddr *) local_address, sizeof(struct sockaddr_in)) == -1) {
		close(server_fd);
		return -1;
	}

	//Se avisa al sistema que comience a atender llamadas de clientes
	if (listen(server_fd, config_socket.max_connections) == -1) {
		close(server_fd);
		return -1;
	}
	free(local_address); // agregado 12-11
	return server_fd;
}

//Abre un socket cliente de tipo AF_INET. Devuelve el descriptor del socket o -1 si hay problemas
int Socket_open_client_inet(SocketConfig socket_config, int i) {
	Aux_print_header("Open client inet", socket_config.ip);

	int client_fd;

	struct hostent *host;
	host = gethostbyname(socket_config.ip);

	//Abrimos el socket cliente
	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	int32_t optval;
	optval = 1;
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	return client_fd;
}

//Conecta con un servidor remoto a traves de socket INET. Recibe el descriptor del cliente y devuelve -1 si hay error
int Socket_connect_to_server_inet(SocketConfig socket_config, int client_fd) {
	Aux_print_header("Connect to server inet", socket_config.ip);
	//Inicializamos las estructura direccion
	struct sockaddr_in* remote_address = malloc(sizeof(struct sockaddr_in));

	//Direccion de donde me quiero conectar
	remote_address->sin_family = AF_INET;
	remote_address->sin_addr.s_addr = inet_addr(socket_config.ip);
	remote_address->sin_port = htons(socket_config.server_port);

	//Aux_printf("\t client_fd: %d\t server_port: %d\t sin_port: %d\n", client_fd, socket_config.server_port, remote_address->sin_port);

	if (connect(client_fd, (struct sockaddr *) remote_address, sizeof(struct sockaddr_in)) == -1)
		return -1;

	free(remote_address); // agregado 12-11

	return 0;
}

//// COMPARTIDO POR AMBOS TIPOS DE SOCKETS ////

//Se le pasa un socket de servidor y acepta en el una conexion de cliente. Devuelve el descriptor del socket del cliente o -1 si hay problemas.
int Socket_accept_client(int server_fd) {
	Aux_print_header("Accept client", NULL);
	//Inicializamos la estructura direccion del cliente y su descriptor
	struct sockaddr_in* remote_address = malloc(sizeof(struct sockaddr_in));
	int address_len = sizeof(struct sockaddr_in);
	int client_fd;

	//Recibe el descriptor del servidor. Devuelve el descriptor del cliente, carga la estructura direccion y el tamaño del mismo
	Aux_printf("A aceptar...\n"); // PRUEBA
	if ((client_fd = accept(server_fd, (struct sockaddr *) remote_address, (void*) &address_len)) == -1)
		return -1;

	Aux_printf("Aceptada la conexion del Cliente: %d\n", client_fd); // PRUEBA

	free(remote_address); // agregado 12-11
	return client_fd;
}

int Socket_get_connection(Connection* connections_set, int* pos, sem_t* sem_cont_connections) {
	int i;
	sem_wait(sem_cont_connections);

	for (i = 0; connections_set[i].state != 0; i++)
		;

	*pos = i;

	connections_set[i].state = 1;

	return 0;
}

int Socket_release_connection(Connection* connections_set, int pos, sem_t* sem_cont_connections) {
	connections_set[pos].state = 0;
	sem_post(sem_cont_connections);
	return 0;
}

void Socket_load_config(char* path) {
	FILE* fi;
	char var[255];
	fi = fopen(path, "rb");
	fscanf(fi, "%s = %d\n", var, &(config_socket.client_port));
	fscanf(fi, "%s = %d\n", var, &(config_socket.server_port));
	fscanf(fi, "%s = %d\n", var, &(config_socket.max_connections));
	fclose(fi);
}
