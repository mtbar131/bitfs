// This code implements a BitTorrent client which downloads data from Bittorrent
// network in on demand fashion. It provides this functionality via FUSE so that
// any torrent can be accesses as a normal file on filesystem.
// This code uses Bittorrent implementation API provided by 'libtorrent-1.1.2' &
// refers API usage examples given by libtorrent.

#ifndef bitfs
#define bitfs

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include "bitfs_metadata.hpp"
#include "torrent_metadata.hpp"

class FS {
private: 

  static FS *_instance;
  int get_file_index(const char*);
  int get_file_size(int);
  void wait_for_piece_download(int);
  void get_piece_buffers();
  
public:
  bitfs_metadata *fsmeta;
	
  static FS *Instance();

  FS();
  ~FS();


  int getattr(const char *path, struct stat *statbuf);
  int open(const char *path, struct fuse_file_info *fileInfo);
  int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
  int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
  void* init_bitfs(struct fuse_conn_info *conn);

  
};



#endif //bitfs
