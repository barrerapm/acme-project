#ifndef DISK_H_
#define DISK_H_

void Disk_load_bootsector(BootSector*);
void Disk_load_FSInfo(FSInfo*);
void Disk_load_FAT(TablaFAT*);

#endif /* DISK_H_ */
