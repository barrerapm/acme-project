#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include "Pack.h"
#include "Type.h"

#define HEAD_LEN 3
#define TRUE 1
#define FALSE 0

// Preparo paquete
void Pack_init_NIPC(NIPC* nipc, uint8_t type) {
	nipc->type = type;
	nipc->length = 0;
	nipc->payload = NULL;
}

void Pack_set_NIPC(NIPC* nipc, void *data, int length) {
	nipc->length = length;
	nipc->payload = (int8_t*) malloc(sizeof(int8_t) * length);
	memcpy(nipc->payload, data, nipc->length);
}

void Pack_serialized(NIPC* nipc, Pack* pack) {
	int offset, aux = 0;
	pack->length = sizeof(nipc->type) + sizeof(nipc->length) + nipc->length;
	pack->serialized = (int8_t*) malloc(sizeof(int8_t) * (pack->length));
	memcpy(pack->serialized, &(nipc->type), aux = sizeof(nipc->type));
	offset = aux;
	memcpy(pack->serialized + offset, &(nipc->length), aux = sizeof(nipc->length));
	offset += aux;
	memcpy(pack->serialized + offset, nipc->payload, nipc->length);
}

// Envío y luego:

void Pack_deserialized(NIPC* nipc, Pack* pack) {
	int offset, aux = 0;
	memcpy(&(nipc->type), pack->serialized, aux = sizeof(nipc->type));
	offset = aux;
	memcpy(&(nipc->length), pack->serialized + offset, aux = sizeof(nipc->length));
	offset += aux;
	(nipc->payload) = (int8_t*) malloc(sizeof(int8_t) * (nipc->length));
	memcpy(nipc->payload, pack->serialized + offset, nipc->length);

}

// Confirmacion de envio y recepcion
int32_t Pack_send(int socket, Pack* pack) {
	int bytes_total = 0, bytes_left = pack->length, sended = 0;

	while (bytes_total < pack->length) {
		if ((sended = send(socket, pack->serialized + bytes_total, bytes_left, MSG_NOSIGNAL)) == -1) {
			bytes_total = -1;
			break;
		}
		if (sended == 0) {
			bytes_total = 0;
			break;
		}

		bytes_total += sended;
		bytes_left -= sended;
	}

	return bytes_total;
}

// Recibe el descriptor del que recibe y el pack, modifica el parametro received y retorna 1: falte recibir head; 0: si recibio la cabeza; -1: error al recibir
char Pack_head_received(int socket, Pack* pack, int* received) {
	int aux;
	aux = recv(socket, pack->serialized + *received, HEAD_LEN, 0);
	*received += aux;
	if ((*received < HEAD_LEN) && (aux > 0))
		return TRUE; // Sigue el while
	else if (aux < 1)
		return -1;

	return FALSE; // Sale del while
}

// Recibe el descriptor del que recibe y devuelve el NIPC cargado con lo recibido y retorna cantidad recibida o -1 en caso de error al recibir
int32_t Pack_recv(int socket, Pack* pack) {
	int bytes_left, error = 0, aux = 0, received = 0;

	pack->serialized = (int8_t*) malloc(sizeof(int8_t) * HEAD_LEN);

	//Controla que la primer recepcion contenga hasta minimamente el payload length
	while ((error = Pack_head_received(socket, pack, &received)) > 0)
		;
	if (error == -1)
		return -1;
	if (received == 0)
		return 0;

	// Calculo de cuantos bytes faltan recibir
	bytes_left = (*((uint16_t *) (pack->serialized + 1))) - (received - HEAD_LEN);
	if (bytes_left != 0)
		pack->serialized = realloc(pack->serialized, (sizeof(int8_t) * bytes_left) + HEAD_LEN);

	//Completa el pack hasta recibirlo entero
	while (bytes_left > 0) {
		if ((aux = recv(socket, pack->serialized + received, bytes_left, 0)) == -1) {
			free(pack->serialized);
			free(pack);
			received = -1;
			break;
		}
		received += aux;
		bytes_left -= aux;
	}

	pack->length = received; //No sirve para nada pero igual esta cargado jaja

	return received;
}

// Serializa el paquete, lo envia, recibe la respuesta y lo deserializa
int Pack_process(int client_fd, NIPC* nipc) {

	Pack* pack = (Pack*) malloc(sizeof(Pack));

	//Serializamos el paquete NIPC
	Pack_serialized(nipc, pack);

	free(nipc->payload);

	//Enviamos pedido
	if (Pack_send(client_fd, pack) == -1)
		return -1;

	// Resetea el pack para volverlo a cargar
	Pack_reload_pack(pack);

	// Recibimos respuesta
	if ((Pack_recv(client_fd, pack)) == -1)
		return -1;
	//Deserializamos el paquete NIPC
	Pack_deserialized(nipc, pack);

	free(pack->serialized);
	free(pack);

	return 0;
}

void Pack_destroy(NIPC* nipc, Pack* pack) {
	free(nipc->payload);
	free(nipc);
	free(pack->serialized);
	free(pack);
}

void Pack_reload_pack(Pack* pack) {
	free(pack->serialized);
	//free(pack);
	//pack = (Pack*) malloc(sizeof(Pack));
}

// Convierte un payload en una estructura previamente alocada en memoria
void Pack_set_struct(NIPC* nipc, void* struc) {
	memcpy(struc, nipc->payload, nipc->length);
}
