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
 *      @FILE:  collections/collections.h
 *      @AUTOR: Facundo Viale
 *      @VERSION:       1.5     (27/08/2009)
 *                                      - Full Revision
 *                              1.0     (01/09/2008)
 *                                      - Base Version
 */

#ifndef OLD_COLLECTIONS_H_
#define OLD_COLLECTIONS_H_

#define _XOPEN_SOURCE 500

#include <semaphore.h>
#include <pthread.h>

struct link_element {
	void *data;
	struct link_element *next;
};
typedef struct link_element t_link_element;

struct dlink_element {
	struct dlink_element *previous;
	void *data;
	struct dlink_element *next;
};
typedef struct dlink_element t_dlink_element;

#endif /*OLD_COLLECTIONS_H_*/
