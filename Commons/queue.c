/*
 * Copyright (c) 2011, C-Talks and/or its owners. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact C-Talks owners, Matias Dumrauf or Facundo Viale
 * or visit https://sites.google.com/site/utnfrbactalks/ if you need additional
 * information or have any questions.
 */

/*
 *      @FILE:  collections/queue.c
 *      @AUTOR: Facundo Viale
 *      @VERSION:       1.6     (2/09/2009)
 *                                      - Mayor funcionalidad con Closures
 *                              1.5     (28/08/2009)
 *                                      - Full Revision
 *                              1.0     (26/08/2008)
 *                                      - Base Version
 */

#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include "queue.h"

/*
 * @NAME: collection_queue_create
 * @DESC: Crea y devuelve un puntero a una cola
 */
t_queue *collection_queue_create() {
	t_queue* queue = malloc(sizeof(t_queue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->elements_count = 0;
	if (sem_init(&(queue->semaforo), 0, 0) == -1) {
		return NULL;
	}
	if (pthread_mutex_init(&(queue->mutex), NULL) == -1) {
		return NULL;
	}
	if (pthread_mutex_init(&(queue->external_mutex), NULL) == -1) {
		return NULL;
	}
	return queue;
}

/*
 * @NAME: collection_queue_clean
 * @DESC: Elimina todos los elementos de la cola.
 */
 void collection_queue_clean(t_queue *queue, void(*data_destroyer)(void*)) {
 t_link_element* element;
 sem_wait(&queue->semaforo);
 while (queue->head != NULL) {
 element = queue->head;
 queue->head = queue->head->next;
 data_destroyer(element->data);
 free(element);
 }
 queue->elements_count = 0;
 sem_post(&queue->semaforo);
 }

/*
 * @NAME: collection_queue_destroy
 * @DESC: Destruye una cola, reciviendo como argumento el metodo encargado de liberar cada
 *              elemento de la cola.
 */
 void collection_queue_destroy(t_queue *queue, void(*data_destroyer)(void*)) {
 collection_queue_clean(queue, data_destroyer);
 sem_destroy(&queue->semaforo);
 free(queue);
 }

/*
 * @NAME: collection_queue_push
 * @DESC: Agrega un elemento al final de la cola
 */
void collection_queue_push(t_queue *queue, void *element) {
	//sem_wait(&queue->semaforo);
	pthread_mutex_lock(&(queue->mutex));
	if (element != NULL) {
		t_link_element *auxelement = malloc(sizeof(t_link_element));
		auxelement->data = element;
		auxelement->next = NULL;
		if (queue->tail != NULL) {
			queue->tail->next = auxelement;
			queue->tail = queue->tail->next;
		} else {
			queue->head = auxelement;
			queue->tail = auxelement;
		}
		queue->elements_count++;
	}
	pthread_mutex_unlock(&(queue->mutex));
	sem_post(&queue->semaforo);
}

/*
 * @NAME: collection_queue_push_head
 * @DESC: Agrega un elemento al principio de la cola
 */
void collection_queue_push_head(t_queue *queue, void *element) {
	//sem_wait(&queue->semaforo);
	pthread_mutex_lock(&(queue->mutex));
	if (element != NULL) {
		t_link_element *auxelement = malloc(sizeof(t_link_element));
		auxelement->data = element;
		auxelement->next = NULL;
		if (queue->head != NULL) {
			auxelement->next = queue->head;
			queue->head = auxelement;
		} else {
			queue->head = auxelement;
			queue->tail = auxelement;
		}
		queue->elements_count++;
	}
	pthread_mutex_unlock(&(queue->mutex));
	sem_post(&queue->semaforo);
}

/*
 * @NAME: collection_queue_pop
 * @DESC: Saca un elemento del principio de la cola
 */
void* collection_queue_pop(t_queue *queue) {
	void* data = NULL;
	sem_wait(&queue->semaforo);
	pthread_mutex_lock(&(queue->mutex));
	if (queue->head != NULL) {
		t_link_element *element = queue->head;
		queue->head = queue->head->next;
		if (queue->head == NULL) {
			queue->tail = NULL;
		}
		data = element->data;
		free(element);
		queue->elements_count--;
	}
	//sem_post(&queue->semaforo);
	pthread_mutex_unlock(&(queue->mutex));
	return data;
}

/*
 * @NAME: collection_queue_pop
 * @DESC: Saca un elemento del final de la cola
 */
void* collection_queue_invert(t_queue *queue) {
	t_queue* queue_aux = NULL;
	queue_aux = collection_queue_create();

	pthread_mutex_lock(&(queue->mutex));
	while (collection_queue_size(queue) > 0) {
		collection_queue_push_head(queue_aux, collection_queue_pop(queue));
	}

	while (collection_queue_size(queue_aux) > 0) {
		collection_queue_push(queue, collection_queue_pop(queue_aux));
	}
	pthread_mutex_unlock(&(queue->mutex));
	return queue;
}

/*
 * @NAME: collection_queue_iterator
 * @DESC: Itera la lista llamando al closure por cada elemento
 *//*
 void collection_queue_iterator(t_queue* queue, void(*closure)(void*)) {
 t_link_element *element = queue->head;
 sem_wait(&queue->semaforo);
 while (element != NULL) {
 closure(element->data);
 element = element->next;
 }
 sem_post(&queue->semaforo);
 }
 */

/*
 * @NAME: collection_queue_size
 * @DESC: Devuelve la cantidad de elementos de la cola
 */
int collection_queue_size(t_queue* queue) {
	return queue->elements_count;
}

/*
 * @NAME: collection_queue_isEmpty
 * @DESC: Verifica si la cola esta vacia
 */
int collection_queue_isEmpty(t_queue *queue) {
	return collection_queue_size(queue) == 0;
}

/*
 * @NAME: collection_list_find
 * @DESC: Retorna el primer valor encontrado, el cual haga que el closure devuelva != 0
 */
void* collection_queue_find(t_queue *queue, int(*closure)(void*, void*), void* parameter) {
	t_link_element *element = queue->head;
	void *data = NULL;

	//sem_wait(&list->semaforo);
	pthread_mutex_lock(&(queue->mutex));
	while (element != NULL) {
		if (closure(element->data, parameter)) {
			data = element->data;
			break;
		}
		element = element->next;
	}
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(queue->mutex));
	return data;
}
