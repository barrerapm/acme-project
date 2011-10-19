#ifndef PFSPrint_H_
#define PFSPrint_H_

struct boot_sector;
struct block;
struct cluster;
struct sector;
struct fs_info;
struct FAT_table;
struct long_dir_entry;
struct dir_entry;

// Prints para pruebas
void PFSPrint_fs_info(struct fs_info*);
void PFSPrint_FAT_table(struct FAT_table);
void PFSPrint_bootsector(struct boot_sector*);
void PFSPrint_block(struct boot_sector*, struct block, int);
void PFSPrint_cluster(struct boot_sector*, struct cluster, int);
void PFSPrint_sector(struct sector, int);
void PFSPrint_long_dir_entry(struct long_dir_entry*);
void PFSPrint_dir_entry(struct dir_entry*);
void ln(void);

#endif /* PFSPrint_H_ */
