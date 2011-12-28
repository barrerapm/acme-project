#ifndef FUSE_H_
#define FUSE_H_


#define FUSE_USE_VERSION 25
#include <fuse.h>

int FUSE_create(const char *path, mode_t mode, struct fuse_file_info *info);
int FUSE_open(const char *path, struct fuse_file_info *info);
int FUSE_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info);
int FUSE_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info);
int FUSE_flush(const char *path, struct fuse_file_info *info);
int FUSE_release(const char *path, struct fuse_file_info *info);
int FUSE_truncate(const char *path, off_t offset);
int FUSE_ftruncate(const char *path, off_t offset, struct fuse_file_info *info);
int FUSE_unlink(const char *path);
int FUSE_mkdir(const char *path, mode_t mode);
int FUSE_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info);
int FUSE_rmdir(const char *path);
int FUSE_getattr(const char *path, struct stat *st);
int FUSE_fgetattr(const char *path, struct stat *st, struct fuse_file_info *info);
int FUSE_rename(const char *src, const char *dest);

#endif /* FUSE_H_ */
