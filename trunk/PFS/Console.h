#ifndef CONSOLE_H_
#define CONSOLE_H_

void Console_PFS(void);
int Console_fsinfo(int n_params, char **params);
int Console_finfo(int n_params, char **params);
int Console_create(int n_params, char **params);
int Console_read(int n_params, char **params);
int Console_write(int n_params, char **params);
int Console_flush(int n_params, char **params);
int Console_release(int n_params, char **params);
int Console_truncate(int n_params, char **params);
int Console_unlink(int n_params, char **params);
int Console_mkdir(int n_params, char **params);
int Console_readdir(int n_params, char **params);
int Console_rmdir(int n_params, char **params);
int Console_rename(int n_params, char **params);
int Console_help(int n_params, char **params);
int Console_bootsector(int n_params, char **params);
int Console_fs_info(int n_params, char **params);
int Console_fat(int n_params, char **params);
int Console_exit(int n_params, char **params);
int Console_entries(int n_params, char **params);

#endif /* CONSOLE_H_ */
