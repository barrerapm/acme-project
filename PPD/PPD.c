#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include "PPD.h"
#include "Pack.h"
#include "Socket.h"
#include "Struct.h"
#include "log.h"
#include "Type.h"
#include "Addressing.h"
#include "algoritmos.h"
#include "Aux.h"
#include <math.h>

#define MAX_EVENTS 100 /* LO CONSIDERO UN NRO LO SUFICIENTEMENTE GRANDE, NO SOBREPASA LA CANTIDAD DE CONEXIONES */

ConfigPPD config_ppd; // Estructura global config_ppd
int disk_fd;
void *disk_map;
t_log* project_log;
int n_writes = 0;
int every_n_writes;

// Creamos el  estado del disco
Sector_Fis* estado;

// STEPS para el algoritmo
uint32_t steps, MAX_STEPS;

// Creamos el mutex para sincronizar el acceso a la cola y para dormir al consumidor globales
pthread_mutex_t mutex_cola = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_cont_consumidor;

// Creamos la cabeza y la cola globales
Nodo* head = NULL;
Nodo* tail = NULL;

// Creamos el contador de conexiones
int total_connections;

// Creamos los descriptores del server y del epoll
int server_fd, epoll_fd, server_console_fd; // Creamos el evento del epoll
struct epoll_event event;
int max_events;

