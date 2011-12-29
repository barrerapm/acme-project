#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ConsolePPD.h"
#include "Struct.h"
#include "Aux.h"
#include "Socket.h"
#include "Pack.h"
#include "Type.h"
#include "log.h"

t_log* project_log;

int n_commands = 5;
char* commands[] = { "info", "clean", "trace", "help", "exit" };
char* help_commands[] = { "info", "clean start_sector end_sector", "trace n_sector1 [n_sector2|n_sector3|n_sector4|n_sector5]", "help", "exit" };
int client_fd;

int main(int argc, char* argv[]) {

	printf("Argc: %d, Argv 1:%s \n", argc, argv[1]); /* MODIFICACION 26-12-11 */

	if (Socket_init_client_unix(&client_fd, argv[1]) == -1) {
		printf("error al conectar conectar con el servidor\n"); //log correspondiente
	}

	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));

	//HANDSHAKE
	Pack_init_NIPC(nipc, TYPE_HANDSHAKE);

	if (Pack_process(client_fd, nipc) == -1) {
		//log
		return -1;
	}

	/* EVALUAR EL ERROR DEL HANDSHAKE */

	int (*functions[])(int, char**) = {
		Console_info, Console_clean, Console_trace, Console_help, Console_exit};

	while (1) {
		printf(">> Ingrese el nombre del comando seguido de los parametros\n");
		/*printf(" · Console_info: \t1\n");
		 printf(" · Console_clean: \t\t 2 start_cluster end_cluster\n");
		 printf(" · Console_trace: \t\t 3 n_sector1 [n_sector2|n_sector3|n_sector4|n_sector5]\n");*/
		printf(": ");
		if (Aux_do_command(functions, commands, n_commands) == 2) {
			break;
		}
	}

	return 0;
}

int Console_info(int n_params, char** params) {
	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));

	// Pedimos el estado del cabezal
	Pack_init_NIPC(nipc, TYPE_STATE);

	if (Pack_process(client_fd, nipc) == -1) {
		//log
		free(nipc->payload);
		free(nipc);
		return -1;
	}

	Sector_Fis sector_fis;
	Pack_set_struct(nipc, &sector_fis);

	printf(">> info\n");
	printf("Posicion del cabezal: %d : %d\n", sector_fis.cylinder, sector_fis.sector);

	free(nipc->payload);
	free(nipc);

	return 0;
}

int Console_clean(int n_params, char** params) {
	printf(">> clean\n");
	Sector* sector = (Sector*) malloc(sizeof(Sector));
	int i;
	for (i = 0; i < sizeof(Sector); i++)
		sector->byte[i] = '\0';
	int n_sector;

	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));

	int result = 0;
	for (n_sector = atoi(params[1]); n_sector < atoi(params[2]); n_sector++) {

		// Pedimos el estado del cabezal
		Pack_init_NIPC(nipc, TYPE_WRITE);

		Pack_set_NIPC(nipc, sector, sizeof(Sector));

		if (Pack_process(client_fd, nipc) == -1) {
			//log
			free(sector);
			free(nipc->payload);
			free(nipc);
			printf("Sector %d not cleaned\n", n_sector);
			result = -1;
			break;
		}

		if (nipc->type == TYPE_ERROR_WRITE) {
			//log
			free(sector);
			free(nipc->payload);
			free(nipc);
			printf("Sector %d not cleaned\n", n_sector);
			result = -1;
			break;
		}

		printf("Sector %d cleaned\n", n_sector);
	}

	free(sector);
	free(nipc->payload);
	free(nipc);
	return result;
}

int Console_trace(int n_params, char** params) {
       printf(">> trace\n");
       printf("\n");

       NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));
       Pack* pack = (Pack*) malloc(sizeof(Pack));
       int result = 0;
       uint32_t n_sector;
       Info_Trace info_trace;

       int n;
       for (n = 1; n < n_params; n++) {

               n_sector = atoi(params[n]);

               Pack_init_NIPC(nipc, /*TYPE_TRACE*/11);

               Pack_set_NIPC(nipc, &n_sector, sizeof(uint32_t));
               Pack_serialized(nipc, pack);

               free(nipc->payload);

               //Enviamos pedido
               if (Pack_send(client_fd, pack) == -1)
                       return -1;

               Pack_reload_pack(pack);
       }

       for (n = 1; n < n_params; n++) {

               if ((Pack_recv(client_fd, pack)) == -1)
                       return -1;
               //Deserializamos el paquete NIPC
               Pack_deserialized(nipc, pack);
               Pack_reload_pack(pack);

               Pack_set_struct(nipc, &info_trace);

               free(nipc->payload);

               printf("Posicion actual: %d:%d\n", info_trace.estado.cylinder, info_trace.estado.sector);
               printf("Sector solicitado: %d:%d\n", info_trace.destino.cylinder, info_trace.destino.sector);

               if (info_trace.estado.cylinder != info_trace.destino.cylinder) {
                       printf("Cilindros recorridos: \t%d:%d ... %d:%d\n", info_trace.estado.cylinder, info_trace.estado.sector, info_trace.destino.cylinder, info_trace.estado.sector);
               }

               if (info_trace.estado.sector == info_trace.destino.sector) {
               } else if (info_trace.estado.sector < info_trace.destino.sector) {
                       printf("Sectores recorrido: \t%d:%d ... %d:%d\n", info_trace.destino.cylinder, info_trace.estado.sector, info_trace.destino.cylinder, info_trace.destino.sector);

               } else {
                       printf("Sectores recorrido: \t%d:%d ... %d:%d >< %d:0 ... %d:%d\n", info_trace.destino.cylinder, info_trace.estado.sector, info_trace.destino.cylinder, info_trace.sectors_per_track,
                                       info_trace.destino.cylinder, info_trace.destino.cylinder, info_trace.destino.sector);
               }

               printf("Tiempo transcurrido: %.3f\n", info_trace.latency);

               if (info_trace.proximo.sector == 65535) {
                       printf("Proximo sector: ...\n");
               } else {
                       printf("Proximo sector: %d:%d\n", info_trace.proximo.cylinder, info_trace.proximo.sector);
               }
               printf("\n");
       }
       free(nipc);
       free(pack);
       return result;
}
int Console_help(int n_params, char** params) {
	Aux_print_string_vec(help_commands, n_commands);
	return 0;
}

int Console_exit(int n_params, char** params) {
	return 0;
}
