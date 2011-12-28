#ifndef CONSOLEPPD_H_
#define CONSOLEPPD_H_

typedef struct sector_fis {
	uint32_t cylinder;
	uint16_t sector;
} Sector_Fis;

typedef struct info_trace {
	Sector_Fis estado;
	Sector_Fis destino;
	Sector_Fis proximo;
	uint16_t sectors_per_track;
	double latency;
} Info_Trace;

int Console_info(int n_params, char** params);
int Console_clean(int n_params, char** params);
int Console_trace(int n_params, char** params);
int Console_help(int n_params, char** params);
int Console_exit(int n_params, char** params);

#endif /* CONSOLEPPD_H_ */
