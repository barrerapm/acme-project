#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "Struct.h"
#include "PRAID1.h"
#include "HILO_PPD.h"
#include "log.h"
#include "Pack.h"
#include "Socket.h"
#include "Type.h"
#include "Addressing.h"
#include "list.h"
#include "queue.h"
#include "Aux.h"

t_log* project_log;
ConfigPRAID1 config_praid1;

#define MAX_EVENTS 100 /* LO CONSIDERO UN NRO LO SUFICIENTEMENTE GRANDE, NO SOBREPASA LA CANTIDAD DE CONEXIONES */

// Creamos el contador de conexiones
int total_connections; /* NO SE SI ES NECESARIO */

// Creamos los descriptores del server y del epoll
int server_fd_PFS, server_fd_PPD, epoll_fd;

// Declaramos la cola de pedido general
t_queue* general_petitions_queue = NULL;
t_list* PPDs_list = NULL;

// Creamos el evento del epoll
struct epoll_event event;

CHS_id* chs_id;

int main(void) {
	chs_id = (CHS_id*) malloc(sizeof(CHS_id));

	// Creamos la cola de peticiones generales
	general_petitions_queue = collection_queue_create();

	// Creamos la lista de PPDs circular
	PPDs_list = collection_list_create();

	PRAID1_inicializar_config();

	PRAID1_console_print(">> Consola PRAID1: RAID en funcionamiento\n");

	struct epoll_event events[MAX_EVENTS];

	int num_fds, client_fd;

	project_log = log_create("Runner", "log.txt", DEBUG | INFO | WARNING, M_CONSOLE_ENABLE);

	// Preparamos el socket servidor para que se conecte el PPD
	if ((server_fd_PPD = Socket_open_server_inet(config_praid1.socket_config_PPD)) == -1) {
		log_error(project_log, "[main]", "%s", "Error al abrir server inet del PPD");
		return -1;
	}
	Aux_printf("PPD_server_fd: %d\n", server_fd_PPD);

	// Preparamos el socket servidor para que se conecte el PFS
	if ((server_fd_PFS = Socket_open_server_inet(config_praid1.socket_config_PFS)) == -1) {
		log_error(project_log, "[main]", "%s", "Error al abrir server inet del PFS");
		return -1;
	}
	Aux_printf("PFS_server_fd: %d\n", server_fd_PFS);

	// Creo el descriptor de epoll
	if ((epoll_fd = epoll_create(10)) == -1) {
		log_error(project_log, "[main]", "%s", "Error al crear el epoll");
		return -1;
	}

// Preparo el evento del server PFS para luego cargarlo
	event.events = EPOLLIN;
	event.data.fd = server_fd_PFS;

// Agrego el descriptor del server PFS y su evento a epoll
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd_PFS, &event) == -1) {
		log_error(project_log, "[main]", "%s", "Error al agregar el server del PFS al epoll (epoll_ctl: listen_sock)");
		return -1;
	}
	++total_connections;

	// Preparo el evento del server PPD para luego cargarlo
	event.events = EPOLLIN;
	event.data.fd = server_fd_PPD;

	// Agrego el descriptor del server PPD y su evento a epoll
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd_PPD, &event) == -1) {
		log_error(project_log, "[main]", "%s", "Error al agregar el server del PPD al epoll (epoll_ctl: listen_sock)");
		return -1;
	}
	++total_connections;

	pthread_t RAID_consumidor;
	pthread_create(&RAID_consumidor, NULL, (void*) &PRAID1_hilo_consumidor, NULL);

	NIPC* nipc = NULL;
	Pack* pack = NULL;

	Info_RAID* info_raid;

	int n;

	unsigned int nodo_pos;

	Hilo_PPD* hilo_PPD;
	PPD_CHS_id* PPD_chs_id;
	char* id_aux = NULL;

	while (1) { // ARREGLAR ESTE IF GIGANTE // ARREGLAR ESTE IF GIGANTE // ARREGLAR ESTE IF GIGANTE // ARREGLAR ESTE IF GIGANTE

		Aux_printf("[MAIN_RAID] ESPERANDO CONEXION (%ld)\n", Aux_actual_microseconds());
		aux: if ((num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) == -1) {
			log_error(project_log, "[main]", "%s", "Error en el epoll_wait");
			goto aux;
			return -1;
		}

		Aux_printf("[MAIN_RAID] ALGUIEN SE QUIERE CONECTAR (%ld)\n", Aux_actual_microseconds());

		for (n = 0; n < num_fds; n++) {
			Aux_printf("NUM_FDS: %d\n", num_fds);
			Aux_printf("FOR ITERACION: %d\n", n);

			Aux_printf("[MAIN_RAID] Conexion-------------> client_fd: %d\n", events[n].data.fd);

			if (events[n].data.fd == server_fd_PPD) {
				// SE QUIERE CONECTAR UN PPD

				if ((client_fd = Socket_accept_client(server_fd_PPD)) == -1) {
					log_error(project_log, "[main]", "%s", "Error al aceptar el server del PPD");
					return -1;
				}

				Aux_printf("[MAIN_RAID] ESTAMOS CONECTADOS CON UN PPD\n");

				nipc = (NIPC*) malloc(sizeof(NIPC));
				pack = (Pack*) malloc(sizeof(Pack));

				/* RECIBE EL CHS DENTRO DEL HANDSHAKE */
				// Cargamos el pack con lo recibido
				if ((Pack_recv(client_fd, pack)) == -1) {
					log_error(project_log, "[main]", "%s", "Error al recibir el Handshale con CHS del PPD");
					return -1;
				}

				Aux_printf("[MAIN_RAID] RECIBI DEL CLIENTE\n");

				//Deserializamos el paquete
				Pack_deserialized(nipc, pack);

				free(pack->serialized);
				free(pack);

				Aux_printf("[MAIN_RAID] DESERIALICE\n");

				PPD_chs_id = (PPD_CHS_id*) malloc(sizeof(PPD_CHS_id));
				Pack_set_struct(nipc, PPD_chs_id);

				free(nipc->payload);
				free(nipc);

				pthread_mutex_lock(&(PPDs_list->external_mutex));

				if (collection_list_isEmpty(PPDs_list)) {
					Aux_printf("PRIMER ELEMENTO\n");
					// EL PPD QUE SE CONECTO ES EL MASTER (PRO)
					pthread_mutex_unlock(&(PPDs_list->external_mutex));

					Aux_printf("ANTES CREATE LIST PRIMER ELEMENTO\n");
					chs_id->disk_id_list = collection_list_create();

					id_aux = get_disk_id(PPD_chs_id->disk_id);

					Aux_printf("ANTES ADD PRIMER ELEMENTO\n");
					collection_list_add(chs_id->disk_id_list, id_aux);

					chs_id->cylinder = PPD_chs_id->cylinder;
					chs_id->head = PPD_chs_id->head;
					chs_id->sector = PPD_chs_id->sector;

					nipc = (NIPC*) malloc(sizeof(NIPC));
					Pack_init_NIPC(nipc, TYPE_HANDSHAKE);

					pack = (Pack*) malloc(sizeof(Pack));
					Pack_serialized(nipc, pack);

					if (Pack_send(client_fd, pack) == -1) {
						log_error(project_log, "[main]", "%s", "Error al enviar el Handshake con CHS del PPD");
						exit(-1);
					}

					free(nipc->payload);
					free(nipc);
					free(pack->serialized);
					free(pack);

					Aux_printf("[MAIN_RAID] VOY A CREAR EL HILO DEL PPD\n");

					//Cargamos el nodo de cada PPD con la info restante
					info_raid = get_info_RAID(PPD_chs_id->disk_id, STATE_OK);

					free(PPD_chs_id);

					Aux_printf("ANTES CONSOLE PRINT\n");
					PRAID1_console_print(">> Consola PRAID1: Se conecto el PPD: %s\n", info_raid->disk_id);

					Aux_printf("[MAIN_RAID] VOY A AGREGAR EL PPD A LA LISTA\n");

					pthread_mutex_lock(&(PPDs_list->external_mutex));

					// Agregamos a la lista de PPDs un nodo
					nodo_pos = collection_list_add(PPDs_list, info_raid);

					hilo_PPD = get_hilo_PPD(client_fd, info_raid);

					pthread_mutex_unlock(&(PPDs_list->external_mutex));

					pthread_mutex_init(&hilo_PPD->ready, NULL);

					// Creamos el hilo del PPD
					Aux_printf("[MAIN_RAID]                               !!!!!!!!hilo_PPD: %p\n", hilo_PPD);
					Aux_printf("[MAIN_RAID]                               !!!!!!!!hilo_PPD->my_petitions_queue: %p\n", hilo_PPD->my_petitions_queue);

					pthread_create(&(info_raid->thread_id), NULL, (void*) &PRAID1_PPD, hilo_PPD);

					//info_raid->thread_id = hilo_PPD->thread_id;

					if (*hilo_PPD->thread_id == info_raid->thread_id) {
						Aux_printf("\t LOS THREAD_ID SON IGUALES!!!   %lu\n", *hilo_PPD->thread_id);
					} else {
						Aux_printf("\t LOS THREAD_ID SON DISTINTOS OH SHIT!!!!!\n");
					}

					Aux_printf("[MAIN_RAID] AGREGUE EL PPD A LA LISTA\n");

					pthread_mutex_lock(&hilo_PPD->ready);
					pthread_mutex_lock(&hilo_PPD->ready);

				} else {
					// YA HAY AL MENOS UN PPD, ENTONCES VERIFICAMOS LA COMPATIBILIDAD DEL QUE SE CONECTO
					Aux_printf("MAYOR PRIMER ELEMENTO\n");

					pthread_mutex_unlock(&(PPDs_list->external_mutex));

					id_aux = get_disk_id(PPD_chs_id->disk_id);

					if ((collection_list_find(chs_id->disk_id_list, Equals_disk_id, &(PPD_chs_id->disk_id)) == NULL) && (chs_id->cylinder == PPD_chs_id->cylinder) && (chs_id->head == PPD_chs_id->head)
							&& (chs_id->sector == PPD_chs_id->sector)) {

						collection_list_add(chs_id->disk_id_list, id_aux);

						nipc = (NIPC*) malloc(sizeof(NIPC));
						Pack_init_NIPC(nipc, TYPE_HANDSHAKE);

						pack = (Pack*) malloc(sizeof(Pack));
						Pack_serialized(nipc, pack);

						if (Pack_send(client_fd, pack) == -1) {
							log_error(project_log, "[main]", "%s", "Error al enviar el Handshake con CHS del PPD");
							// return -1;
						}

						free(nipc->payload);
						free(nipc);
						free(pack->serialized);
						free(pack);

						// Cargamos el info

						// MODIFICADO 13-12
						info_raid = get_info_RAID(PPD_chs_id->disk_id, STATE_SYNC);

						free(PPD_chs_id);
						PRAID1_console_print(">> Consola PRAID1: Se conecto el PPD: %s\n", info_raid->disk_id);
						pthread_mutex_lock(&(PPDs_list->external_mutex));

						// Agregamos a la lista de PPDs un nodo
						nodo_pos = collection_list_add(PPDs_list, info_raid);

						hilo_PPD = get_hilo_PPD(client_fd, info_raid);

						pthread_mutex_unlock(&(PPDs_list->external_mutex));

						pthread_mutex_init(&hilo_PPD->ready, NULL);

						// Creamos el hilo del PPD
						pthread_create(&(info_raid->thread_id), NULL, (void*) &PRAID1_PPD, hilo_PPD);

						//info_raid->thread_id = hilo_PPD->thread_id;

						if (*hilo_PPD->thread_id == info_raid->thread_id) {
							Aux_printf("\t LOS THREAD_ID SON IGUALES!!!    %lu\n", *hilo_PPD->thread_id);
						} else {
							Aux_printf("\t LOS THREAD_ID SON DISTINTOS OH SHIT!!!!!\n");
						}

						pthread_mutex_lock(&hilo_PPD->ready);
						pthread_mutex_lock(&hilo_PPD->ready);

					} else {
						//  retorna error al PPD "disco incompatible"
						Aux_printf("->NINGUN ELEMENTO O DISCO INCOMPATIBLE\n");
						nipc = (NIPC*) malloc(sizeof(NIPC));
						Pack_init_NIPC(nipc, TYPE_HANDSHAKE);
						char* error_chs = (char*) malloc(sizeof(char));
						*error_chs = 1;
						Pack_set_NIPC(nipc, error_chs, sizeof(char));
						free(error_chs);
						free(PPD_chs_id);
						pack = (Pack*) malloc(sizeof(Pack));
						Pack_serialized(nipc, pack);

						if (Pack_send(client_fd, pack) == -1) {
							log_error(project_log, "[main]", "%s", "Disco incompatible, por CHS invalido o por ID existente");
						}

						free(nipc->payload);
						free(nipc);
						free(pack->serialized);
						free(pack);
					}
				}

			} else if (events[n].data.fd == server_fd_PFS) {

				if ((client_fd = Socket_accept_client(server_fd_PFS)) == -1) {
					log_error(project_log, "[main]", "%s", "Error al aceptar el cliente del PFS");
					return -1;
				}

				Aux_printf("[MAIN_RAID] ESTAMOS CONECTADOS CON PFS fd: %d\n", client_fd);

				if (!collection_list_isEmpty(PPDs_list)) {
					// TENGO A LO SUMO UN PPD CONECTADO Y UN PFS SE QUIERE CONECTAR

					event.events = EPOLLIN;
					event.data.fd = client_fd;

					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						log_error(project_log, "[main]", "%s", "Error al agregar el cliente del PFS al epoll (epoll_ctl: listen_sock)");
						return -1;
					}
					++total_connections;

					Aux_printf("[MAIN_RAID] AGREGUE LA NUEVA CONEXION AL EPOLL\n");

				} else {
					// retorna error al PFS "falta disco"
					Aux_printf("[MAIN_RAID] SE CONECTO UN PFS Y RAID NO TIENE DISCOS\n");

					nipc = (NIPC*) malloc(sizeof(NIPC));
					pack = (Pack*) malloc(sizeof(Pack));

					/* RECIBE EL CHS DENTRO DEL HANDSHAKE */
					// Cargamos el pack con lo recibido
					if ((Pack_recv(client_fd, pack)) == -1) {
						log_error(project_log, "[main]", "%s", "Error al recibir el Handshake del PFS");
						return -1;
					}

					Aux_printf("[MAIN_RAID] RECIBI DEL CLIENTE\n");

					//Deserializamos el paquete
					Pack_deserialized(nipc, pack);

					Pack_reload_pack(pack);

					Pack_init_NIPC(nipc, TYPE_HANDSHAKE);
					char* error_inactive = (char*) malloc(sizeof(char));
					*error_inactive = 1;
					Pack_set_NIPC(nipc, error_inactive, sizeof(char));
					free(error_inactive);

					Pack_serialized(nipc, pack);

					if (Pack_send(client_fd, pack) == -1) {
						log_error(project_log, "[main]", "%s", "Error al enviar respuesta de Handshake al PFS");
					}

					Aux_printf("Envie la rta\n");
					log_error(project_log, "[main]", "%s", "Error al conectarse un PFS, el RAID no tiene discos");

					free(nipc->payload);
					free(nipc);
					free(pack->serialized);
					free(pack);

					exit(-1);
				}

			} else {
				// 	EL PFS ME ESTA HACIENDO UN SEND /* */

				nipc = (NIPC*) malloc(sizeof(NIPC));
				pack = (Pack*) malloc(sizeof(Pack));

				Aux_printf("[MAIN_RAID] HAY UN CLIENTE HACIENDO SEND fd: %d\n", events[n].data.fd);

				// Cargamos el pack con lo recibido
				if ((Pack_recv(events[n].data.fd, pack)) == -1) {
					log_error(project_log, "[main]", "%s", "Error al recibir un pedido del PFS");
					return -1;
				}

				Aux_printf("[MAIN_RAID] RECIBI DEL PFS\n");

				//Deserializamos el paquete
				Pack_deserialized(nipc, pack);

				free(pack->serialized);
				free(pack);

				Aux_printf("[MAIN_RAID] DESERIALICE\n");

				Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));

				Pack_set_struct(nipc, sec_nsec);
				Aux_printf("\t[MAIN_RAID] OBTUVE EL N_SEC(%d) DEL NIPC DEL NODO\n", sec_nsec->n_sector);
				Aux_printf("\t[MAIN_RAID] EL TYPE ES: %d\n", nipc->type);
				free(sec_nsec);

				//Resolvemos el pedido y armamos el paquete NIPC en caso de ser necesario
				if ((PRAID1_resolve(events[n].data.fd, nipc)) == -1) {
					log_error(project_log, "[main]", "%s", "Error al resolver una operacion del PFS");
					return -1;
				}

				Aux_printf("[MAIN_RAID] RESOLVI EL PEDIDO\n");

				//free(nipc->payload);
				//free(nipc);
			}
		}
		/*if (total_connections == 0)
		 break;*/ // REVISAR
	}

	Aux_printf("ME CERRE, TODO PIOLA :D\n");

	return 0;
}

