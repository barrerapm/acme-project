#ifndef FUSE_H_
#define FUSE_H_

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

static struct fuse_operations operaciones =
		{ .create = FUSE_create, .open = FUSE_open, .read = FUSE_read, .write = FUSE_write, .flush = FUSE_flush, .release = FUSE_release, .truncate = FUSE_truncate, .ftruncate = FUSE_ftruncate,
				.unlink = FUSE_unlink, .mkdir = FUSE_mkdir, .readdir = FUSE_readdir, .rmdir = FUSE_rmdir, .fgetattr = FUSE_fgetattr, .getattr = FUSE_getattr, .rename = FUSE_rename, };

#endif /* FUSE_H_ */
