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
//extern int MAX_EVENTS;
#define MAX_EVENTS 100
extern CHS_id* chs_id;
extern t_queue* general_petitions_queue;
extern t_list* PPDs_list;

int PRAID1_PPD(void* parametro) {

	char* start_sync_time;
	Aux_printf("[PRAID1_PPD] SOY EL HILO PPD, ENTRE!\n");

	Hilo_PPD* hilo_PPD = (Hilo_PPD*) parametro;
	Aux_printf("[PRAID1_PPD]                               !!!!!!!!hilo_PPD: %p\n", hilo_PPD);
	Aux_printf("[PRAID1_PPD]                               !!!!!!!!hilo_PPD->my_petitions_queue: %p\n", hilo_PPD->my_petitions_queue);

	int epoll_fd, num_fds, error;

	t_list* historic_petitions = NULL;
	t_list* petition_send = NULL;
	petition_send = collection_list_create();

	Aux_printf("[PRAID1_PPD] CREE LA LISTA DE PEDIDOS ENVIADOS\n");

	// Creamos el evento del epoll
	struct epoll_event event;
	struct epoll_event events[MAX_EVENTS];

	// Creo el descriptor de epoll
	if ((epoll_fd = epoll_create(10)) == -1) {
		log_error(project_log, "[PRAID1_PPD]", "%s", "Error al crear el epoll");
		return -1;
	}

	// Preparo el evento del PPD para luego cargarlo
	event.events = EPOLLIN;
	event.data.fd = hilo_PPD->client_fd;

	Aux_printf("[PRAID1_PPD] ME DISPONGO A METER EL FD DEL PPD\n");

	// Agrego el descriptor del PPD y su evento a epoll
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, hilo_PPD->client_fd, &event) == -1) {
		log_error(project_log, "[main]", "%s", "Error al agregar el cliente del PPD al epoll (epoll_ctl: listen_sock)");
		return -1;
	}

	// Declaramos la coleccion de seniales y el descriptor de la senial
	sigset_t mask;
	int signal_fd;
	ssize_t signal_len;

	// Inicializamos la senial
	sigemptyset(&mask);

	// Cargamos nuestra senial en la coleccion de seniales
	sigaddset(&mask, SIGUSR1);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		log_warning(project_log, "[PRAID1_PPD]", "%s", "No se pudo obtener la senial sigprocmask");
		perror("sigprocmask");
	}

	// Metemos en un descriptor la coleccion de seniales
	signal_fd = signalfd(-1, &mask, 0);

	// Preparo el evento del PPD para luego cargarlo
	event.events = EPOLLIN;
	event.data.fd = signal_fd;

	// Agrego el descriptor de la senial y su evento a epoll
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &event) == -1) {
		log_error(project_log, "[PRAID1_PPD]", "%s", "Error al agregar una senial al epoll (epoll_ctl: listen_sock)");
		return -1;
	}

	// Comienzo a monitorear los sockets tanto del PPD como la SIGNAL
	int n;

	Aux_printf("[PRAID1_PPD] COMIENZO A MONITOREAR EL PPD Y LA SENIAL\n");

	Pack* pack = NULL;
	NIPC* nipc = NULL;

	Info_PPD* info_ppd = NULL;
	Info_PPD* info_ppd_aux = NULL;

	// Inicializamos la estructura info del descriptor de seniales
	struct signalfd_siginfo fdsi;

	pthread_mutex_unlock(&hilo_PPD->ready);

	if (*(hilo_PPD->state) == STATE_SYNC) {
		historic_petitions = collection_list_create();
		pthread_t PRAID1_sync;
		start_sync_time = Aux_date_string();
		sem_t semaf;
		sem_init(&semaf, 0, 32);
		Sync_PPD sync_PPD;
		sync_PPD.sync_petitions_queue = hilo_PPD->my_petitions_queue;
		sync_PPD.semaf = &semaf;
		int n_sync_sectors = 0;
		sync_PPD.n_sync_sectors = &n_sync_sectors;
		sync_PPD.thread_id = *hilo_PPD->thread_id;
		pthread_create(&PRAID1_sync, NULL, (void*) &PRAID1_hilo_sync, &sync_PPD);
	}
	NsecType* nsec_type;

	while (1) {

		Aux_printf("[PRAID1_PPD] ESPERANDO CONEXION (%ld)\n", Aux_actual_microseconds());

		if ((num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) == -1) {
			log_error(project_log, "[PRAID1_PPD]", "%s", "Error en epoll_wait");
			return -1;
		}

		Aux_printf("[PRAID1_PPD] ALGUIEN SE QUIERE (%ld)\n", Aux_actual_microseconds());

		for (n = 0; n < num_fds; n++) {
			Aux_printf("[PRAID1_PPD] pthread_id: %lu\n", *hilo_PPD->thread_id);

			if (events[n].data.fd == signal_fd) {

				signal_len = read(signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
				if (signal_len != sizeof(struct signalfd_siginfo)) {
					log_warning(project_log, "[PRAID1_PPD]", "%s", "No se pudo leer una senial");
					perror("read");
				}

				Aux_printf("[PRAID1_PPD] primer if\n");
				Aux_printf("[PRAID1_PPD] SIGUSR1: %u     ", SIGUSR1);
				Aux_printf("[PRAID1_PPD] fdsi.ssi_signo: %u\n", fdsi.ssi_signo);

				if (fdsi.ssi_signo == SIGUSR1) {
					Aux_printf("[PRAID1_PPD] segundo if\n");

					while (!collection_queue_isEmpty(hilo_PPD->my_petitions_queue)) {
						Aux_printf("[PRAID1_PPD] dentro del while de envio ppd\n");

						// Saco de la lista de pedidos del PPD

						// MODIFICADO 13-12 los Aux_printf antes eran de info_raid y rompia
						Aux_printf("\t[PRAID1_PPD] LONGITUD DE LA COLA ANTES DE SACAR (%d)\n", collection_queue_size(hilo_PPD->my_petitions_queue));

						info_ppd = collection_queue_pop(hilo_PPD->my_petitions_queue);

						Aux_printf("\t[PRAID1_PPD] LONGITUD DE LA COLA DESPUES DE SACAR (%d)\n", collection_queue_size(hilo_PPD->my_petitions_queue));

						Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));
						// MODIFICADO 13-12 antes decia nipc solo
						Pack_set_struct(info_ppd->nipc, sec_nsec);
						Aux_printf("\t[PRAID1_PPD] OBTUVE EL N_SEC(%u) DEL NIPC DEL NODO\n", sec_nsec->n_sector);
						free(sec_nsec);

						// Envio el pedido al PPD
						pack = (Pack*) malloc(sizeof(Pack));
						Pack_serialized(info_ppd->nipc, pack);
						Aux_printf("[PRAID1_PPD] seriealizo\n");

						Aux_printf("[PRAID1_PPD] FD DEL PPD: %d\n", hilo_PPD->client_fd);

						// Enviamos respuesta al pedido 	// ¡¡¡CORREGIDO 11-12-21 (MARTIN) TESTEAR!!!
						// EVALUAMOS SI ES UN PEDIDO DE ESCRITURA DE SYNC, POR SI NO ES NECESARIO DE ENVIAR (PFS YA MANDO LA ESCRITURA)
						if ((info_ppd->nipc->type == TYPE_WRITE) && (*(hilo_PPD->state) == STATE_SYNC) && (info_ppd->fd == -1)) {
							nsec_type = (NsecType*) malloc(sizeof(NsecType));

							memcpy(&(nsec_type->n_sec), info_ppd->nipc->payload, sizeof(uint32_t));
							nsec_type->type = info_ppd->nipc->type;

							uint32_t* n_sector = NULL;
							n_sector = collection_list_find(historic_petitions, Equal_nsec, &(nsec_type->n_sec));

							if (!collection_queue_isEmpty(hilo_PPD->my_petitions_queue)) {
								info_ppd_aux = collection_queue_find(hilo_PPD->my_petitions_queue, Equal_nsec_type, nsec_type);
							}
							// Agregado 23-12
							free(nsec_type);

							if ((info_ppd_aux == NULL) && (n_sector == NULL)) {
								if (Pack_send(hilo_PPD->client_fd, pack) == -1) {
									Aux_printf("[PRAID1_PPD] error de envio a PPD\n");
									log_warning(project_log, "[PRAID1_PPD]", "%s", "No se pudo enviar respuesta de pedido al PPD");
								}

								// Meto en la lista de pedidos enviados
								collection_list_add(petition_send, info_ppd);
								Aux_printf("[PRAID1_PPD]\t (carga de petition send)\n");

							} else {
								free(info_ppd->nipc->payload);
								free(info_ppd->nipc);
								free(info_ppd);
							}

						} else {

							error = Pack_send(hilo_PPD->client_fd, pack);

							Aux_printf("[PRAID1_PPD] veo si se cae el PPD. ERROR: %d\n", error);

							if ((error == -1) || (error == 0)) { // TODO fijarse como matar al proceso RAID completo y ver que se hace con el fd del epoll
								Aux_printf("[PRAID1_PPD] se cayo el PPD\n");
								log_warning(project_log, "[PRAID1_PPD]", "%s", "No se pudo enviar respuesta de pedido al PPD"); // SE CAYO EL PPD

								Hilo_PPP_disk_fault(info_ppd, hilo_PPD, petition_send,historic_petitions);
								//pthread_kill(*hilo_PPD->thread_id, SIGKILL);
								kill(*hilo_PPD->thread_id, SIGKILL);
							}

							// Meto en la lista de pedidos enviados
							collection_list_add(petition_send, info_ppd);
							Aux_printf("[PRAID1_PPD]\t (carga de petition send)\n");
						}

						free(pack->serialized);
						free(pack);

						//TODO Corregir o eliminar
						// EN TEORIA YA ESTA ARREGLADO
						if ((info_ppd->nipc->type == TYPE_WRITE) && (*(hilo_PPD->state) == STATE_SYNC) && info_ppd->fd != -1) {

							uint32_t* n_sector = (uint32_t*) malloc(sizeof(uint32_t));
							memcpy(n_sector, info_ppd->nipc->payload, sizeof(uint32_t));

							uint32_t* n_sector2;
							n_sector2 = collection_list_find(historic_petitions, Equal_nsec, n_sector);

							if (n_sector2 == NULL) {
								collection_list_add(historic_petitions, n_sector);
							} else {
								free(n_sector);
							}
						}

					}
				}
				Aux_printf("[PRAID1_PPD] sali del while de envio recepcion de \n");
			} else {
				Aux_printf("Read unexpected signal\n"); // poner log o lo que sea
			}

			Aux_printf("Antes del prox if\n");

			if (events[n].data.fd == hilo_PPD->client_fd) {

				Aux_printf("[PRAID1_PPD] HAY UN PPD HACIENDO SEND fd: %d\n", events[n].data.fd);

				// Cargamos el pack con lo recibido
				pack = (Pack*) malloc(sizeof(Pack));

				error = Pack_recv(events[n].data.fd, pack);
				/*if ((Pack_recv(events[n].data.fd, pack)) == -1) {
				 log_error(project_log, "[PRAID1_PPD]", "%s", "Error al recibir el pedido del PPD");
				 return -1;
				 }*/
				if ((error == -1) || (error == 0)) { // TODO fijarse como matar al proceso RAID completo y ver que se hace con el fd del epoll
					Aux_printf("[PRAID1_PPD] se cayo el PPD\n");
					log_warning(project_log, "[PRAID1_PPD]", "%s", "No se pudo enviar respuesta de pedido al PPD"); // SE CAYO EL PPD

					Hilo_PPP_disk_fault(NULL, hilo_PPD, petition_send,historic_petitions); // REVISAR EL PARAMETRO NULL
					//pthread_kill(*hilo_PPD->thread_id, SIGKILL);
					kill(*hilo_PPD->thread_id, SIGKILL);
				}

				Aux_printf("[PRAID1_PPD] RECIBI DEL CLIENTE\n");

				nipc = (NIPC*) malloc(sizeof(NIPC));

				//Deserializamos el paquete
				Pack_deserialized(nipc, pack);

				free(pack->serialized);
				free(pack);

				Aux_printf("NIPC LENGH: %d, TYPE:%d, sizof uint32_t: %d\n", nipc->length, nipc->type, sizeof(uint32_t));

				Aux_printf("[PRAID1_PPD] DESERIALICE\n");
				Aux_printf("\t antes de PRAID1_PPD_resolve\n");
				Aux_printf("\t ");
				//Resolvemos el pedido y armamos el paquete NIPC en caso de ser necesario
				if ((PRAID1_PPD_resolve(nipc, petition_send, hilo_PPD, start_sync_time, historic_petitions)) == -1) {
					log_error(project_log, "[PRAID1_PPD]", "%s", "Error al resolver la operacion");
					return -1;
				}

				free(nipc->payload);
				free(nipc);

				Aux_printf("[PRAID1_PPD] RESOLVI EL PEDIDO\n");
			}
			Aux_printf("Antes del prox for\n");
		}
	}
	return 0;
}

