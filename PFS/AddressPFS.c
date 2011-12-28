#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include "Struct.h"
#include "PFS.h"
#include "Addressing.h"
#include "AddressPFS.h"
#include "Pack.h"
#include "Socket.h"
#include "log.h"
#include "Type.h"

extern ConfigPFS config_pfs;
extern Connection* connections_set;
extern sem_t* sem_cont_connections;
extern t_log* project_log;
extern BootSector* bs;

// Recibe el n_block a pedir y devuelve el block cargado. Retorna -1 en caso de error
int AddressPFS_read_block(int n_block, Block* block) {
	int pos, i, initial_sector, n_sector;
	// Buscamos una conexion disponible en el vector de conexiones
	Socket_get_connection(connections_set, &pos, sem_cont_connections);

	initial_sector = n_sector = n_block * config_pfs.sectors_per_block;
	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));
	Pack_init_NIPC(nipc, TYPE_READ);
	Pack* pack = (Pack*) malloc(sizeof(Pack));

	// Enviamos los 2 pedidos de sectores del bloque
	for (i = 0; i < config_pfs.sectors_per_block; i++) {
		Pack_set_NIPC(nipc, &n_sector, sizeof(int));
		//Aux_printf("[AddressPFS_read_block] Envio el sector: %d\n", n_sector);
		Pack_serialized(nipc, pack);
		if (Pack_send(connections_set[pos].client_fd, pack) == -1) {
			log_warning(project_log, "[AddressPFS_read_block]", "%s", "Error al enviar un sector a leer");
			return -1;
		}
		free(nipc->payload);
		Pack_reload_pack(pack);
		n_sector++;
	}

	Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));

	// Recibimos los 2 sectores pedidos del bloque
	for (i = 0; i < config_pfs.sectors_per_block; i++) {
		if ((Pack_recv(connections_set[pos].client_fd, pack)) == -1) {
			log_warning(project_log, "[AddressPFS_read_block]", "%s", "Error al recibir un sector leido");
			return -1;
		}
		Pack_deserialized(nipc, pack);
		if (nipc->type == TYPE_ERROR_READ) {
			log_warning(project_log, "[AddressPFS_read_block]", "%s", "Type error al leer un sector");
			return -1;
		}
		memcpy(sec_nsec, nipc->payload, sizeof(Sector_n_sector));
		memcpy(block->sector[sec_nsec->n_sector - initial_sector].byte, &(sec_nsec->sector), sizeof(Sector));
		//log_info(project_log,"[AddressPFS_read_block]","%s%d%s%d","Recibi el pedido del sector ",sec_nsec->n_sector," y el tamaño del nipc es ",nipc->length);
		free(nipc->payload);
		Pack_reload_pack(pack);
	}

	free(nipc);
	free(pack);
	free(sec_nsec);

	// Desocupamos la conexion
	Socket_release_connection(connections_set, pos, sem_cont_connections);

	return 0;
}

// Recibe el n_block y block a grabar y lo graba en disco
int AddressPFS_write_block(int n_block, Block* block) {
	int pos, i, initial_sector, n_sector;
	// Buscamos una conexion disponible en el vector de conexiones
	Socket_get_connection(connections_set, &pos, sem_cont_connections);

	initial_sector = n_sector = n_block * config_pfs.sectors_per_block;
	NIPC* nipc = (NIPC*) malloc(sizeof(NIPC));
	Pack_init_NIPC(nipc, TYPE_WRITE);
	Pack* pack = (Pack*) malloc(sizeof(Pack));
	Sector_n_sector* sec_nsec = (Sector_n_sector*) malloc(sizeof(Sector_n_sector));

	// Enviamos los 2 sectores a escribir del bloque
	for (i = 0; i < config_pfs.sectors_per_block; i++) {
		sec_nsec->n_sector = n_sector;
		//sec_nsec->sector.byte = block->sector[i].byte;
		memcpy(&(sec_nsec->sector.byte), block->sector[i].byte, sizeof(Sector));
		Pack_set_NIPC(nipc, sec_nsec, sizeof(Sector_n_sector));
		//Aux_printf("Nipc length: %d\n",nipc->length);
		Pack_serialized(nipc, pack);
		//Aux_printf("Pack length: %d\n",pack->length);
		if (Pack_send(connections_set[pos].client_fd, pack) == -1) {
			log_warning(project_log, "[AddressPFS_write_block]", "%s", "Error al enviar un sector a escribir");
			return -1;
		}
		free(nipc->payload);
		free(pack->serialized);
		//Pack_reload_pack(pack);
		n_sector++;
	}
	free(sec_nsec);

	// Recibimos las 2 respuestas de sectores escritos del bloque
	for (i = 0; i < config_pfs.sectors_per_block; i++) {
		if ((Pack_recv(connections_set[pos].client_fd, pack)) == -1) {
			log_warning(project_log, "[AddressPFS_write_block]", "%s", "Error al recibir la respuesta de un sector escrito");
			return -1;
		}
		Pack_deserialized(nipc, pack);
		if (nipc->type == TYPE_ERROR_WRITE) {
			log_warning(project_log, "[AddressPFS_write_block]", "%s", "Type error al escribir un sector");
			return -1;
		}
		//log_info(project_log,"[AddressPFS_write_block]","%s%d","Recibi la respuesta de escritura de un sector y el tamaño del nipc es ",nipc->length);
		free(nipc->payload);
		Pack_reload_pack(pack);
	}

	free(nipc);
	free(pack);

	// Desocupamos la conexion
	Socket_release_connection(connections_set, pos, sem_cont_connections);

	return 0;
}