int main(void) {
	project_log = log_create("Runner", "log.txt", DEBUG | INFO | WARNING, M_CONSOLE_ENABLE);

	// Abrimos el disco y lo mapeamos en memoria
	if (PPD_open_disk() == -1) {
		PPD_log_print("error", "[main]", "Error al abrir el disco");
		return -1;
	}

	max_events = 100; //config_ppd.socket_config_PFS.max_connections + config_ppd.socket_config_RAID.max_connections;

	Aux_printf("SE ABRIO EL DISCO\n");

	struct epoll_event events[max_events + 1];

int 	num_fds;

	int client_fd;

	// Creo el descriptor de epoll
	if ((epoll_fd = epoll_create(10)) == -1) {
		PPD_log_print("error", "[main]", "Error al crear el epoll");
		return -1;
	}

	server_console_fd = Socket_open_server_unix(config_ppd.path_unix);

	//Preparo el evento del server para luego cargarlo
	event.events = EPOLLIN;
	event.data.fd = server_console_fd;

	// Agrego el descriptor del server y su evento a epoll
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_console_fd, &event) == -1) {
		PPD_log_print("error", "[main]", "Error al agregar al epoll el server de la consola (epoll_ctl: listen_sock)");
		return -1;
	}
	++total_connections;
	if (!strcmp(config_ppd.modo_inicio, "LISTEN")) {
		pid_t process_id;
		char* parameters[] = { "ConsolePPD", config_ppd.path_unix, NULL };

		process_id = fork();

		switch (process_id) {
		case -1:
			/* Error al crear el proceso hijo */
			printf("Error al crear el proceso hijo\n");
			break;
		case 0:
			/* Poceso hijo */
			if (execv(config_ppd.console_path, parameters) == -1) {
				perror("La cague en el execv");
				return EXIT_FAILURE;
			}
			break;
		default:
			/* Proceso padre */
			printf("La id del proceso hijo es: %d\n", process_id);
			break;
		}

	}
	sem_init(&sem_cont_consumidor, 0, 0);

	steps = MAX_STEPS = 128;

	estado = (Sector_Fis*) malloc(sizeof(Sector_Fis));

	// Inicializamos el estado del disco
	estado->cylinder = 0;
	estado->sector = 0;

	/* EN ESTA PARTE SE INICIA LA CONEXION DEL PPD SEGUN LA CONFIG APROPIADA COMO CLIENTE O COMO SERVIDOR */

	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));
	Pack* pack = (Pack*) malloc(sizeof(Pack));

	// NO estoy en modo RAID
	if (!strcmp(config_ppd.modo_inicio, "LISTEN")) {
		// Establecemos la conexion con el PFS
		if ((server_fd = Socket_open_server_inet(config_ppd.socket_config_PFS)) == -1) {
			PPD_log_print("error", "[main]", "Error al abrir el server inet del PFS");
			return -1;
		}
		Aux_printf("server_fd: %d\n", server_fd);

		//Preparo el evento del server para luego cargarlo
		event.events = EPOLLIN;
		event.data.fd = server_fd;

		// Agrego el descriptor del server y su evento a epoll
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
			PPD_log_print("error", "[main]", "Error al agregar al epoll el server del PFS (epoll_ctl: listen_sock)");
			return -1;
		}
		++total_connections;

		pthread_t consumidor;
		pthread_create(&consumidor, NULL, (void*) &PPD_hilo_consumidor, NULL);

		int n;

		while (1) {

			Aux_printf("[P] ESPERANDO CONEXION (%ld)\n", Aux_actual_microseconds());

			if ((num_fds = epoll_wait(epoll_fd, events, max_events + 1, -1)) == -1) {
				PPD_log_print("warning", "[main]", "No se pudo hacer epoll_wait");
				continue;
			}

			Aux_printf("[P] ALGUIEN SE QUIERE CONECTAR (%ld)\n", Aux_actual_microseconds());

			for (n = 0; n < num_fds; n++) {

				if (events[n].data.fd == server_console_fd) {
					if ((client_fd = Socket_accept_client(server_console_fd)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo aceptar el cliente de la consola");
						continue;
					}

					Aux_printf("[P] ESTAMOS CONECTADOS\n");

					event.events = EPOLLIN;
					event.data.fd = client_fd;

					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo agregar al epoll el server del PFS (epoll_ctl: listen_sock)");
						continue;
					}
					++total_connections;

					Aux_printf("[P] AGREGUE LA NUEVA CONEXION AL EPOLL\n");

				} else if (events[n].data.fd == server_fd) {
					if ((client_fd = Socket_accept_client(server_fd)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo aceptar el cliente del PFS");
						continue;
					}

					Aux_printf("[P] ESTAMOS CONECTADOS\n");

					event.events = EPOLLIN;
					event.data.fd = client_fd;

					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo agregar al epoll el server del PFS (epoll_ctl: listen_sock)");
						continue;
					}
					++total_connections;

					Aux_printf("[P] AGREGUE LA NUEVA CONEXION AL EPOLL\n");
				} else {

					Aux_printf("[P] HAY UN CLIENTE HACIENDO SEND fd: %d\n", events[n].data.fd);

					// Cargamos el pack con lo recibido
					if ((Pack_recv(events[n].data.fd, pack)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo recibir el pedido del PFS");
						exit(-1);
					}

					Aux_printf("[P] RECIBI DEL CLIENTE\n");

					//Deserializamos el paquete
					Pack_deserialized(nipc, pack);

					Aux_printf("[P] DESERIALICE\n");

					//Resolvemos el pedido y armamos el paquete NIPC en caso de ser necesario
					if ((PPD_resolve(events[n].data.fd, nipc, pack)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo resolver la operacion del PFS");
						continue;
					}

					Aux_printf("[P] RESOLVI EL PEDIDO\n");

				}
			}

			if (total_connections == 0)
				break;
		}
	}

	// Estoy en modo RAID
	if (!strcmp(config_ppd.modo_inicio, "CONNECT")) {
		Aux_printf("ESTOY EN MODO RAID\n");

		// Establecemos la conexion con el PRAID1
		//Abrimos el socket cliente
		if ((client_fd = Socket_open_client_inet(config_ppd.socket_config_RAID, 1)) == -1) {
			PPD_log_print("error", "[main]", "Error al abrir el cliente inet del RAID");
			return -1;
		}

		Aux_printf("client_fd: %d\n", client_fd);

		Aux_printf("ABRI EL SOCKET CLIENTE\n");

		// Realizamos la conexion con el server
		if (Socket_connect_to_server_inet(config_ppd.socket_config_RAID, client_fd) == -1) {
			PPD_log_print("error", "[main]", "Error al conectar con el server inet del RAID");
			return -1;
		}

		Aux_printf("ME CONECTE CON EL PRAID\n");

		// Handshake
		//Inicializamos paquete NIPC
		Pack_init_NIPC(nipc, TYPE_HANDSHAKE);

		PPD_CHS_id* PPD_chs_id = (PPD_CHS_id*) malloc(sizeof(PPD_CHS_id));

		PPD_chs_id->head = config_ppd.heads;
		PPD_chs_id->cylinder = config_ppd.cylinders;
		PPD_chs_id->sector = config_ppd.sectors_per_track;
		memcpy(&(PPD_chs_id->disk_id), &(config_ppd.ID_Disco), strlen(config_ppd.ID_Disco) + 1);

		Pack_set_NIPC(nipc, PPD_chs_id, sizeof(PPD_CHS_id));

		free(PPD_chs_id);

		Aux_printf("\t NIPC: type: %d    lenght: %d    payload: %d\n", nipc->type, nipc->length, *nipc->payload);

		//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		Aux_printf("voy a send (%ld)\n", Aux_actual_microseconds());
		//Enviamos pedido
		if (Pack_send(client_fd, pack) == -1) {
			PPD_log_print("error", "[main]", "Error al enviar Handshake con CHS al RAID");
			return -1;
		}

		free(nipc->payload);

		// Resetea el pack para volverlo a cargar
		Pack_reload_pack(pack);

		// Recibimos respuesta
		if ((Pack_recv(client_fd, pack)) == -1) {
			PPD_log_print("error", "[main]", "Error al recibir Handshake de CHS del RAID");
			return -1;
		}

		//Deserializamos el paquete NIPC
		Pack_deserialized(nipc, pack);

		Pack_reload_pack(pack);

		Aux_printf("nipc cargado type: %d length: %d\n", nipc->type, nipc->length);

		// EVALUAR SI ES UN DISCO VALIDO O NO
		if (nipc->length != 0) {
			PPD_log_print("error", "[main]", "Error al conectarse con RAID, CHS invalido o ID inexistente");
			close(client_fd);
			free(nipc->payload);
			free(nipc);
			free(pack);
			free(estado);
			PPD_close_disk();
			return -1;
		}
		free(nipc->payload);

		//Preparo el evento del cliente para luego cargarlo
		event.events = EPOLLIN;
		event.data.fd = client_fd;

		// Agrego el descriptor del cliente y su evento a epoll
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
			PPD_log_print("error", "[main]", "No se pudo agregar al epoll el cliente del RAID (epoll_ctl: listen_sock)");
			return -1;
		}
		++total_connections;

		pthread_t consumidor;
		pthread_create(&consumidor, NULL, (void*) &PPD_hilo_consumidor, NULL);

		int n;

		while (1) {

			Aux_printf("[P] ESPERANDO CONEXION (%ld)\n", Aux_actual_microseconds());

			if ((num_fds = epoll_wait(epoll_fd, events, max_events, -1)) == -1) {
				PPD_log_print("warning", "[main]", "No se pudo hacer epoll_wait");
				continue;
			}

			Aux_printf("[P] ALGUIEN SE QUIERE CONECTAR (%ld)\n", Aux_actual_microseconds());

			for (n = 0; n < num_fds; n++) {
				if (events[n].data.fd == server_console_fd) {

					if ((client_fd = Socket_accept_client(server_console_fd)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo aceptar el cliente de la consola");
						continue;
					}

					Aux_printf("[P] ESTAMOS CONECTADOS\n");

					event.events = EPOLLIN;
					event.data.fd = client_fd;

					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo agregar al epoll el server del PFS (epoll_ctl: listen_sock)");
						continue;
					}
					++total_connections;

					Aux_printf("[P] AGREGUE LA NUEVA CONEXION AL EPOLL\n");

				} else if (events[n].data.fd == client_fd) {

					Aux_printf("[P] HAY UN SERVER HACIENDO SEND fd: %d\n", events[n].data.fd);

					// Cargamos el pack con lo recibido
					if ((Pack_recv(events[n].data.fd, pack)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo recibir el pedido del RAID");
						exit(-1);
					}

					Aux_printf("[P] RECIBI DEL SERVER\n");

					//Deserializamos el paquete
					Pack_deserialized(nipc, pack);

					Aux_printf("[P] DESERIALICE\n");

					//Resolvemos el pedido y armamos el paquete NIPC en caso de ser necesario
					if ((PPD_resolve(events[n].data.fd, nipc, pack)) == -1) {
						PPD_log_print("warning", "[main]", "No se pudo resolver el pedido del RAID");
						continue;
					}

					Aux_printf("[P] RESOLVI EL PEDIDO\n");

					Pack_reload_pack(pack);
					free(nipc->payload);

				}
			}

			if (total_connections == 0)
				break;
		}
	}

// Cerramos el disco, xq no tenemos mas conexiones
	if (PPD_close_disk() == -1) {
		PPD_log_print("error", "[main]", "Error al cerrar el disco");
	}

	print_config_ppd();
	Aux_printf("ME CERRE, TODO PIOLA :D\n");

	Pack_destroy(nipc, pack);

	free(nipc->payload);
	free(nipc);
	free(pack->serialized);
	free(pack);
	free(estado);
	PPD_close_disk();

	return 0;
}

