#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "PFS.h"
#include "PPD.h"
#include "utils.h"
#include "PFSPrint.h"
#define FAT_ENTRY_BYTES 4
#define ROOT_ENTRY_BYTES 32
#define MAX_FILE_NAME_LENGHT 28
#define ROOT_CLUSTER 0
#define LAST_FILE_CLUSTER 0xFFFFFFF
#define FREE_CLUSTER 0x0000000
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATRR_LONG_NAME 0x0F

// Prints para pruebas
extern ConfigPFS config_pfs; // Estructura global config_ppd

void PFSPrint_long_dir_entry(LongDirEntry* long_dir_entry) {
	int i;
	/*
	 unsigned short utf16_name1[5];
	 unsigned short utf16_name2[6];
	 unsigned short utf16_name3[2];
	 strcpy(utf16_name1,long_dir_entry->name1);
	 strcpy(utf16_name2,long_dir_entry->name2);
	 strcpy(utf16_name3,long_dir_entry->name3);

	 char utf8_name1[sizeof(utf16_name1) / 2];
	 char utf8_name2[sizeof(utf16_name2) / 2];
	 char utf8_name3[sizeof(utf16_name3) / 2];
	 int	len_utf8_name1 = 0, len_utf8_name2 = 0, len_utf8_name3 = 0;
	 unicode_utf16_to_utf8_inbuffer(utf16_name1, sizeof(utf16_name1), utf8_name1, &len_utf8_name1);
	 unicode_utf16_to_utf8_inbuffer(utf16_name2, sizeof(utf16_name2), utf8_name2, &len_utf8_name2);
	 unicode_utf16_to_utf8_inbuffer(utf16_name3, sizeof(utf16_name3), utf8_name3, &len_utf8_name3);
	 */
	ln();
	printf(">> Long Directory Entry\n");
	printf("sequence_number: %x\n", long_dir_entry->sequence_number);

	printf("name_1+2+3: ");
	for (i = 0; i < sizeof(long_dir_entry->name1) / 2; i++)
		printf("%1c", long_dir_entry->name1[i]);
	for (i = 0; i < sizeof(long_dir_entry->name2) / 2; i++)
		printf("%1c", long_dir_entry->name2[i]);
	for (i = 0; i < sizeof(long_dir_entry->name3) / 2; i++)
		printf("%1c", long_dir_entry->name3[i]);
	printf(" (UTF16)");
	ln();/*
	 printf("name_1+2+3: ");
	 for (i = 0; i < sizeof(len_utf8_name1); i++)
	 printf("%1c", utf8_name1[i]);
	 for (i = 0; i < sizeof(len_utf8_name2); i++)
	 printf("%1c", utf8_name2[i]);
	 for (i = 0; i < sizeof(len_utf8_name3); i++)
	 printf("%1c", utf8_name3[i]);
	 printf(" (UTF8)");
	 ln();*/
	printf("attributes: %x\n", long_dir_entry->attributes);
	printf("reserved: %x\n", long_dir_entry->reserved);
	printf("checksum: %x\n", long_dir_entry->checksum);
	printf("first_cluster_n: %x\n", long_dir_entry->first_cluster_n);
}

