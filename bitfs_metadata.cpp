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
#include "bitfs_metadata.hpp"
#include "torrent_metadata.hpp"

namespace lt = libtorrent;
namespace bfs = boost::filesystem;


void bitfs_metadata::setSession(lt::session *session1){
  sess = session1;
}

lt::session* bitfs_metadata::getSession(){
  return sess;
}

void bitfs_metadata::setTMeta(torrent_metadata *tMeta){
  tmeta = tMeta;
}

torrent_metadata* bitfs_metadata::getTMeta(){
  return tmeta;
}

bitfs_metadata::bitfs_metadata(void){
  std::cout << "Bitfs metadata object is being created" << std::endl;
}
bitfs_metadata::~bitfs_metadata(void){
  std::cout << "Bitfs metadata object is being deleted" << std::endl;
}
