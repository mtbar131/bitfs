// This code implements a BitTorrent client which downloads data from Bittorrent
// network in on demand fashion. It provides this functionality via FUSE so that
// any torrent can be accesses as a normal file on filesystem.
// This code uses Bittorrent implementation API provided by 'libtorrent-1.1.2' &
// refers API usage examples given by libtorrent.


#ifndef bitfs_metadata_h
#define bitfs_metadata_h

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
namespace lt = libtorrent;
namespace bfs = boost::filesystem;

class bitfs_metadata {

private :
public :
  /* session object for libtorrent - same object for all torrents */
  lt::session *sess;

  /* metadata of torrent file mounted in this session */
  torrent_metadata *tmeta;


  bitfs_metadata();
	
  ~bitfs_metadata();

  void setSession(lt::session *session);
	
  lt::session* getSession();
	
  void setTMeta(torrent_metadata *tmeta);
	
  torrent_metadata* getTMeta();
};

#endif //bitfs_metadata_h