Hilo_PPD* get_hilo_PPD(int client_fd, Info_RAID* info_raid) {
	Hilo_PPD* hilo_PPD = (Hilo_PPD*) malloc(sizeof(Hilo_PPD));
	// Cargamos la estructura que mandamos como parametro al hilo PPD
	hilo_PPD->client_fd = client_fd;
	hilo_PPD->thread_id = &(info_raid->thread_id);
	hilo_PPD->my_petitions_queue = info_raid->petition_list_queue;
	hilo_PPD->state = &(info_raid->state);
	return hilo_PPD;
}

Info_RAID* get_info_RAID(char* disk_id, char state) {
	Info_RAID* info_raid = (Info_RAID*) malloc(sizeof(Info_RAID));
	info_raid->state = state;
	info_raid->petition_list_queue = collection_queue_create();
	memcpy(&(info_raid->disk_id), disk_id, strlen(disk_id) + 1);
	return info_raid;
}

char* get_disk_id(char* disk_id) {
	char* id_aux = (char*) malloc(strlen(disk_id) + 1);
	memcpy(id_aux, disk_id, strlen(disk_id) + 1);
	return id_aux;
}

int PRAID1_hilo_consumidor() {

	Info_PPD* info_ppd = NULL;
	Info_RAID* info_raid = NULL;

	Aux_printf("[PRAID1_hilo_consumidor] SOY EL HILO PRAID1_hilo_consumidor, ENTRE! (%ld)\n", Aux_actual_microseconds());

	int PPD_pos;

	while (1) {

		Aux_printf("\t[PRAID1_hilo_consumidor] ME DISPONGO A SACAR DE LA COLA (%ld)\n", Aux_actual_microseconds());

		Aux_printf("\t[PRAID1_hilo_consumidor] LONGITUD DE LA COLA ANT}ES DE SACAR (%d)\n", collection_queue_size(general_petitions_queue));

		info_ppd = collection_queue_pop(general_petitions_queue);

		Aux_printf("\t[PRAID1_hilo_consumidor] SALI DE SACAR DE COLA (%ld)\n", Aux_actual_microseconds());

		Aux_printf("\t[PRAID1_hilo_consumidor] info_ppd TIENE TYPE: %d\n", info_ppd->nipc->type);

		Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
		Pack_set_struct(info_ppd->nipc, sec_nsec);
		Aux_printf("\t[PRAID1_hilo_consumidor] OBTUVE EL N_SEC(%d) DEL NIPC DEL NODO\n", sec_nsec->n_sector);
		free(sec_nsec);

		Aux_printf("\t[PRAID1_hilo_consumidor] SAQUE DE LA COLA! el FD: %d \n", info_ppd->fd);

		if (PPDs_list == NULL) {
			Aux_printf("\t[PRAID1_hilo_consumidor] PPDslist NULL!!\n");
		}

		pthread_mutex_lock(&(PPDs_list->external_mutex));

		Aux_printf("\t[PRAID1_hilo_consumidor] pase el mutex external del PPDslist\n");

		if (info_ppd->nipc->type == TYPE_READ) {
			info_ppd->reply = REPLY;
			info_raid = collection_list_removeByClosure2(PPDs_list, Equal_stateOK, NULL);
			PRAID1_console_print(">> Consola PRAID1: Se hizo un read del PPD: %s\n", info_raid->disk_id); // PROBLEMA PARA LA SYNC? TIRA CUALQUIERA EL EQUAL?

			Aux_printf("\t[PRAID1_hilo_consumidor] CANTIDAD DE ELEMENTOS EN LA LISTA DE PETICIONES DE UN PPD ANTES DE AGREGAR: %d \n", collection_queue_size(info_raid->petition_list_queue));

			collection_queue_push(info_raid->petition_list_queue, info_ppd);

			collection_list_add(PPDs_list, info_raid);

			Aux_printf("[PRAID1_hilo_consumidor] voy a pthread_kill\n");
			pthread_kill(info_raid->thread_id, SIGUSR1);
			Aux_printf("[PRAID1_hilo_consumidor] HICE UN WHILE\n");

			Aux_printf("\t[PRAID1_hilo_consumidor] CANTIDAD DE ELEMENTOS EN LA LISTA DE PETICIONES DE UN PPD DESPUES DE AGREGAR: %d \n", collection_queue_size(info_raid->petition_list_queue));

		} else if (info_ppd->nipc->type == TYPE_WRITE) {

			for (PPD_pos = 0; collection_list_size(PPDs_list) > PPD_pos; PPD_pos++) {

				if (PPD_pos == 0) {
					info_ppd->reply = REPLY;
				} else {
					info_ppd->reply = NO_REPLY;
				}

				info_raid = collection_list_remove(PPDs_list, 0);
				PRAID1_console_print(">> Consola PRAID1: Se hizo un write del PPD: %s\n", info_raid->disk_id);

				PRAID1_queue_push_writes(info_ppd, info_raid);

				collection_list_add(PPDs_list, info_raid);

				Aux_printf("[PRAID1_hilo_consumidor] voy a pthread_kill\n");
				pthread_kill(info_raid->thread_id, SIGUSR1);
				Aux_printf("[PRAID1_hilo_consumidor] HICE UN WHILE\n");

			}

			free(info_ppd->nipc->payload);
			free(info_ppd->nipc);
			free(info_ppd);
		}

		pthread_mutex_unlock(&(PPDs_list->external_mutex));

		Aux_printf("[PRAID1_hilo_consumidor] cargue en PPD\n");

	}
	return 0;
}

