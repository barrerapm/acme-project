#ifndef PFS_H_
#define PFS_H_
#define MAX_NAME_SHORT_ENTRY 1024

typedef struct config_pfs {
	unsigned short bytes_per_sector;
	unsigned short sectors_per_cluster;
	unsigned short clusters_per_block;
} ConfigPFS;

struct boot_sector {
	u_char jump_instruction[3];
	u_char oem_name[8];
	unsigned short bytes_per_sector;
	unsigned char sectors_per_cluster;
	unsigned short reserved_sector_count;
	unsigned char number_of_fats;
	unsigned short root_directory_location;
	unsigned short total_sectors;
	unsigned char media_descriptor;
	unsigned short sectors_per_fat;
	unsigned short sectors_per_track;
	unsigned short number_of_heads;
	unsigned int count_of_hidden_sectors;
	unsigned int total_sectors2;
	// Fat32
	unsigned int fat32_sectors_per_fat;
	unsigned short fat32_flags;
	unsigned short fat32_version;
	unsigned int fat32_cluster_num_dir_start;
	unsigned short fat32_sector_num_fs_inf_sec;
	unsigned short fat32_sector_num_copy_bs;
	u_char fat32_reserved[12];
	u_char fat32_physical_drive_num;
	u_char fat32_reserved_12_16;
	u_char fat32_ext_boot_signature;
	unsigned int fat32_serial_num2;
	u_char fat32_volume_label[11];
	u_char fat32_file_system_type[8];
	u_char fat32_os_boot_code32[420];
	unsigned short fat32_boot_sector_signature;
	// Fat1216
	u_char fat1216_physical_drive_number;
	u_char fat1216_current_head;
	u_char fat1216_ext_boot_signature;
	unsigned int fat1216_serial_num;
	u_char fat1216_volume_label[11];
	u_char fat1216_file_system_type[8];
	u_char fat1216_os_boot_code1216[448];
	unsigned short fat1216_boot_sector_signature;
}__attribute__((packed));

struct fs_info {
	u_char sector_signature[4];
	u_char reserved[480];
	u_char sector_signature2[4];
	unsigned int free_clusters;
	unsigned int most_recently_alloc_cluster;
	u_char reserved2[12];
	u_char sector_signature3[4];
}__attribute__((packed));

struct FAT_table {
	unsigned int entry[130816];
};

struct dir_entry {
	char dos_file_name[8];
	char dos_file_extension[3];
	u_char file_attributes;
	u_char reserved;
	u_char create_time;
	unsigned short create_time2;
	unsigned short create_date;
	unsigned short last_acces_date;
	unsigned short fst_cluster_high;
	unsigned short last_modified_time;
	unsigned short last_modified_date;
	unsigned short fst_cluster_low;
	unsigned int file_size;
}__attribute__((packed));

struct long_dir_entry {
	u_char sequence_number;
	unsigned short name1[5];
	u_char attributes;
	u_char reserved;
	u_char checksum;
	unsigned short name2[6];
	unsigned short first_cluster_n;
	unsigned short name3[2];
}__attribute__((packed));

struct sector;
struct cluster;
struct block;

typedef struct FAT_table TablaFAT;
typedef struct boot_sector BootSector;
typedef struct fs_info FSInfo;
typedef struct dir_entry DirEntry;
typedef struct long_dir_entry LongDirEntry;

void PFS_file_list(BootSector*);
void PFS_directory_list(BootSector*, TablaFAT*, const char*, char**, int*);
void PFS_rename_file(BootSector*, TablaFAT*, const char*, const char*);
void PFS_read_file_content(BootSector*, TablaFAT*, const char*);
int PFS_search_file_entry(BootSector*, DirEntry*, LongDirEntry*, const char*);
char* PFS_file_name(DirEntry*, LongDirEntry*);
void PFS_load_entry(struct cluster*, DirEntry*, LongDirEntry*, int);
int PFS_max_cluster_entries(BootSector*);
int PFS_n_first_file_cluster(DirEntry*);
int PFS_total_file_clusters(BootSector*, DirEntry*);
#endif /* PFS_H_ */
