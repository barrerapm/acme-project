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
 *      @FILE:  collections/list.c
 *      @AUTOR: Facundo Viale
 *      @VERSION:       1.6     (2/09/2009)
 *                                      - Mayor funcionalidad con Closures
 *                              1.5     (27/08/2009)
 *                                      - Full Revision
 *                              1.0     (26/08/2008)
 *                                      - Base Version
 */

#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

#include "list.h"

/*
 * @NAME: collection_list_create
 * @DESC: Crea una lista
 */
t_list *collection_list_create() {
	t_list *list = malloc(sizeof(t_list));
	list->head = NULL;
	list->elements_count = 0;
	if (sem_init(&list->semaforo, 0, 0) == -1) {
		free(list);
		return NULL;
	}
	if (pthread_mutex_init(&(list->mutex), NULL) == -1) {
		free(list);
		return NULL;
	}
	if (pthread_mutex_init(&(list->external_mutex), NULL) == -1) {
		free(list);
		return NULL;
	}
	return list;
}

/*
 * @NAME: collection_list_add
 * @DESC: Agrega un elemento al final de la lista
 */
int collection_list_add(t_list *list, void *data) {
	t_link_element *new_element = malloc(sizeof(t_link_element));
	new_element->data = data;
	new_element->next = NULL;

	//sem_wait(&list->semaforo);
	pthread_mutex_lock(&(list->mutex));
	if (list->head == NULL) {
		//new_element->next = list->head;
		list->head = new_element;
	} else {
		t_link_element* element = list->head;
		while (element->next != NULL) {
			element = element->next;
		}
		element->next = new_element;
	}
	list->elements_count++;
	sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
	return list->elements_count - 1;
}

/*
 * @NAME: collection_list_get
 * @DESC: Retorna el contenido de una posicion determianda de la lista
 */
void* collection_list_get(t_list *list, int num) {
	void* data = NULL;
	int cont;

	//sem_wait(&list->semaforo);
	pthread_mutex_lock(&(list->mutex));
	if ((list->elements_count > num) && (num >= 0)) {
		t_link_element *element = list->head;
		for (cont = 0; cont < num; element = element->next, cont++)
			;
		data = element->data;
	}
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
	return data;
}

/*
 * @NAME: collection_list_put
 * @DESC: Agrega un elemento en una posicion determinada de la lista
 *//*
 void collection_list_put(t_list *list, int num, void *data) {
 t_link_element* element = NULL;
 t_link_element* new_element = NULL;
 int cont;
 if ((list->elements_count > num) && (num >= 0)) {
 //sem_wait(&list->semaforo);
 pthread_mutex_lock(&(list->mutex));
 new_element = malloc(sizeof(t_link_element));
 new_element->data = data;
 if (num == 0) {
 new_element->next = list->head;
 list->head = new_element;
 } else {
 element = list->head;
 for (cont = 0; cont < num; element = element->next, cont++)
 ;
 new_element->next = element->next;
 element->next = new_element;
 }
 list->elements_count++;
 sem_post(&list->semaforo);
 pthread_mutex_unlock(&(list->mutex));
 }
 }*/

/*
 * @NAME: collection_list_switch
 * @DESC: Coloca un valor en una de la posiciones de la lista retornando el valor anterior
 *//*
 void *collection_list_switch(t_list *list, int num, void *data) {
 void *old_data = NULL;
 int cont;
 sem_wait(&list->semaforo);
 if ((list->elements_count > num) && (num >= 0)) {
 t_link_element *element = list->head;
 for (cont = 0; cont < num; element = element->next, cont++)
 ;
 old_data = element->data;
 element->data = data;
 }
 sem_post(&list->semaforo);
 return old_data;
 }*/

/*
 * @NAME: collection_list_set
 * @DESC: Coloca un valor en una de la posiciones de la lista liberando el valor anterior
 *//*
 void collection_list_set(t_list *list, int num, void *data,
 void(*data_destroyer)(void*)) {
 void *old_data = collection_list_switch(list, num, data);
 if (old_data != NULL) {
 data_destroyer(old_data);
 }
 }*/

/*
 * @NAME: collection_list_find
 * @DESC: Retorna el primer valor encontrado, el cual haga que el closure devuelva != 0
 */
void* collection_list_find(t_list *list, int(*closure)(void*, void*), void* parameter) {
	t_link_element *element = list->head;
	void *data = NULL;

	//sem_wait(&list->semaforo);
	pthread_mutex_lock(&(list->mutex));
	while (element != NULL) {
		if (closure(element->data, parameter)) {
			data = element->data;
			break;
		}
		element = element->next;
	}
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
	return data;
}

/*
 * @NAME: collection_list_iterator
 * @DESC: Itera la lista llamando al closure por cada elemento
 */
void collection_list_iterator(t_list* list, void(*closure)(void*, void*), void* parameter) {
	t_link_element *element = list->head;
	pthread_mutex_lock(&(list->mutex));
	//sem_wait(&list->semaforo);
	while (element != NULL) {
		closure(element->data, parameter);
		element = element->next;
	}
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
}

/*
 * @NAME: collection_list_remove
 * @DESC: Remueve un elemento de la lista de una determinada posicion y lo retorna.
 */