void Hilo_PPP_disk_fault(Info_PPD* info_ppd, Hilo_PPD* hilo_PPD, t_list* petition_send, t_list* historic_petitions) {

	Aux_printf("******************** ALTO PRINTEO: VOY A REENCOLAR *******************\n");

	Info_RAID* info_raid;
	Info_PPD* info_ppd_aux;
	info_raid = collection_list_removeByClosure2(PPDs_list, Equal_thread_id, hilo_PPD->thread_id);
	Aux_printf("     PASE EL INFO_RAID\n");

	char state = STATE_OK;

	// TODO chequear la posibilidad del write repetido (fijarse que tengan la condicion REPLY) el otro problema seria que se podrian generar inconsistencias cuando se reencola, xq

	if (collection_list_find(PPDs_list, Equal_stateOK, &state) == NULL) {
		Aux_printf("--------------------------------------------------------------------------\n");

		Aux_printf("ESTOY EN LA FUNCION DE MUERTE AL RAID JAJA\n");

		Aux_printf("--------------------------------------------------------------------------\n");
		//sleep(1);
		exit(-1);
		//pid_t parental_id = getppid ();
		//kill(getpid(), SIGKILL);
	}
	Aux_printf("     PASE BUSCARS DISCOS OK\n");
	// MODIFICADO MARTIN (11-12-22)
	// CARGA LOS PEDIDOS EN EL ORDEN DE PRIORIDAD DE COMO SE MANDARON EN LA CABEZA DE LA general_petitions_queue (REVISAR!!!)
	pthread_mutex_lock(&general_petitions_queue->external_mutex);
	//if (*hilo_PPD->state == STATE_OK) {
	collection_queue_invert(hilo_PPD->my_petitions_queue);
	while (collection_queue_size(hilo_PPD->my_petitions_queue) > 0) {
		info_ppd_aux = collection_queue_pop(hilo_PPD->my_petitions_queue);
		if (info_ppd_aux->reply == REPLY) {
			collection_queue_push_head(general_petitions_queue, info_ppd_aux);
		} else {
			free(info_ppd_aux->nipc->payload);
			free(info_ppd_aux->nipc);
			free(info_ppd_aux);
		}
	}

	if (info_ppd != NULL) {
		if (info_ppd->reply == REPLY) {
			collection_queue_push_head(general_petitions_queue, info_ppd);
		} else {
			free(info_ppd->nipc->payload);
			free(info_ppd->nipc);
			free(info_ppd);
		}
	}


	while (collection_list_size(petition_send) > 0) {
		info_ppd_aux = collection_list_remove(petition_send, collection_list_size(petition_send) - 1);
		if (info_ppd_aux->reply == REPLY) {
			collection_queue_push_head(general_petitions_queue, info_ppd_aux);
		} else {
			free(info_ppd_aux->nipc->payload);
			free(info_ppd_aux->nipc);
			free(info_ppd_aux);
		}
	}
	pthread_mutex_unlock(&general_petitions_queue->external_mutex);

	// AGREGADO MARTIN 11-12-28
	if (*hilo_PPD->state == STATE_SYNC) {
		uint32_t n, count;
		uint32_t* aux;
		count = collection_list_size(historic_petitions);
		for (n = 0; n < count; n++) {
			aux = collection_list_remove(historic_petitions, 0);
			free(aux);
		}
	}

	Aux_printf("     ANTES DE LOS FREES\n");
	collection_list_removeByClosure2(chs_id->disk_id_list, Equals_disk_id, info_raid->disk_id);
	//free(delid)	REVISAR
	free(info_raid->petition_list_queue); //TIRA SEGMENTATION FAULT
	free(info_raid);

	Aux_printf("     PASE LOS FREES\n");
	//free(hilo_PPD->my_petitions_queue);
	//free(hilo_PPD->state);
	//free(hilo_PPD->thread_id);
	//free(hilo_PPD);
}

