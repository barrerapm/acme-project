#ifndef ADRESSING_H_
#define ADRESSING_H_

struct boot_sector;

void Adressing_read_cluster(struct cluster*, int);
void Adressing_read_block(struct block*, int);
void Adressing_close_disk(void);
int Adressing_open_disk(void);


#endif /* ADRESSING_H_ */