int PPD_hilo_consumidor() {

	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));
	Pack* pack = (Pack*) malloc(sizeof(Pack));
	Info* info = NULL;
	Info* prox; // = NULL;

	while (1) {
		prox = (Info*) malloc(sizeof(Info));
		info = (Info*) malloc(sizeof(Info));
		Aux_printf("[C] SOY EL HILO, ENTRE! (%ld)\n", Aux_actual_microseconds());

		Aux_printf("[C] ANTES DEL sem_wait\n  sem_cont: %ld\n", sem_cont_consumidor.__align);
		sem_wait(&sem_cont_consumidor);
		Aux_printf("[C] DESPUES DEL sem_wait\n  sem_cont: %ld\n", sem_cont_consumidor.__align);

		Aux_printf("[C] ME DISPONGO A SACAR DE LA COLA (%ld)\n", Aux_actual_microseconds());

		PPD_queue_pop(info, prox);

		Aux_printf("[C] SALI DE SACAR QUEUE!\n");

		Aux_printf("[C] ME DISPONGO A RESOLVER LA LECTURA/ESCRITURA!\n");

		if (info->type == TYPE_READ) {

			Aux_printf("[C] ME DISPONGO A RESOLVER LA LECTURA!\n");

			if (PPD_solve_read(info, nipc) == -1) {
				PPD_log_print("warning", "[PPD_hilo_consumidor]", "Error al resolver una lectura");
				nipc->type = TYPE_ERROR_READ;

				Aux_printf("[C] ERROR\n");
			}
		} else {
			if (info->type == TYPE_WRITE) {

				if (PPD_solve_write(info, nipc) == -1) {
					PPD_log_print("warning", "[PPD_hilo_consumidor]", "Error al resolver una escritura");
					nipc->type = TYPE_ERROR_WRITE;
				}
			} else {
				PPD_solve_trace(info, prox, nipc);
			}
		}

		free(prox);
		Aux_printf("[C] VOY A SERIALIZAR!\n");

//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		Aux_printf("[C] VOY A ENVIAR RESPUESTA! (%ld)\n", Aux_actual_microseconds());

//Enviamos respuesta al pedido
		if (Pack_send(info->fd, pack) == -1) {
			PPD_log_print("warning", "[PPD_hilo_consumidor]", "Error al enviar una respuesta de pedido");
		}
		Pack_reload_pack(pack);
		free(nipc->payload);
		free(info);
	}
	return 0;
}