int PRAID1_PPD_resolve(NIPC* nipc, t_list* petition_send, Hilo_PPD* hilo_PPD, char* start_sync_time, t_list* historic_petitions) {
	Aux_printf("\t[PRAID1_PPD_resolve]\n");

	Info_PPD* info_ppd = NULL;

	Pack* pack = NULL;

	NsecType* nsec_type = (NsecType*) malloc(sizeof(NsecType));

	memcpy(&(nsec_type->n_sec), nipc->payload, sizeof(uint32_t));
	nsec_type->type = nipc->type;

	Aux_printf("\t[PRAID1_PPD_resolve] ENTRE AL WHILE, PETITION SEND SIZE: %d\n", collection_list_size(petition_send));

	Aux_printf("\t[PRAID1_PPD_resolve] REMOVI, PETITION SEND SIZE: %d\n", collection_list_size(petition_send));

	info_ppd = collection_list_removeByClosure2(petition_send, Equal_nsec_type, nsec_type);

	if ((info_ppd->fd == -1) && (*(hilo_PPD->state) == STATE_SYNC)) {
		Aux_printf("\t[PRAID1_PPD_resolve] voy a aumentar *info_ppd->sync_PPD.n_sync_sectors !\n");
		(*info_ppd->sync_PPD.n_sync_sectors)++;
		Aux_printf("\t[PRAID1_PPD_resolve] aumente *info_ppd->sync_PPD.n_sync_sectors !\n");

		info_ppd->sync_PPD.sync_petitions_queue = NULL;

		sem_post(info_ppd->sync_PPD.semaf);

		if ((*info_ppd->sync_PPD.n_sync_sectors) == (chs_id->cylinder * chs_id->sector * chs_id->head)) {
			PRAID1_console_print(">> Consola PRAID1: Se sincronizo un disco\n\t Inicio: %s Fin: %s\n", start_sync_time, Aux_date_string());
			uint32_t n, count;
			Info_PPD* aux;
			count = collection_list_size(historic_petitions);
			for (n = 0; n < count; n++) {
				aux = collection_list_remove(historic_petitions, 0);
				free(aux->nipc->payload);
				free(aux->nipc);
				free(aux);
			}
			*(hilo_PPD->state) = STATE_OK;
		}
	}

	if (info_ppd->sync_PPD.sync_petitions_queue != NULL) {

		Aux_printf("\t[PRAID1_PPD_resolve] ENTRE EN EL IF DE METER INFO DE SINCRO!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

		//info_ppd->nipc->type = TYPE_WRITE;

		free(info_ppd->nipc->payload);
		//free(info_ppd->nipc);

		//info_ppd->nipc = (NIPC*) malloc(sizeof(NIPC));
		Pack_init_NIPC(info_ppd->nipc, TYPE_WRITE);
		Pack_set_NIPC(info_ppd->nipc, nipc->payload, nipc->length);

		info_ppd->reply = NO_REPLY;
		collection_queue_push(info_ppd->sync_PPD.sync_petitions_queue, info_ppd);
		info_ppd->sync_PPD.sync_petitions_queue = NULL; // REVISAR
		pthread_kill(info_ppd->sync_PPD.thread_id, SIGUSR1); // ERROR DE ID!!!!!!!!!1
		Aux_printf("\t MANDE LA SENIAL\n");

	} else {
		if (info_ppd->reply == REPLY) {

			Aux_printf("\t[PRAID1_PPD_resolve]  ES UN NODO DE ENVIO\n");

			pack = (Pack*) malloc(sizeof(Pack));

			//Serializamos el paquete NIPC
			Pack_serialized(nipc, pack);

			Aux_printf("\t[PRAID1_PPD_resolve] VOY A ENVIAR RESPUESTA! (%ld)\n", Aux_actual_microseconds());

			//Enviamos respuesta al pedido
			Aux_printf("\t[PRAID1_PPD_resolve] VOY A ENVIAR A FD: %d\n", info_ppd->fd);
			if (Pack_send(info_ppd->fd, pack) == -1) {
				log_warning(project_log, "[PRAID1_PPD_resolve]", "%s", "No se pudo enviar la respuesta al pedido");
			}
			Aux_printf("\t[PRAID1_PPD_resolve] ENVIE\n");

			free(pack->serialized);
			free(pack);
			//Aux_printf("!!!!!!!!!!!!!!RESPUESTA DEL HILO PPD(%d) A UN PFS/HILO PPD (%d)!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", info_ppd->fd);
		}

		free(info_ppd->nipc->payload);
		free(info_ppd->nipc);
		free(info_ppd);

	}

	free(nsec_type);
	return 0;
}

void PRAID1_hilo_sync(void* info_sync) {
	Sync_PPD* sync_PPD = (Sync_PPD*) info_sync;

	uint32_t n_sec, max_sectors;

	max_sectors = chs_id->sector;
	max_sectors *= chs_id->cylinder;
	max_sectors *= chs_id->head;

	Aux_printf("CHS SINCRO: %d\n", max_sectors);

	for (n_sec = 0; n_sec < max_sectors; n_sec++) {
		//Aux_printf("\t PRAID1_hilo_sync: %d\n", *fd);
		//sleep(1);
		//usleep(25);
		sem_wait(sync_PPD->semaf);
		PRAID1_add_sync_petition(n_sec, *sync_PPD);
	}
}

void PRAID1_add_sync_petition(uint32_t n_sec, Sync_PPD sync_PPD) {

//usleep(0); // CAMBIAR POR VARIABLE GLOBAL CARGADA POR CONFIG

	Info_PPD* info_ppd = (Info_PPD*) malloc(sizeof(Info_PPD));
	info_ppd->nipc = (NIPC*) malloc(sizeof(NIPC));

	Pack_init_NIPC(info_ppd->nipc, TYPE_READ);
	Pack_set_NIPC(info_ppd->nipc, &n_sec, sizeof(uint32_t));

	info_ppd->fd = -1;
	info_ppd->sync_PPD.n_sync_sectors = sync_PPD.n_sync_sectors;
	info_ppd->sync_PPD.sync_petitions_queue = sync_PPD.sync_petitions_queue;
	info_ppd->sync_PPD.thread_id = sync_PPD.thread_id;
	info_ppd->sync_PPD.semaf = sync_PPD.semaf;
//Aux_printf("[PRAID1_add_sync_petition] info_ppd->fd: %d", info_ppd->fd);
//info_ppd->reply = REPLY;

	collection_queue_push(general_petitions_queue, info_ppd);
//usleep(1000000);
}

int Equal_thread_id(void* info, void* info2) {
	Info_RAID* info_raid = (Info_RAID*) info;
	pthread_t* thread_id2 = (pthread_t*) info2;
	return (info_raid->thread_id == *thread_id2);
}