void PRAID1_queue_push_writes(Info_PPD* info_ppd, Info_RAID* info_raid) {

	Info_PPD* info_ppd2 = (Info_PPD*) malloc(sizeof(Info_PPD));
	info_ppd2->nipc = (NIPC*) malloc(sizeof(NIPC));
	info_ppd2->nipc->payload = (int8_t*) malloc(info_ppd->nipc->length);

	info_ppd2->fd = info_ppd->fd;
	info_ppd2->reply = info_ppd->reply;
	info_ppd2->sync_PPD = info_ppd->sync_PPD;
	info_ppd2->nipc->type = info_ppd->nipc->type;
	info_ppd2->nipc->length = info_ppd->nipc->length;
	memcpy(info_ppd2->nipc->payload, info_ppd->nipc->payload, info_ppd->nipc->length);

	collection_queue_push(info_raid->petition_list_queue, info_ppd2);

}

int PRAID1_resolve(int fd, NIPC* nipc) {
	Info_PPD* info_ppd = NULL;

	Pack* pack = NULL;

	Aux_printf("\t[PRAID1_resolve] ENTRE EN EL RESOLVE\n");
	Aux_printf("\t[PRAID1_resolve] EL TYPE ES: %d\n", nipc->type);

	Sector_n_sector* sec_nsec;

	switch (nipc->type) {
	case TYPE_HANDSHAKE:
		/* NO SE Q PIJA HABRIA Q HACER EN CASO DE ERROR XD */

		//Serializamos el paquete NIPC
		pack = (Pack*) malloc(sizeof(Pack));
		Pack_serialized(nipc, pack);

		//Enviamos respuesta al pedido
		if (Pack_send(fd, pack) == -1) {
			log_warning(project_log, "[PRAID1_resolve]", "%s", "No se pudo enviar el Handshake");
			free(pack->serialized);
			free(pack);
			return -1;
		}

		free(pack->serialized);
		free(pack);

		Aux_printf("\t[PRAID1_resolve] HICE EL HANDSHAKE CON EL PFS\n");
		break;
	case TYPE_READ:
		Aux_printf("[PRAID1_resolve] ENTRE EN EL READ\n");

		sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
		Pack_set_struct(nipc, sec_nsec);
		Aux_printf("\t[PRAID1_PPD_resolve] OBTUVE EL N_SEC(%d) DEL NIPC\n", sec_nsec->n_sector);
		free(sec_nsec);

		info_ppd = (Info_PPD*) malloc(sizeof(Info_PPD));

		Aux_printf("[PRAID1_resolve] HICE EL MALLOC\n");

		info_ppd->fd = fd;

		// CORREGIDO 11-12-21 (MARTIN)
		//info_ppd->nipc = (NIPC*) malloc(sizeof(NIPC));
		//info_ppd->nipc->payload = (int8_t*) malloc(nipc->length);
		//memcpy(info_ppd->nipc, nipc, sizeof(NIPC));
		info_ppd->nipc = nipc;
		info_ppd->sync_PPD.sync_petitions_queue = NULL;
		//memcpy(info_ppd->nipc->payload, nipc->payload, nipc->length);

		sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
		Pack_set_struct(info_ppd->nipc, sec_nsec);
		Aux_printf("\t[PRAID1_PPD_resolve] OBTUVE EL N_SEC(%d) DEL NIPC DEL NODO\n", sec_nsec->n_sector);
		free(sec_nsec);

		Aux_printf("[PRAID1_resolve] VOY A METER EN LA COLA GENERAL DE PEDIDOS\n");

		Aux_printf("\t[PRAID1_resolve] EN LA COLA GRAL DE PEDIDOS TENGO: %d\n", collection_queue_size(general_petitions_queue));

		// Agregamos el pedido a la cola gral de pedidos
		collection_queue_push(general_petitions_queue, info_ppd);

		Aux_printf("\t[PRAID1_resolve] METI EN LA COLA GENERAL DE PEDIDOS\n");

		Aux_printf("\t[PRAID1_resolve] EN LA COLA GRAL DE PEDIDOS TENGO size: %d, count: %d\n", collection_queue_size(general_petitions_queue), general_petitions_queue->elements_count);
		break;
	case TYPE_WRITE:
		Aux_printf("[PRAID1_resolve] ENTRE EN EL WRITE\n");

		sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
		Pack_set_struct(nipc, sec_nsec);
		Aux_printf("\t[PRAID1_PPD_resolve] OBTUVE EL N_SEC(%d) DEL NIPC\n", sec_nsec->n_sector);
		free(sec_nsec);

		info_ppd = (Info_PPD*) malloc(sizeof(Info_PPD));

		Aux_printf("[PRAID1_resolve] HICE EL MALLOC\n");

		info_ppd->fd = fd;
		// CORREGIDO 11-12-21 (MARTIN)
		//info_ppd->nipc = (NIPC*) malloc(sizeof(NIPC));
		//info_ppd->nipc->payload = (int8_t*) malloc(nipc->length);
		//memcpy(info_ppd->nipc, nipc, sizeof(NIPC));
		info_ppd->nipc = nipc;
		info_ppd->sync_PPD.sync_petitions_queue = NULL;
//		memcpy(info_ppd->nipc->payload, nipc->payload, nipc->length);

		sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
		Pack_set_struct(info_ppd->nipc, sec_nsec);
		Aux_printf("\t[PRAID1_PPD_resolve] OBTUVE EL N_SEC(%d) DEL NIPC DEL NODO\n", sec_nsec->n_sector);
		free(sec_nsec);

		Aux_printf("[PRAID1_resolve] VOY A METER EN LA COLA GENERAL DE PEDIDOS\n");

		Aux_printf("\t[PRAID1_resolve] EN LA COLA GRAL DE PEDIDOS TENGO: %d\n", collection_queue_size(general_petitions_queue));

		// Agregamos el pedido a la cola gral de pedidos
		collection_queue_push(general_petitions_queue, info_ppd);

		Aux_printf("\t[PRAID1_resolve] METI EN LA COLA GENERAL DE PEDIDOS\n");

		Aux_printf("\t[PRAID1_resolve] EN LA COLA GRAL DE PEDIDOS TENGO size: %d, count: %d\n", collection_queue_size(general_petitions_queue), general_petitions_queue->elements_count);
		break;
	case TYPE_END_CONECTION: /* SEGURO HAY QUE AGREGARLE COSAS XQ ACA HAY MAS CONEXIONES, NO LO QUIERO PENSAR POR AHORA XD */
		if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) == -1) {
			perror("epoll_ctl: listen_sock"); // revisar el log correspondiente
			return -1;
		}

		--total_connections;

		Aux_printf("\t[PRAID1_resolve] SAQUE EL DESCRIPTOR DEL EPOLL\n");

		//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		//Enviamos respuesta al pedido
		if (Pack_send(fd, pack) == -1) {
			log_warning(project_log, "[PRAID1_resolve]", "%s", "No se pudo enviar el End Connection");
		}

		if (total_connections == 1) {
			--total_connections;
			close(server_fd_PFS);
		}
		break;
	default:
		log_warning(project_log, "[PRAID1_resolve]", "%s", "No se pudo resolver una operacion inexistente");
		return -1;
		break;
	}
	return 0;
}

