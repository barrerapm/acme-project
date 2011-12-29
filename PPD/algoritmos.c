#include <stdio.h>
#include <stdlib.h>
#include "algoritmos.h"
#include "Type.h"
#include "Aux.h"
#include <math.h>

extern Nodo* head;
extern Nodo* tail;
extern Sector_Fis* estado;
extern uint32_t steps, MAX_STEPS;

// ------------------------------- ALGORITMOS --------------------------------------------------
Info* sstf(Info* info) {

	Aux_printf("ESTOY EN LA PROFUNDIDAD DEL ALGORITMO SSTF\n");

	int val, val_aux;

	Nodo* aux;
	Nodo* ant;
	Nodo* aUsar;
	Nodo* antUsar;
/*
	aux = head;
	int n = 0;
	while (aux != NULL) {
		n++;
		//printf("PRE cyl: %d   sec: %d\n", aux->info->sector_fis.cylinder, aux->info->sector_fis.sector);
		aux = aux->sgte;
	}
	printf("[sstf] Cantidad de nodos: %d\n",n);
*/
	char i;
	for (i = 0; i < 2; i++) {
		ant = aux = aUsar = head;
		antUsar = NULL;

		if (aux != NULL) {
			val = modulo(aux->info->sector_fis.cylinder - estado->cylinder);
			ant = aux;
			aux = aux->sgte;

			while (aux != NULL) {
				if ((val_aux = modulo(aux->info->sector_fis.cylinder - estado->cylinder)) < val) {
					val = val_aux;
					antUsar = ant;
					aUsar = aux;
				}
				ant = aux;
				aux = aux->sgte;
			}

			Aux_printf("\tvalor i: %d\n", i);

			if (i == 1) {
				if (aUsar == NULL) {
					return NULL;
				}
				return (aUsar->info);
			}

			if (antUsar == NULL) {
				head = aUsar->sgte;
				if (tail == aUsar)
					tail = head;
			} else {
				if (aUsar == tail) {
					antUsar->sgte = NULL;
					tail = antUsar;
				} else
					antUsar->sgte = aUsar->sgte;

			}

/*			aux = head;
			while (aux != NULL) {
				Aux_printf("POST cyl: %d   sec: %d\n", aux->info->sector_fis.cylinder, aux->info->sector_fis.sector);
				aux = aux->sgte;
			}
*/
			info->type = aUsar->info->type;
			info->sector_fis = aUsar->info->sector_fis;
			info->sector = aUsar->info->sector;
			info->fd = aUsar->info->fd;

			Aux_printf("A USAR  cyl: %d   sec: %d\n", aUsar->info->sector_fis.cylinder, aUsar->info->sector_fis.sector);

			// Agregado 25-11
			Aux_printf("En sstf el info quedo type: %d fd: %d sec_fis: %d\n", info->type, info->fd, info->sector_fis.sector);
			free(aUsar->info);
			free(aUsar);
		}
	}
	return NULL;

}

Info* sstf_nSteps(Info* info) {

	int val, val_aux;
	uint8_t counter = steps;

	Nodo* aux;
	Nodo* ant;
	Nodo* aUsar;
	Nodo* antUsar;

/*	aux = head;
	while (aux != NULL) {
		Aux_printf("PRE cyl: %d   sec: %d\n", aux->info->sector_fis.cylinder, aux->info->sector_fis.sector);
		aux = aux->sgte;
	}
*/
	char i;
	for (i = 0; i < 2; i++) {

		ant = aux = aUsar = head;
		antUsar = NULL;

		if (aux != NULL) {
			val = modulo(aux->info->sector_fis.cylinder - estado->cylinder);
			ant = aux;
			aux = aux->sgte;
			counter--;

			while (aux != NULL && counter > 0) {
				if ((val_aux = modulo(aux->info->sector_fis.cylinder - estado->cylinder)) < val) {
					val = val_aux;
					antUsar = ant;
					aUsar = aux;
				}
				ant = aux;
				aux = aux->sgte;
				counter--;

			}

			Aux_printf("[sstf_nSteps] i: %d cyl: %d\t sec: %d\n", i, aUsar->info->sector_fis.cylinder, aUsar->info->sector_fis.sector);

			if (i == 1) {
				return (aUsar->info);
			}

			if (antUsar == NULL) {
				head = aUsar->sgte;
				if (tail == aUsar) {
					tail = head;
				}
			} else {
				if (aUsar == tail) {
					antUsar->sgte = NULL;
					tail = antUsar;
				} else {
					antUsar->sgte = aUsar->sgte;
				}
			}

/*			aux = head;
			while (aux != NULL) {
				Aux_printf("POST cyl: %d   sec: %d\n", aux->info->sector_fis.cylinder, aux->info->sector_fis.sector);
				aux = aux->sgte;
			}
*/
			info->type = aUsar->info->type;
			info->sector_fis = aUsar->info->sector_fis;
			info->sector = aUsar->info->sector;
			info->fd = aUsar->info->fd;

			free(aUsar->info);
			free(aUsar);

			(steps)--;
			if (steps == 0)
				steps = MAX_STEPS;

			if (counter == 0 || head == NULL)
				counter = steps;
		}
	}
	return NULL;
}

int modulo(int valor) {
	return (int) fabs((double) valor);
}
