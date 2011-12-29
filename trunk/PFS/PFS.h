#ifndef PFS_H_
#define PFS_H_
#define MAX_NAME_SHORT_ENTRY 1024

#include "Socket.h"
#include "Struct.h"

typedef struct config_pfs {
	unsigned short bytes_per_sector;
	unsigned short blocks_per_cluster;
	unsigned short sectors_per_block;
	unsigned short cache_size;
	SocketConfig socket_config;
} ConfigPFS;

struct boot_sector {
	uint8_t jump_instruction[3];
	uint8_t oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t number_of_fats;
	uint16_t root_directory_location;
	uint16_t total_sectors;
	uint8_t media_descriptor;
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t number_of_heads;
	uint32_t count_of_hidden_sectors;
	uint32_t total_sectors2;
	// Fat32
	uint32_t fat32_sectors_per_fat;
	uint16_t fat32_flags;
	uint16_t fat32_version;
	uint32_t fat32_first_cluster;
	uint16_t fat32_sector_num_fs_inf_sec;
	uint16_t fat32_sector_num_copy_bs;
	uint8_t fat32_reserved[12];
	uint8_t fat32_physical_drive_num;
	uint8_t fat32_reserved_12_16;
	uint8_t fat32_ext_boot_signature;
	uint32_t fat32_serial_num2;
	uint8_t fat32_volume_label[11];
	uint8_t fat32_file_system_type[8];
	uint8_t fat32_os_boot_code32[420];
	uint16_t fat32_boot_sector_signature;
}__attribute__((packed));

struct fs_info {
	uint8_t sector_signature[4];
	uint8_t reserved[480];
	uint8_t sector_signature2[4];
	uint32_t free_clusters;
	uint32_t most_recently_alloc_cluster;
	uint8_t reserved2[12];
	uint8_t sector_signature3[4];
}__attribute__((packed));

struct FAT_table {
	uint32_t* entry;//[130816];
};

struct dir_entry {
	int8_t dos_file_name[8];
	int8_t dos_file_extension[3];
	uint8_t file_attributes;
	uint8_t reserved;
	uint8_t create_time;
	uint16_t create_time2;
	uint16_t create_date;
	uint16_t last_acces_date;
	uint16_t fst_cluster_high;
	uint16_t last_modified_time;
	uint16_t last_modified_date;
	uint16_t fst_cluster_low;
	uint32_t file_size;
}__attribute__((packed));

struct long_dir_entry {
	uint8_t sequence_number;
	uint16_t name1[5];
	uint8_t attributes;
	uint8_t reserved;
	uint8_t checksum;
	uint16_t name2[6];
	uint16_t first_cluster_n;
	uint16_t name3[2];
}__attribute__((packed));

typedef struct FAT_table TablaFAT;
typedef struct boot_sector BootSector;
typedef struct fs_info FSInfo;
typedef struct dir_entry DirEntry;
typedef struct long_dir_entry LongDirEntry;

void PFS_file_list(void);
int PFS_rename_file(const char*, const char*);
int PFS_directory_list(const char*, char***, int*);
int PFS_read_file_content(const char*, off_t, char*, size_t);
int32_t PFS_get_path_entries(const char*, LongDirEntry*, DirEntry*, int*, int*);
int32_t PFS_search_file_entries(const char*, int, LongDirEntry*, DirEntry*);
char* PFS_get_file_name(int attr, LongDirEntry*, DirEntry*);
char* PFS_get_long_file_name(LongDirEntry*);
unsigned char* PFS_get_short_file_name(DirEntry*);
void PFS_set_file_name(const unsigned char*, LongDirEntry*, DirEntry*);
void PFS_set_long_file_name(const char*, LongDirEntry*);
void PFS_set_short_file_name(const char*, DirEntry*);
int32_t PFS_load_entry(Cluster*, int, LongDirEntry*, DirEntry*);
void PFS_load_long_entry(Cluster*, int, LongDirEntry*);
void PFS_load_short_entry(Cluster*, int, DirEntry*);
int32_t PFS_max_cluster_entries(void);
int32_t PFS_max_sector_entries();
int PFS_max_block_entries(void);
int32_t PFS_n_first_file_cluster(DirEntry*);
int32_t PFS_inicializar_config(void);
void PFS_exit();
int PFS_get_n_block_by_cluster(int, int*);
int PFS_get_n_block_by_sector(int, int*);
int PFS_remove_file_entries(const char*, int);
void PFS_destroy_entries(LongDirEntry*, DirEntry*);
void PFS_remove_entry(int, Block*, int, int);
void PFS_set_entries_to_block(LongDirEntry*, DirEntry*, int, Block*);
char* PFS_dir_path(const char*);
char* PFS_path_name(const char*);
void PFS_set_first_n_cluster_entry(int, LongDirEntry*, DirEntry*);
int PFS_set_file_entries_free(const char*, int);
void PFS_set_entry(Cluster*, int, void*);
void PFS_n_block_n_sector_of_cluster_by_bytes(int*, int*, int*);
unsigned char PFS_lfn_checksum(const unsigned char *);
void PFS_hilo_consola();
int PFS_create_dir(const char* path);
int PFS_create_file(const char* path);
int PFS_set_path_entries_free(const char* path);
int PFS_rename_path(const char* path, const char* new_path);
void fsinfo(int, char **);
int finfo(int, char **);
int PFS_n_block_by_sector(int n_sector);
uint32_t PFS_n_clusters_by_bytes(uint32_t bytes);
void PFS_set_block_by_FAT_entry(int n_block, Block* block);
int PFS_first_FAT_n_entry_in_block(int n_block);
int PFS_n_block_by_FAT_entry(int n_entry);
int PFS_entries_per_block();
int PFS_first_FAT_block();
void PFS_clean_cluster(Cluster* cluster);
void PFS_init_block(Block* block);
void PFS_init_entries(int attr, const unsigned char* file_name, int first_file_cluster, LongDirEntry* long_dir_entry, DirEntry* dir_entry);
int PFS_write_entries(int dir_n_cluster, LongDirEntry* long_dir_entry, DirEntry* dir_entry);
int PFS_get_file_entries(Cluster* cluster, const char* file_name, LongDirEntry* long_dir_entry, DirEntry* dir_entry, int* n_entry);
int PFS_is_visible_file_entry(LongDirEntry* long_dir_entry, DirEntry* dir_entry);
int PFS_n_block_by_n_cluster(int n_cluster);
void PFS_clean_entries(LongDirEntry* long_dir_entry, DirEntry* dir_entry);
void PFS_clean_long_dir_entry(LongDirEntry* dir_entry);

#endif /* PFS_H_ */
