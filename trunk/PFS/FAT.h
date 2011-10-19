#ifndef FAT_H_
#define FAT_H_

struct boot_sector;
struct FAT_table;

void FAT_get_file_clusters(struct boot_sector*, struct FAT_table*, int*);

#endif /* FAT_H_ */