void PFSPrint_dir_entry(DirEntry* dir_entry) {
	int i;
	/*
	 char utf8_name1[sizeof(dir_entry->dos_file_name) / 2];
	 char utf8_name2[sizeof(dir_entry->dos_file_extension) / 2];
	 int	len_utf8_name1 = 0, len_utf8_name2 = 0;
	 unicode_utf16_to_utf8_inbuffer(dir_entry->dos_file_name, sizeof(dir_entry->dos_file_name), utf8_name1, &len_utf8_name1);
	 unicode_utf16_to_utf8_inbuffer(dir_entry->dos_file_extension, sizeof(dir_entry->dos_file_extension), utf8_name2, &len_utf8_name2);
	 */
	printf(">> Directory Entry\n");
	printf("dos_file_name+extension: ");
	for (i = 0; i < sizeof(dir_entry->dos_file_name); i++)
		printf("%c", dir_entry->dos_file_name[i]);
	printf(".");
	for (i = 0; i < sizeof(dir_entry->dos_file_extension); i++)
		printf("%c", dir_entry->dos_file_extension[i]);
	printf(" (UTF16)");
	ln();/*
	 printf("dos_file_name+extension: ");
	 for (i = 0; i < len_utf8_name1; i++)
	 printf("%c", utf8_name1[i]);
	 printf(".");
	 for (i = 0; i < len_utf8_name2; i++)
	 printf("%c", utf8_name2[i]);
	 printf(" (UTF8)");
	 ln();*/
	printf("file_attributes: %x ", dir_entry->file_attributes);
	switch (dir_entry->file_attributes) {
	case 0x01:
		printf("(Read Only)");
		break;
	case 0x02:
		printf("(Hidden)");
		break;
	case 0x04:
		printf("(System)");
		break;
	case 0x08:
		printf("(Volume Label)");
		break;
	case 0x10:
		printf("(Directory)");
		break;
	case 0x20:
		printf("(Archive)");
		break;
	case 0x40:
		printf("(Device)");
		break;
	case 0x80:
		printf("(Unused)");
		break;
	case 0x0F:
		printf("(Long File Name)");
		break;
	}
	ln();
	printf("reserved: %x\n", dir_entry->reserved);
	printf("create_time: %x\n", dir_entry->create_time);
	printf("create_time2: %x\n", dir_entry->create_time2);
	printf("create_date: %x\n", dir_entry->create_date);
	printf("last_acces_date: %x\n", dir_entry->last_acces_date);
	printf("fst_cluster_high: %x\n", dir_entry->fst_cluster_high);
	printf("last_modified_time: %x\n", dir_entry->last_modified_time);
	printf("last_modified_date: %x\n", dir_entry->last_modified_date);
	printf("fst_cluster_low: %x\n", dir_entry->fst_cluster_low);
	printf("file_size: %x\n", dir_entry->file_size);
}

void PFSPrint_FAT_table(TablaFAT FAT_table) {
	int i, total_entries = sizeof(FAT_table.entry) / FAT_ENTRY_BYTES; // Total de bytes de FAT_table/4 bytes
	ln();
	printf("> FAT Table\n");
	for (i = 0; i < 20/*cambiar por: total_entries*/; i++)
		printf("Entrada %d: %x\n", i, FAT_table.entry[i]);
}

void PFSPrint_fs_info(FSInfo* fs_info) {
	int i;
	ln();
	printf("> FS Info\n");
	printf("sector_signature: %s\n", fs_info->sector_signature);
	printf("reserved: %d ", fs_info->reserved[0]);
	for (i = 0; i < sizeof(fs_info->reserved); i++)
		printf("%d ", fs_info->reserved[i]);
	ln();
	printf("sector_signature2: %.4s\n", fs_info->sector_signature2);
	printf("free_clusters: %u\n", fs_info->free_clusters);
	printf("most_recently_alloc_cluster: %u\n", fs_info->most_recently_alloc_cluster);
	printf("reserved2: %d ", fs_info->reserved2[0]);
	for (i = 0; i < sizeof(fs_info->reserved2); i++)
		printf("%d ", fs_info->reserved2[i]);
	ln();
	printf("sector_signature3: %d\n", fs_info->sector_signature3);
}

void PFSPrint_block(BootSector* bs, Block block, int n_block) {
	int i;
	ln();
	printf("> Block %d\n", n_block);
	for (i = 0; i < config_pfs.clusters_per_block; i++) {
		PFSPrint_cluster(bs, block.cluster[i], n_block * config_pfs.clusters_per_block + i);
	}
}

void PFSPrint_cluster(BootSector* bs, Cluster cluster, int n_cluster) {
	int i;
	ln();
	printf("> Cluster %u\n", n_cluster);
	for (i = 0; i < config_pfs.sectors_per_cluster; i++) {
		PFSPrint_sector(cluster.sector[i], bs->reserved_sector_count + (bs->number_of_fats * bs->fat32_sectors_per_fat) + (n_cluster * config_pfs.sectors_per_cluster) + i);
	}
}