void PPD_queue_pop(Info* info, Info* prox) {
	Info* aux_prox = NULL;
	Aux_printf("[C] HOLA, SOY SACAR!\n");

	if (strcmp(config_ppd.algoritmo, "SSTF") == 0) {

		Aux_printf("[C] VOY A USAR EL ALGORITMO SSTF\n");

		pthread_mutex_lock(&mutex_cola);
		aux_prox = sstf(info);
		pthread_mutex_unlock(&mutex_cola);

		if (aux_prox != NULL) {
			memcpy(prox, aux_prox, sizeof(Info));
		} else {
			prox->sector_fis.cylinder = 0;
			prox->sector_fis.sector = 65535;
		}

		Aux_printf("[C] ME FUI DE SSTF, SAQUE FD: %d\n", info->fd);
		Aux_printf("[C] ME FUI DE SSTF, aUsar    cyl: %d  sec: %d\n", info->sector_fis.cylinder, info->sector_fis.sector);
		if (prox == NULL) {
			Aux_printf("[C] ME FUI DE SSTF, Prox  NO HAY PROX\n");
		} else {
			Aux_printf("[C] ME FUI DE SSTF, Prox    cyl: %d  sec: %d\n", prox->sector_fis.cylinder, prox->sector_fis.sector);
		}
	}

	if (strcmp(config_ppd.algoritmo, "SSTF_NSTEPS") == 0) {
		pthread_mutex_lock(&mutex_cola);
		aux_prox = sstf_nSteps(info);
		pthread_mutex_unlock(&mutex_cola);

		if (aux_prox != NULL) {
			memcpy(prox, aux_prox, sizeof(Info));
		} else {
			prox->sector_fis.cylinder = 0;
			prox->sector_fis.sector = 65535;
		}
		Aux_printf("[C] ME FUI DE SSTF_NSTEPS, SAQUE FD: %d\n", info->fd);
		Aux_printf("[C] ME FUI DE SSTF_NSTEPS, aUsar    cyl: %d  sec: %d\n", info->sector_fis.cylinder, info->sector_fis.sector);
		if (prox == NULL) {
			Aux_printf("[C] ME FUI DE SSTF_NSTEPS, Prox  NO HAY PROX\n");
		} else {
			Aux_printf("[C] ME FUI DE SSTF_NSTEPS, Prox    cyl: %d  sec: %d\n", prox->sector_fis.cylinder, prox->sector_fis.sector);
		}
	}
}

