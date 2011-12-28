#ifndef AddressPFS_H_
#define AddressPFS_H_

int AddressPFS_read_cluster(Cluster*, int);
int AddressPFS_write_cluster(int , Cluster* );
int AddressPFS_read_block(int, Block*);
int AddressPFS_write_block(int, Block*);


#endif /* AddressPFS_H_ */