void PRAID1_inicializar_config(void) {
	FILE* fi;
	char var[255]; /* Ver si es indefinido */
	fi = fopen("ConfigPRAID1.txt", "rb");
	fscanf(fi, "%s = %s\n", var, config_praid1.socket_config_PFS.ip);
	fscanf(fi, "%s = %hu\n", var, &(config_praid1.socket_config_PFS.server_port));
	fscanf(fi, "%s = %hu\n", var, &(config_praid1.socket_config_PFS.max_connections));
	fscanf(fi, "%s = %s\n", var, config_praid1.socket_config_PPD.ip);
	fscanf(fi, "%s = %hu\n", var, &(config_praid1.socket_config_PPD.server_port));
	fscanf(fi, "%s = %hu\n", var, &(config_praid1.socket_config_PPD.max_connections));
	fscanf(fi, "%s = %d\n", var, &(config_praid1.console));
	fclose(fi);
}

int Equal_stateOK(void* info, void* info2) {
	Info_RAID* info_raid = (Info_RAID*) info;
	return (info_raid->state == STATE_OK);
}

int Equal_nsec_type(void* info, void* info2) {
	Info_PPD* info_ppd = (Info_PPD*) info;
	NsecType* nsec_type = (NsecType*) info2;

	Sector_n_sector sec_nsec;
	Pack_set_struct(info_ppd->nipc, &sec_nsec);

	return ((nsec_type->n_sec == sec_nsec.n_sector) && (nsec_type->type == info_ppd->nipc->type));
}