void PPD_queue_push(int fd, NIPC* nipc) {
	Aux_printf("[P] ENTRE EN QUEUE PUSH\n");
	uint32_t* n_sector = (uint32_t*) malloc(sizeof(uint32_t));

// Mete en la cola
	Nodo* p = (Nodo*) malloc(sizeof(Nodo));
	p->info = (Info*) malloc(sizeof(Info));
	p->info->type = nipc->type;
	p->info->fd = fd;
	p->sgte = NULL;

	Aux_printf("[P] VOY A HACER EL MEMCPY\n");

	memcpy(n_sector, nipc->payload, sizeof(*n_sector));

	Aux_printf("[P] VOY A PASAR A FISICO\n");

	sector_log_fis(*n_sector, &(p->info->sector_fis));

	if (nipc->type == TYPE_WRITE) {
		Aux_printf("WRITE CARGO SECTOR EN P -------------------\n");
		memcpy(&(p->info->sector), nipc->payload + sizeof(uint32_t), sizeof(Sector));
	}

	Aux_printf("[P] ESTOY CERCA DE METERME EN LA COLA\n");

//Bloquea la cola y carga el nodo
	pthread_mutex_lock(&mutex_cola);
	if (head == NULL) {
		head = p;
		tail = head;
	} else {
		tail->sgte = p;
		tail = p;
	}
	sem_post(&sem_cont_consumidor);

	pthread_mutex_unlock(&mutex_cola);

	Aux_printf("[P] AUMENTE EL CONTADOR DE LA COLA\n  sem_cont: %ld\n", sem_cont_consumidor.__align);

	free(n_sector);

	Aux_printf("[P] METI EN LA COLA\n");

}

// Recibe paquete NIPC, realiza lo que se requiere y duelve el paquete serializado listo para enviar y retorna -1 si el type es desconocido
int PPD_resolve(int fd, NIPC* nipc, Pack* pack) {
	switch (nipc->type) {
	case TYPE_HANDSHAKE:
		/* NO SE Q PIJA HABRIA Q HACER EN CASO DE ERROR XD */
		//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		//Enviamos respuesta al pedido
		if (Pack_send(fd, pack) == -1) {
			PPD_log_print("warning", "[PPD_resolve]", "No se pudo enviar una respuesta de pedido");
			return -1;
		}
		break;
	case TYPE_READ: //Lee el sector pedido
		PPD_queue_push(fd, nipc);
		break;
	case TYPE_WRITE: //Escribir el sector pedido
		PPD_queue_push(fd, nipc);
		break;
	case 11: //Trace
		PPD_queue_push(fd, nipc);
		break;
	case TYPE_STATE:
		Pack_set_NIPC(nipc, estado, sizeof(Sector_Fis));

		//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		//Enviamos respuesta al pedido
		if (Pack_send(fd, pack) == -1) {
			PPD_log_print("warning", "[PPD_resolve]", "No se pudo enviar una respuesta de pedido");
			return -1;
		}
		break;
	case TYPE_END_CONECTION: //Cerrar socket servidor
		if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) == -1) {
			PPD_log_print("warning", "[PPD_resolve]", "No se pudo agregar al epoll el evento (epoll_ctl: listen_sock)");
			return -1;
		}

		--total_connections;

		Aux_printf("[P] SAQUE EL DESCRIPTOR DEL EPOLL\n");

		//Serializamos el paquete NIPC
		Pack_serialized(nipc, pack);

		//Enviamos respuesta al pedido
		if (Pack_send(fd, pack) == -1) {
			PPD_log_print("warning", "[PPD_resolve]", "No se pudo enviar una respuesta de pedido");
			return -1;
		}

		if (total_connections == 1) {
			--total_connections;
			close(server_fd);
		}
		break;
	default:
		PPD_log_print("warning", "[PPD_resolve]", "No se pudo resolver una operacion inexistente");
		return -1;
		break;
	}
	return 0;
}

