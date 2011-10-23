#ifndef FAT_H_
#define FAT_H_

struct boot_sector;
struct FAT_table;

void FAT_get_file_clusters(int*);
int FAT_get_free_clusters(int , int* );
void FAT_write_FAT_entry(int , int* );

#endif /* FAT_H_ */
