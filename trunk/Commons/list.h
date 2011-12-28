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
 *      @FILE:  collections/list.h
 *      @AUTOR: Facundo Viale
 *      @VERSION:       1.6     (2/09/2009)
 *                                      - Mayor funcionalidad con Closures
 *                              1.5     (27/08/2009)
 *                                      - Full Revision
 *                              1.0     (26/08/2008)
 *                                      - Base Version
 */

#ifndef OLD_LIST_H_
#define OLD_LIST_H_

#define _XOPEN_SOURCE 500

#include <semaphore.h>
#include <pthread.h>
#include "collections.h"

typedef struct t_list {
	t_link_element *head;
	int elements_count;
	sem_t semaforo;
	pthread_mutex_t mutex;
	pthread_mutex_t external_mutex;
} t_list;

t_list *collection_list_create();
int collection_list_add(t_list *list, void *data);
void *collection_list_get(t_list *list, int num);
void collection_list_put(t_list *list, int num, void *data);
void *collection_list_switch(t_list* list, int num, void* data);
void collection_list_set(t_list* list, int num, void* data,
		void(*data_destroyer)(void*));
void *collection_list_find(t_list *list, int(*closure)(void*, void*), void*);
void collection_list_iterator(t_list *list, void(*closure)(void*, void*), void*);
void *collection_list_remove(t_list *list, int num);
void collection_list_removeAndDestroy(t_list *list, int num,
		void(*data_destroyer)(void*));
void collection_list_removeByPointer(t_list *list, void *data,
		void(*data_destroyer)(void*));
void collection_list_removeByClosure(t_list *list, int(*closure)(void*),
		void(*data_destroyer)(void*));
void *collection_list_removeByClosure2(t_list *list, int(*closure)(void*, void*), void*);
int collection_list_size(t_list *list);
int collection_list_isEmpty(t_list *list);
void collection_list_clean(t_list *list, void(*data_destroyer)(void*));
void collection_list_destroy(t_list *list, void(*data_destroyer)(void*));

#endif /*OLD_LIST_H_*/