int PPD_solve_read(Info* info, NIPC* nipc) {

	Aux_printf("HOLA, SOY SACAR! VOY A HACER MALLOC sec_nsec y te aviso si lo hice\n");

	Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));

	Aux_printf("HICE MALLOC sec_nsec\n");

	sector_fis_log(&(info->sector_fis), &(sec_nsec->n_sector));

	// ACTUALIZAMOS EL ESTADO DEL DISCO
	estado->cylinder = info->sector_fis.cylinder;
	estado->sector = info->sector_fis.sector;

	Aux_printf("Voy a leer el sector: %d\n", sec_nsec->n_sector);
	PPD_read_sector(&(sec_nsec->sector), sec_nsec->n_sector);
	Aux_printf("Lei el sector: %d\n", sec_nsec->n_sector);

	nipc->type = info->type;

	Pack_set_NIPC(nipc, sec_nsec, sizeof(Sector_n_sector));

	Aux_printf("CARGADO NIPC CON TYPE: %d y su Length: %d\n", nipc->type, nipc->length);

	free(sec_nsec);
	return 0;
}

int PPD_solve_write(Info* info, NIPC* nipc) {
	uint32_t n_sector;

	sector_fis_log(&(info->sector_fis), &(n_sector));

	// ACTUALIZAMOS EL ESTADO DEL DISCO
	estado->cylinder = info->sector_fis.cylinder;
	estado->sector = info->sector_fis.sector;

	Aux_printf("Voy a escribir el sector: %d\n", n_sector);
	PPD_write_sector(&(info->sector), n_sector);
	Aux_printf("Escribi el sector: %d\n", n_sector);

	nipc->type = info->type;

	// Agrega el numero de sector al payload
	Pack_set_NIPC(nipc, &n_sector, sizeof(uint32_t));

	Aux_printf("CARGADO NIPC CON TYPE: %d y su Length: %d\n", nipc->type, nipc->length);

	return 0;
}

int PPD_solve_trace(Info* info, Info* prox, NIPC* nipc) { // HAY Q REVISAR ESTO....

	Info_Trace info_trace;
	info_trace.estado.cylinder = estado->cylinder;
	info_trace.estado.sector = estado->sector;

	info_trace.destino.cylinder = info->sector_fis.cylinder;
	info_trace.destino.sector = info->sector_fis.sector;

	info_trace.proximo.cylinder = prox->sector_fis.cylinder;
	info_trace.proximo.sector = prox->sector_fis.sector;

	info_trace.sectors_per_track = config_ppd.sectors_per_track;
	info_trace.latency = latency_time(&info->sector_fis);

	nipc->type = info->type;
	Pack_set_NIPC(nipc, &info_trace, sizeof(Info_Trace));

	usleep(config_ppd.T_Lectura);
	return 0;
}

void PPD_inicializar_config(void) {
	FILE* fi;
	char var[255]; /* Ver si es indefinido */
	fi = fopen("ConfigPPD.txt", "rb");

	fscanf(fi, "%s = %s\n", var, config_ppd.path); //OK
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.bytes_per_sector)); //OK
	fscanf(fi, "%s = %hhu\n", var, &(config_ppd.heads));
	fscanf(fi, "%s = %du\n", var, &(config_ppd.cylinders));
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.sectors_per_track));
	fscanf(fi, "%s = %s\n", var, (config_ppd.algoritmo));
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.RPM));
	fscanf(fi, "%s = %hhu\n", var, &(config_ppd.T_Lectura));
	fscanf(fi, "%s = %hhu\n", var, &(config_ppd.T_Escritura));
	fscanf(fi, "%s = %hhu\n", var, &(config_ppd.T_Seek));
	fscanf(fi, "%s = %s\n", var, config_ppd.modo_inicio);
	fscanf(fi, "%s = %s\n", var, config_ppd.path_unix);
	fscanf(fi, "%s = %s\n", var, config_ppd.ID_Disco);
	fscanf(fi, "%s = %hhu\n", var, &(config_ppd.LOG));
	fscanf(fi, "%s = %u\n", var, &(config_ppd.Retardo_R));
	fscanf(fi, "%s = %u\n", var, &(config_ppd.Retardo_W));
	fscanf(fi, "%s = %s\n", var, config_ppd.socket_config_RAID.ip);
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.socket_config_RAID.server_port));
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.socket_config_RAID.max_connections));
	fscanf(fi, "%s = %s\n", var, config_ppd.socket_config_PFS.ip);
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.socket_config_PFS.server_port));
	fscanf(fi, "%s = %hu\n", var, &(config_ppd.socket_config_PFS.max_connections));
	fscanf(fi, "%s = %s\n", var, config_ppd.console_path);
	fclose(fi);
	print_config_ppd();

	every_n_writes = floor(0.001 * (config_ppd.cylinders * config_ppd.sectors_per_track));
	printf("[PPD_inicializar_config] Se realizara el msync cada %d escrituras\n", every_n_writes);
	config_ppd.Sectors_ms = ((config_ppd.RPM * config_ppd.sectors_per_track) / 60000.0);
}

