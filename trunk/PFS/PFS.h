#ifndef PFS_H_
#define PFS_H_
#define MAX_NAME_SHORT_ENTRY 1024

typedef struct config_pfs {
	unsigned short bytes_per_sector;
	unsigned short sectors_per_cluster;
	unsigned short clusters_per_block;
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
	// Fat1216
	uint8_t fat1216_physical_drive_number;
	uint8_t fat1216_current_head;
	uint8_t fat1216_ext_boot_signature;
	uint32_t fat1216_serial_num;
	uint8_t fat1216_volume_label[11];
	uint8_t fat1216_file_system_type[8];
	uint8_t fat1216_os_boot_code1216[448];
	uint16_t fat1216_boot_sector_signature;
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
	uint32_t entry[130816];
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


struct sector;
struct cluster;
struct block;

typedef struct FAT_table TablaFAT;
typedef struct boot_sector BootSector;
typedef struct fs_info FSInfo;
typedef struct dir_entry DirEntry;
typedef struct long_dir_entry LongDirEntry;

void PFS_file_list(void);
void PFS_rename_file(const char*, const char*);
int32_t PFS_directory_list(const char*, char**, int*);
void PFS_read_file_content(const char*);
int32_t PFS_search_path_entry(DirEntry* dir_entry, LongDirEntry* long_dir_entry, const char* path);
int32_t PFS_search_file_entry(DirEntry*, LongDirEntry*, const char*,int);
void PFS_path_dir_names(const char* , char**,int*);
char* PFS_file_name(int attr, LongDirEntry* , DirEntry* );
char* PFS_long_file_name(LongDirEntry*);
char* PFS_short_file_name(DirEntry*);
int32_t PFS_load_entry(struct cluster* , LongDirEntry* , DirEntry* , int );
void PFS_load_long_entry(struct cluster*,LongDirEntry*, int);
void PFS_load_short_entry(struct cluster*, DirEntry*, int);
int32_t PFS_max_cluster_entries(void);
int32_t PFS_n_first_file_cluster(DirEntry*);
int32_t PFS_total_file_clusters(DirEntry*);
void PFS_inicializar_config(void);

#endif /* PFS_H_ */