void PFSPrint_sector(Sector sector, int n_sector) {
	int i;
	printf(">> Sector %u: ", n_sector);
	for (i = 0; i < config_pfs.bytes_per_sector; i++)
		printf("%c", sector.byte[i]);
	ln();
}

void PFSPrint_config_pfs(void) {
	printf("> Config PFS: ");
	printf("%d ", config_pfs.bytes_per_sector);
	printf("%d ", config_pfs.sectors_per_cluster);
	ln();
}

void PFSPrint_bootsector(BootSector* bs) {
	int i;
	ln();
	printf("> Boot Sector\n");
	printf("jump_instruction: %x ", bs->jump_instruction[0]);
	for (i = 1; i < sizeof(bs->jump_instruction); i++)
		printf("%x ", bs->jump_instruction[i]);
	ln();
	printf("oem_name: %c", bs->oem_name[0]);
	for (i = 1; i < sizeof(bs->oem_name); i++)
		printf("%c", bs->oem_name[i]);
	ln();
	printf("bytes_per_sector: %u\n", bs->bytes_per_sector);
	printf("sectors_per_cluster: %u\n", bs->sectors_per_cluster);
	printf("reserved_sector_count: %u\n", bs->reserved_sector_count);
	printf("number_of_fats: %u\n", bs->number_of_fats);
	printf("root_directory_location: %u\n", bs->root_directory_location);
	printf("total_sectors: %u\n", bs->total_sectors);
	printf("media_descriptor: %x\n", bs->media_descriptor);
	printf("sectors_per_fat: %u\n", bs->sectors_per_fat);
	printf("sectors_per_track: %u\n", bs->sectors_per_track);
	printf("number_of_heads: %u\n", bs->number_of_heads);
	printf("count_of_hidden_sectors: %u\n", bs->count_of_hidden_sectors);
	printf("total_sectors2: %u\n", bs->total_sectors2);
	//Fat32
	if (bs->sectors_per_fat == 0) {
		printf("fat32_sectors_per_fat: %u\n", bs->fat32_sectors_per_fat);
		printf("fat32_flags: %u\n", bs->fat32_flags);
		printf("fat32_version: %u\n", bs->fat32_version);
		printf("fat32_cluster_num_dir_start: %u\n", bs->fat32_cluster_num_dir_start);
		printf("fat32_sector_num_fs_inf_sec: %u\n", bs->fat32_sector_num_fs_inf_sec);
		printf("fat32_sector_num_copy_bs: %u\n", bs->fat32_sector_num_copy_bs);
		printf("fat32_reserved: %u ", bs->fat32_reserved[0]);
		for (i = 1; i < sizeof(bs->fat32_reserved); i++)
			printf("%u ", bs->fat32_reserved[i]);
		ln();
		printf("fat32_physical_drive_num: %u\n", bs->fat32_physical_drive_num);
		printf("fat32_reserved_12_16: %u\n", bs->fat32_reserved_12_16);
		printf("fat32_ext_boot_signature: %u\n", bs->fat32_ext_boot_signature);
		//printf("fat32_serial_num: %u\n",bs->fat32_serial_num);
		printf("fat32_volume_label: %u ", bs->fat32_volume_label[0]);
		for (i = 1; i < sizeof(bs->fat32_volume_label); i++)
			printf("%u ", bs->fat32_volume_label[i]);
		ln();
		printf("fat32_file_system_type: %c", bs->fat32_file_system_type[0]);
		for (i = 1; i < sizeof(bs->fat32_file_system_type); i++)
			printf("%c", bs->fat32_file_system_type[i]);
		ln();
		printf("fat32_os_boot_code32: %u ", bs->fat32_os_boot_code32[0]);
		for (i = 1; i < sizeof(bs->fat32_os_boot_code32); i++)
			printf("%u ", bs->fat32_os_boot_code32[i]);
		ln();
		printf("fat32_boot_sector_signature: %u\n", bs->fat32_boot_sector_signature);
	}
}

void ln(void) {
	printf("\n");
}