int PPD_open_disk() {
	PPD_inicializar_config();
	if ((disk_fd = open(config_ppd.path, O_RDWR)) == -1) {
		perror("Error disco\n");
		return -1;
	}

	struct stat file_stat;
	if (fstat(disk_fd, &file_stat) < 0) {
		perror("Error disco\n");
		PPD_log_print("warning", "[PPD_open_disk]", "Error al leer las estadisticas del disco");
		return -1;
	}

	if ((disk_map = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0)) == MAP_FAILED) {
		PPD_log_print("warning", "[PPD_open_disk]", "Error al mapear el disco en memoria");
		return -1;
	} else {
		PPD_log_print("info", "[PPD_open_disk]", "Disco mapeado en memoria");
	}

	// Ayuda a manejar el acceso a memoria del disco mapeado
	if (madvise(disk_map, file_stat.st_size, MADV_SEQUENTIAL) != 0) {
		PPD_log_print("warning", "[PPD_open_disk]", "Fallo la optimizacion del mapeo, pero sigue funcionando");
		Aux_printf("Fallo la optimizacion del mapeo, pero sigue funcionando"); /* NO SE SI HAY QUE AGREGAR UN LOG O ALGO POR EL ESTILO */
	}

	return disk_fd;
}

// Leer sector: Recibe el nro de sector y devuelve el sector
int32_t PPD_read_sector(Sector* sector, int n_sector) {
	inc_estado_RW(config_ppd.T_Lectura); // AGREGADO MARTIN 11-11-30  *incrementa el estado segun el tiempo de lectura
	usleep(config_ppd.T_Lectura * 1000);

	sleep(config_ppd.Retardo_R);
	memcpy(sector, disk_map + n_sector * config_ppd.bytes_per_sector, sizeof(Sector));
	return 0;

}

// Escribir sector: Recibe nro de sector y sector
int32_t PPD_write_sector(Sector* sector, int n_sector) {
	inc_estado_RW(config_ppd.T_Escritura); // AGREGADO MARTIN 11-11-30  *incrementa el estado segun el tiempo de escritura
	usleep(config_ppd.T_Escritura * 1000);

	sleep(config_ppd.Retardo_W);

	memcpy(disk_map + n_sector * config_ppd.bytes_per_sector, sector, sizeof(Sector));

	if ((++n_writes % every_n_writes) == 0) {
		struct stat file_stat;
		if (fstat(disk_fd, &file_stat) < 0) {
			PPD_log_print("warning", "[PPD_write_sector]", "Error al leer las estadisticas del disco");
			return -1;
		}
		msync(disk_map, file_stat.st_size, MS_ASYNC);
	}

	return 0;

}

int32_t PPD_close_disk(void) {
	if (munmap(disk_map, strlen(disk_map)) == -1) {
		PPD_log_print("warning", "[PPD_close_disk]", "Error al desmapear el disco en memoria");
	}
	return close(disk_fd);
}

