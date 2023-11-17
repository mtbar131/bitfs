// This code implements a BitTorrent client which downloads data from Bittorrent
// network in on demand fashion. It provides this functionality via FUSE so that
// any torrent can be accesses as a normal file on filesystem.
// This code uses Bittorrent implementation API provided by 'libtorrent-1.1.2' &
// refers API usage examples given by libtorrent.


#ifndef torrent_metadata_h
#define torrent_metadata_h

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

namespace lt = libtorrent;
namespace bfs = boost::filesystem;

class torrent_metadata {

private:

public:
  /* The path of torrent file which is mounted */
  bfs::path *torrent_file_path;

  /* Disk cache location */
  bfs::path disk_cache_path;
  
  /* Torrent handle returned by libtorrent */
  lt::torrent_handle thandle;

  /* configuration settings used for this torrent */
  lt::settings_pack sett;

  /* Parameters used while adding this torrent */
  lt::add_torrent_params params;

  /* error codes for this file */
  lt::error_code ec;

  /* mutex lock for all operations */
  boost::mutex fsmtx;

  bfs::path* getTorrentFilePath();
	
  void setTorrentFilePath(bfs::path *torrentFilePath);
	
  bfs::path getDiskCachePath();
	
  void setDiskCachePath(bfs::path diskFilePath);
	
  lt::torrent_handle getThandle();
	
  void setThandle(lt::torrent_handle tHandle);
	
  lt::settings_pack getSettingsPack();
	
  void setSettingsPath(lt::settings_pack settingsPack);
	
  lt::error_code getErrorCode();
	
  void setErrorCode(lt::error_code errorCode);
	
  lt::add_torrent_params getAddTorrentParams();
	
  void setAddTorrentParams(lt::add_torrent_params addTorrentParams);
	
  torrent_metadata(std::string torrent_file);  
};

#endif /*torrent_metadata_h*/
