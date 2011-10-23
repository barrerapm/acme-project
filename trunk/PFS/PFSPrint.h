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
void PFSPrint_fs_info(void);
void PFSPrint_FAT_table(void);
void PFSPrint_bootsector(void);
void PFSPrint_block(struct block, int);
void PFSPrint_cluster(struct cluster, int);
void PFSPrint_sector(struct sector, int);
void PFSPrint_long_dir_entry(struct long_dir_entry*);
void PFSPrint_short_dir_entry(struct dir_entry*);
void PFSPrint_dir_entry(int, struct long_dir_entry*, struct dir_entry*);
void ln(void);

#endif /* PFSPrint_H_ */