int Equal_nsec(void* info, void* info2) {
	uint32_t* n_sec = (uint32_t*) info;
	uint32_t* n_sec2 = (uint32_t*) info2;
	return (*n_sec == *n_sec2);
}

int Equals_disk_id(void* info, void* info2) {
	char* disk_id1 = (char*) info;
	char* disk_id2 = (char*) info2;
	return (!strcmp(disk_id1, disk_id2));
}

void PRAID1_console_print(char* s, ...) {
	if (config_praid1.console) {
		va_list args;
		va_start(args, s);
		vprintf(s, args);
		va_end(args);
	}
}
/*
 void PRAID1_console_print(char* info) {
 if (config_praid1.console) {
 printf(">> Consola PRAID1: %s\n", info);
 }
 }
 */
/* CUANDO SE CONECTA UN NUEVO PPD, SE CREA UN HILO, ESTE HILO A SU VEZ ES UN PRODUCTOR Y TIENE OTRO HILO CONSUMIDOR IGUAL QUE EL PPD NORMAL...
 * HAY 2 LISTAS, EL MAIN DEL RAID ES PRODUCTOR DE UNA LISTA FIFO, ESTA SE DISTRIBUYE A CADA UNO DE LOS HILOS DEL PPD SEGUN CORRESPONDA AL CASO...
 * LA 2DA LISTA DEL MAIN ES UN ANILLO DE PPDS CONECTADOS (ESTADO - SUBLISTA)
 * LA SUBLISTA CONTIENE LOS PEDIDOS DE ESE PPD
 * DESDE EL PUNTO DE VISTA DEL PPD TIENE UNA LISTA (SUBLISTA DEL MAIN). EL PPD ALMACENA EN UNA LISTA AUXILIAR (N_SEC - FD_PFS) QUE CARGA UN
 * NODO A MEDIDA QUE HACE UN SEND AL PPD.
 * LUEGO, CUANDO LLEGA LA RTA AL CONSUMIDOR DEL PPD, LA ASOCIA CON EL N_SEC Y ENVIA AL PFS SEGUN EL DESCRIPTOR CORRESPONDIENTE (FD_PFS). ELIMINA EL NODO
 * DE LA LISTA AUXILIAR.
 * NOTA IMPORTANTE, CADA PPD VA A TENER SU PROPIA SIGNAL, ENTONCES, SE DEBE GENERAR CUANDO SE ACEPTA LA CONEXION DE UNO NUEVO Y DEBE SER AGREGADA AL NODO DEL RING,
 * DE ESTE MODO CUANDO EL CONSUMIDOR QUIERE AGREGAR ALGO A LA SUBLISTA, MANDA UNA SENIAL A ESE SIGNAL QUE TIENE AHI ALMACENADO
 */
