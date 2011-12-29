#ifndef HILO_PPD_H_
#define HILO_PPD_H_

int PRAID1_PPD(void*);
int PRAID1_PPD_resolve(NIPC*, t_list*, Hilo_PPD*, char*, t_list*);
void PRAID1_hilo_sync (void*);
void PRAID1_add_sync_petition(uint32_t, Sync_PPD);
void Hilo_PPP_disk_fault(Info_PPD*, Hilo_PPD*, t_list*, t_list*);
int Equal_thread_id (void*, void*);

#endif /* HILO_PPD_H_ */