// Prints para pruebas
void print_config_ppd(void) {
	Aux_printf("______________________________________________________________________\n");
	Aux_printf("\n");
	Aux_printf("> Config PPD: \n");
	Aux_printf("\t» PATH: %s \n", config_ppd.path);
	Aux_printf("\t» BYTES_PER_SECTOR: %hu\n", config_ppd.bytes_per_sector);
	Aux_printf("\t» CHS: (%hhu-", config_ppd.heads);
	Aux_printf("%u-", config_ppd.cylinders);
	Aux_printf("%hu)\n", config_ppd.sectors_per_track);
	Aux_printf("\t» RPM: %hu\n", config_ppd.RPM);
	Aux_printf("\t» SEEK_TIME: %hhu\n", config_ppd.T_Seek);
	Aux_printf("\t» READ_TIME: %hhu\n", config_ppd.T_Lectura);
	Aux_printf("\t» WRITE_TIME: %hhu\n", config_ppd.T_Escritura);
	Aux_printf("\t» ALGORITMO: %s\n", config_ppd.algoritmo);
	Aux_printf("\t» MODO_INICIO: %s\n", config_ppd.modo_inicio);
	Aux_printf("\t» path_unix: %s\n", config_ppd.path_unix);
	Aux_printf("\t» ID_Disco: %s\n", config_ppd.ID_Disco);
	Aux_printf("\t» LOG: %d\n", config_ppd.LOG);
	Aux_printf("\t» Retardo_R: %d\n", config_ppd.Retardo_R);
	Aux_printf("\t» Retardo_W: %d\n", config_ppd.Retardo_W);
	Aux_printf("\t» socket_config_RAID ip: %s\n", config_ppd.socket_config_RAID.ip);
	Aux_printf("\t» socket_config_RAID serp: %d\n", config_ppd.socket_config_RAID.server_port);
	Aux_printf("\t» socket_config_RAID maxc: %d\n", config_ppd.socket_config_RAID.max_connections);
	Aux_printf("\t» socket_config_PFS ip: %s\n", config_ppd.socket_config_PFS.ip);
	Aux_printf("\t» socket_config_PFS serp: %d\n", config_ppd.socket_config_PFS.server_port);
	Aux_printf("\t» socket_config_PFS maxc: %d\n", config_ppd.socket_config_PFS.max_connections);
	Aux_printf(" ______________________________________________________________________\n");
	Aux_printf("\n");
}

void sector_log_fis(uint32_t n_sector, Sector_Fis* sector_fis) {

	Aux_printf("VOY A CONVERTILO\n");

	Aux_printf("resultado cilindro: %u\n", ((n_sector) / config_ppd.sectors_per_track));

	sector_fis->cylinder = ((n_sector) / config_ppd.sectors_per_track);

	Aux_printf("VOY POR LA SEGUNDA CONVERSION\n");

	Aux_printf("resultado sector: %u\n", (n_sector % config_ppd.sectors_per_track));

	sector_fis->sector = (n_sector % config_ppd.sectors_per_track);
}

void sector_fis_log(Sector_Fis* sector_fis, uint32_t* n_sector) {
	*n_sector = ((sector_fis->cylinder * config_ppd.sectors_per_track) + sector_fis->sector);
	Aux_printf("sector despues de convertirlo: %u \n", *n_sector);
}

void inc_estado_RW(uint8_t T_RW) { // AGREGADO MARTIN 11-11-30

	int sectors = ceil(T_RW * config_ppd.Sectors_ms);

	Aux_printf("INC_ESTADO_RW T_RW: %d   Sectors_ms: %f   cant_sectores: %d     estado->sector: %d\n", T_RW, config_ppd.Sectors_ms, sectors, estado->sector);

	for (; sectors > 0; sectors--) {
		inc_estado();
	}
	Aux_printf("INC_ESTADO_RW   estado->sector: %d\n", estado->sector);
}

void inc_estado(void) {
	if (estado->sector == (config_ppd.sectors_per_track - 1)) {
		estado->sector = 0;
	} else
		estado->sector++;
}

double latency_time(Sector_Fis* destino) { // ARREGLADO MARTIN 11-12-28

	double latencia;

	if (estado->cylinder != destino->cylinder) {
		latencia = (abs(estado->cylinder - destino->cylinder) * config_ppd.T_Seek);
	}

	if (destino->sector < estado->sector) {
		latencia += (destino->sector + (config_ppd.sectors_per_track - estado->sector)) * (1.0 / config_ppd.Sectors_ms);
	} else {
		latencia += (destino->sector - estado->sector) * (1.0 / config_ppd.Sectors_ms);
	}

	estado->cylinder = destino->cylinder;
	estado->sector = destino->sector;

	return (latencia);

}

void PPD_log_print(char* log_type, char* thread, char* info) {
	if (config_ppd.LOG) {
		if (!strcmp(log_type, "error"))
			log_error(project_log, thread, "%s", info);
		else if (!strcmp(log_type, "warning"))
			log_warning(project_log, thread, "%s", info);
		else if (!strcmp(log_type, "info"))
			log_warning(project_log, thread, "%s", info);
	}
}
