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
 *      @FILE:  collections/queue.h
 *      @AUTOR: Facundo Viale
 *      @VERSION:       1.6     (2/09/2009)
 *                                      - Mayor funcionalidad con Closures
 *                              1.5     (28/08/2009)
 *                                      - Full Revision
 *                              1.0     (26/08/2008)
 *                                      - Base Version
 */

#ifndef OLD_QUEUE_H_
#define OLD_QUEUE_H_

#define _XOPEN_SOURCE 500

#include <semaphore.h>
#include <pthread.h>
#include "collections.h"

typedef struct t_queue{
	t_link_element* head;
	t_link_element* tail;
	int elements_count;
	sem_t semaforo;
	pthread_mutex_t mutex;
	pthread_mutex_t external_mutex;
} t_queue;

t_queue *collection_queue_create();
void collection_queu_clean(t_queue *queue, void(*data_destroyer)(void*));
void collection_queue_destroy(t_queue *queue, void(*data_destroyer)(void*));
void collection_queue_push(t_queue *queue, void *element);
void collection_queue_push_head(t_queue *queue, void *element);
void *collection_queue_pop(t_queue *queue);
void* collection_queue_invert(t_queue *queue);
void collection_queue_iterator(t_queue *queue, void(*closure)(void*));
int collection_queue_size(t_queue *queue);
int collection_queue_isEmpty(t_queue *queue);
void* collection_queue_find(t_queue *queue, int(*closure)(void*, void*), void*);
#endif /*OLD_QUEUE_H_*/
