#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <ctime>
#include "torrent_metadata.hpp"
#include "bitfs_metadata.hpp"
#include "FS.hpp"

namespace lt = libtorrent;
namespace bfs = boost::filesystem;

static struct fuse_operations bitfs_operations;

int getattr_wrapper(const char *path, struct stat *statbuf) {
	return FS::Instance()->getattr(path, statbuf);
}

int open_wrapper(const char *path, struct fuse_file_info *fileInfo) {
	return FS::Instance()->open(path, fileInfo);
}

int read_wrapper(const char *path, char *buf, size_t size,
	      off_t offset, struct fuse_file_info *fileInfo) {
	return FS::Instance()->read(path, buf, size, offset, fileInfo);
}

int readdir_wrapper(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fileInfo) {
	return FS::Instance()->readdir(path, buf, filler, offset, fileInfo);
}

void* init_wrapper(struct fuse_conn_info *conn) {
	return FS::Instance()->init_bitfs(conn);
}

/* Initialize fuse operations structure to let fuse know which all
   operations are supported. Returns a parameter array to passed to fuse_main */
char** init_fuse(char **argv) {
    char **fuse_args = (char**) malloc (4 * sizeof(char*));
        
    /* Prepare arguments for fuse */
    fuse_args[0] = argv[0];
    fuse_args[1] = argv[2];
    fuse_args[2] = (char*) malloc (8);
    fuse_args[3] = (char*) malloc (8);

    /* pass DEBUG args for fuse */
    strcpy(fuse_args[2], "-d"); 
    strcpy(fuse_args[3], "-f");

    /* setup available operations */
    bitfs_operations.getattr = getattr_wrapper;
      bitfs_operations.readdir = readdir_wrapper;
    bitfs_operations.open = open_wrapper;
    bitfs_operations.read = read_wrapper;
    bitfs_operations.init = init_wrapper;

    return fuse_args;
}

int main(int argc, char *argv[]){
	
	if (argc != 4) {
        exit(0);
    }
	// torrent_metadata tm(argv[1]);
	//bitfs_metadata bitfs;
	// bitfs.setTMeta(&tm);	
	
	// std::cout<<*(bitfs.getTMeta())->getTorrentFilePath()<<" is the file mount point"<<std::endl;
	// std::cout<<*tm.getTorrentFilePath()<<" is also the file mount point"<<std::endl;
	// std::cout<<(bitfs.getTMeta())->getDiskCachePath()<<" is the disk cache file path"<<std::endl;
	// std::cout<<tm.getDiskCachePath() <<" is also the disk cache path"<<std::endl;

	fuse_main(4, init_fuse(argv), &bitfs_operations, (void*) argv[1]);

	return 0;
}

