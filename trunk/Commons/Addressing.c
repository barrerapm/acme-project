#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include "Socket.h"
#include "Addressing.h"
#include "Pack.h"
#include "log.h"
#include "Type.h"
#include "Aux.h"

extern t_log* project_log;

// Recibe el type de una operacion que no requiere payload y devuelve -1 en caso de error
int Addressing_disk_operation(Connection* connections_set, sem_t* sem_cont_connections, int type) {
	int pos;
	Socket_get_connection(connections_set, &pos, sem_cont_connections);

	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));

	//Inicializamos paquete NIPC
	Pack_init_NIPC(nipc, type);

	// Serializa el paquete, lo envia, recibe la respuesta y lo deserializa
	if (Pack_process(connections_set[pos].client_fd, nipc) == -1) {
		log_warning(project_log, "[Addressing_disk_operation]1", "%s", Aux_type_error_string(nipc->type));
		return -1; // Ver tipo de error
	}

	if ((type == TYPE_HANDSHAKE) && (nipc->length > 0)) {
		log_warning(project_log, "[Addressing_disk_operation]2", "%s", "Error al conectarse con RAID ya que está inactivo");
		return -1;
	} else if (nipc->type < 0) {
		log_warning(project_log, "[Addressing_disk_operation]3", "%s", Aux_type_error_string(nipc->type));
		return -1;
	}

	log_info(project_log, "[Addressing_disk_operation]4", "%s", Aux_type_string(nipc->type));

	free(nipc->payload);
	free(nipc);

	if (type != TYPE_END_CONECTION) {
		Socket_release_connection(connections_set, pos, sem_cont_connections);
	}

	return 0;
}