/*
 // (15-12) Recibe el n_cluster a pedir y devuelve el cluster cargado. Retorna -1 en caso de error
 int AddressPFS_read_cluster(Cluster* cluster, int n_cluster) { //DEBERIA DEVOLVER EL ERROR
 // Calculamos el primer n_block de este cluster
 int i, n_block = PFS_n_block_by_n_cluster(n_cluster);

 Block* block;
 for (i = 0; i < config_pfs.blocks_per_cluster; i++) {
 block = (Block*) malloc(sizeof(Block));
 if (AddressPFS_read_block(n_block++, block) == -1) {
 log_warning(project_log, "[AddressPFS_read_cluster]", "%s%d%s", "Error al leer el bloque ", n_block - 1, " del cluster");
 return -1;
 }
 memcpy(&(cluster->block[i]), block, sizeof(Block));
 free(block);
 }
 return 0;
 }

 // (15-12) Recibe el n_cluster y cluster a grabar y lo graba en disco
 int AddressPFS_write_cluster(int n_cluster, Cluster* cluster) {
 // Calculamos el primer n_block de este cluster
 int i, n_block = PFS_n_block_by_n_cluster(n_cluster);

 // Escribimos cada bloque del cluster
 for (i = 0; i < config_pfs.blocks_per_cluster; i++) {
 if (AddressPFS_write_block(n_block++, &(cluster->block[i])) == -1) {
 log_warning(project_log, "[AddressPFS_write_cluster]", "%s%d%s", "Error al escribir el bloque ", n_block - 1, " del cluster");
 return -1;
 }
 }
 return 0;
 }
 */

int AddressPFS_read_cluster(Cluster* cluster, int n_cluster) { //DEBERIA DEVOLVER EL ERROR
	int i, first_DataRegion_sector = bs->reserved_sector_count + bs->number_of_fats * bs->fat32_sectors_per_fat;
	int n_sector = first_DataRegion_sector + n_cluster * bs->sectors_per_cluster;
	int n_block = PFS_n_block_by_sector(n_sector);
	Block* block;
	for (i = 0; i < config_pfs.blocks_per_cluster; i++, n_block++) {
		block = (Block*) malloc(sizeof(Block));
		AddressPFS_read_block(n_block, block);
		cluster->block[i] = *block;
		free(block);
	}
	return 0;
}

// Recibe el n_cluster y cluster a grabar y lo graba en disco
int AddressPFS_write_cluster(int n_cluster, Cluster* cluster) { //DEBERIA DEVOLVER EL ERROR
	int i, first_DataRegion_sector = bs->reserved_sector_count + bs->number_of_fats * bs->fat32_sectors_per_fat;
	int n_sector = first_DataRegion_sector + n_cluster * bs->sectors_per_cluster;
	int n_block = PFS_n_block_by_sector(n_sector);
	Block* block;
	for (i = 0; i < config_pfs.blocks_per_cluster; i++) {
		block = (Block*) malloc(sizeof(Block));
		*block = cluster->block[i];
		AddressPFS_write_block(n_block++, block);
		free(block);
	}
	return 0;
}
