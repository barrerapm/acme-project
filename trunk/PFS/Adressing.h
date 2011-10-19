#ifndef ADRESSING_H_
#define ADRESSING_H_

struct boot_sector;

void Adressing_read_cluster(struct boot_sector*, struct cluster*, int);
void Adressing_read_block(struct boot_sector*, struct block*, int);

#endif /* ADRESSING_H_ */
