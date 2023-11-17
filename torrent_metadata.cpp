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

torrent_metadata::torrent_metadata(std::string torrent_file) {
  torrent_file_path = new bfs::path(torrent_file);
  disk_cache_path = bfs::unique_path("/tmp/%%%%-%%%%-%%%%-%%%%");	
}

bfs::path* torrent_metadata::getTorrentFilePath(){
  return torrent_file_path;
}

void torrent_metadata::setTorrentFilePath(bfs::path *torrentFilePath){
  torrent_file_path = torrentFilePath;
}

bfs::path torrent_metadata::getDiskCachePath(){
  return disk_cache_path;
}
	
void torrent_metadata::setDiskCachePath(bfs::path diskFilePath){
  disk_cache_path = diskFilePath;
}
	
lt::torrent_handle torrent_metadata::getThandle(){
  return thandle;	
}
	
void torrent_metadata::setThandle(lt::torrent_handle tHandle){
  thandle = tHandle;
}
	
lt::settings_pack torrent_metadata::getSettingsPack(){
  return sett;
}
	
void torrent_metadata::setSettingsPath(lt::settings_pack settingsPack){
  sett = settingsPack;
}
	
lt::error_code torrent_metadata::getErrorCode(){
  return ec;
}
	
void torrent_metadata::setErrorCode(lt::error_code errorCode){
  ec = errorCode;
}
	
lt::add_torrent_params torrent_metadata::getAddTorrentParams(){
  return params;
}
	
void torrent_metadata::setAddTorrentParams(lt::add_torrent_params addTorrentParams){
  params = addTorrentParams;
}
