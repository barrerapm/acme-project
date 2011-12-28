#ifndef CACHE_H_
#define CACHE_H_

#include "list.h"

typedef struct cluster_info {
	Cluster* cluster;
	int n_cluster;
	int flag_flush;
} ClusterInfo;

typedef struct cache {
	char* file_name;
	int file_fd;
	int n_opens;
	t_list* cluster_infos;
} Cache;

void Cache_read_cluster(Cluster*, int, char*);
void Cache_write_cluster(int, Cluster*, char*);
void Cache_add_new(t_list*, Cluster*, uint32_t, int);
int Cache_contains_file_name(void*, void*);
int Cache_contains_n_cluster(void*, void*);
void Cache_destroy_cluster_info(void*);
void Cache_create_dump(void);
void Cache_flush(Cache* this_cache);

#endif /* CACHE_H_ */