void *collection_list_remove(t_list *list, int num) {
	void *data = NULL;
	t_link_element *aux_element = NULL;

	/*if (list->head == NULL
	 )
	 return NULL;*/

	sem_wait(&list->semaforo);
	pthread_mutex_lock(&(list->mutex));
	if (num == 0) {
		aux_element = list->head;
		data = list->head->data;
		list->head = list->head->next;
	} else if ((list->elements_count > num) && (num > 0)) {
		t_link_element *element = list->head;
		int cont;
		for (cont = 0; cont < (num - 1); element = element->next, cont++)
			;
		data = element->next->data;
		aux_element = element->next;
		element->next = element->next->next;
	}
	list->elements_count--;
	free(aux_element);
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
	return data;
}

/*
 * @NAME: collection_list_removeByClosure2
 * @DESC: Remueve el primer elemento de la lista que haga que el closure devuelva != 0.
 */
void* collection_list_removeByClosure2(t_list *list, int(*closure)(void*, void*), void* parameter) {
	t_link_element *element_aux, *element_ant, *element;
	void *data = NULL;

	sem_wait(&list->semaforo);
	pthread_mutex_lock(&(list->mutex));

	element_ant = NULL;
	element = list->head;
	while (element != NULL) {
		if (closure(element->data, parameter)) {
			if (list->head->next == NULL) {
				element_aux = list->head;
				list->head = list->head->next;
				element = NULL;
			} else if (element_ant == NULL) {
				element_aux = element;
				list->head = list->head->next;
				element = list->head;
			} else {
				element_aux = element;
				element = element->next;
				element_ant->next = element;
			}
			data = element_aux->data;
			free(element_aux);
			list->elements_count--;
			break;
		} else {
			element_ant = element;
			element = element->next;
		}
	}
	//sem_post(&list->semaforo);
	pthread_mutex_unlock(&(list->mutex));
	return data;
}

/*
 * @NAME: collection_list_remove_and_destroy
 * @DESC: Remueve un elemento de la lista de una determinada posicion y libera la memoria.
 *//*
 void collection_list_removeAndDestroy(t_list *list, int num,
 void(*data_destroyer)(void*)) {
 void *data = NULL;
 t_link_element *aux_element = NULL;

 if (list->head == NULL
 )
 return;

 sem_wait(&list->semaforo);
 if (num == 0) {
 aux_element = list->head;
 data = list->head->data;
 list->head = list->head->next;
 } else if ((list->elements_count > num) && (num > 0)) {
 t_link_element *element = list->head;
 int cont;
 for (cont = 0; cont < (num - 1); element = element->next, cont++)
 ;
 data = element->next->data;
 aux_element = element->next;
 element->next = element->next->next;
 }
 list->elements_count--;
 free(aux_element);
 data_destroyer(data);
 //sem_post(&list->semaforo);
 }*/

/*
 * @NAME: collection_list_removeByClosure
 * @DESC: Remueve y destruye los elementos de la lista que hagan que el closure devuelva != 0.
 *//*
 void collection_list_removeByClosure(t_list *list, int(*closure)(void*),
 void(*data_destroyer)(void*)) {
 t_link_element *element_aux, *element_ant, *element;

 sem_wait(&list->semaforo);

 element_ant = NULL;
 element = list->head;
 while (element != NULL) {
 if (closure(element->data)) {
 if (list->head->next == NULL) {
 element_aux = list->head;
 list->head = list->head->next;
 element = NULL;
 } else if (element_ant == NULL) {
 element_aux = element;
 list->head = list->head->next;
 element = list->head;
 } else {
 element_aux = element;
 element = element->next;
 element_ant->next = element;
 }
 if (data_destroyer != NULL
 )
 data_destroyer(element_aux->data);
 free(element_aux);
 list->elements_count--;
 } else {
 element_ant = element;
 element = element->next;
 }
 }
 //sem_post(&list->semaforo);
 }*/

/*
 * @NAME: collection_list_size
 * @DESC: Retorna el tamaño de la lista
 */
int collection_list_size(t_list *list) {
	return list->elements_count;
}

/*
 * @NAME: collection_list_isEmpty
 * @DESC: Verifica si la lista esta vacia
 */
int collection_list_isEmpty(t_list *list) {
	return collection_list_size(list) == 0;
}

/*
 * @NAME: collection_list_clean
 * @DESC: Elimina todos los elementos de la lista.
 */
void collection_list_clean(t_list *list, void(*data_destroyer)(void*)) {
	t_link_element* element;
	sem_wait(&list->semaforo);
	while (list->head != NULL) {
		element = list->head;
		list->head = list->head->next;
		if (data_destroyer != NULL) {
			data_destroyer(element->data);
		}
		free(element);
	}
	list->elements_count = 0;
	sem_post(&list->semaforo);
}

/*
 * @NAME: collection_list_destroy
 * @DESC: Destruye una lista, reciviendo como argumento el metodo encargado de liberar cada
 *              elemento de la lista.
 */
 void collection_list_destroy(t_list *list, void(*data_destroyer)(void*)) {
 if (data_destroyer != NULL) {
 collection_list_clean(list, data_destroyer);
 }
 sem_destroy(&list->semaforo);
 free(list);
 }
