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
#include "FS.hpp"
FS* FS::_instance = NULL;

std::set<std::string> dirmap;
std::set<std::string> filenames;
std::map<std::string, std::set<std::string>> entries;
std::map<int, char*> piece_buffer_map;


#define RETURN_ERRNO(x) (x) == 0 ? 0 : -errno

FS* FS::Instance() {
  if(_instance == NULL) {
    _instance = new FS();
  }
  return _instance;
}


FS::FS() {

}

FS::~FS() {

}

bool has_file(const char* path) {
  std::string strpath(path);
  return std::find(filenames.begin(), filenames.end(), strpath) != filenames.end();
}

bool has_dir(const char* path) {
  std::string strpath(path);
  return dirmap.find(strpath) != dirmap.end();    
}

int FS::getattr(const char *path, struct stat *stbuf) {
	
  memset(stbuf, 0, sizeof(struct stat));

  std::string strpath(path);
  if (strcmp(path, "/") != 0)
    strpath += "/";
  if (has_dir(strpath.c_str())) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (has_file(strpath.c_str())) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = get_file_size(get_file_index(path));
  } else {
    return -ENOENT;
  }

  return 0;
}

int FS::open(const char *path, struct fuse_file_info *fileInfo) {
  std::string strpath(path);
  strpath += "/";
  if (!has_file(strpath.c_str()))
    return -ENOENT;
    
  return 0;
}


/* Return index of given filename in file_storage array. If file not found
   returns -1 */
int FS::get_file_index(const char *filepath) {
  int idx = -1;
  boost::shared_ptr<lt::torrent_info> ti = fsmeta->getTMeta()->getAddTorrentParams().ti;
  lt::file_storage files = ti->orig_files();
  for (int i = 0; i < ti->num_files(); i++) {
    if (std::strcmp(filepath + 1, files.file_path(i).c_str()) == 0) {
      idx = i;
      break;
    }
  }
  return idx;
}

int FS::get_file_size(int index) {
  boost::shared_ptr<lt::torrent_info> ti = fsmeta->getTMeta()->getAddTorrentParams().ti;
  lt::file_storage files = ti->orig_files();
  return files.file_size(index);
}


void FS::get_piece_buffers() {

  std::vector<lt::alert*> alerts;
  fsmeta->getSession()->pop_alerts(&alerts);

  for (lt::alert const* a : alerts) {
    //std::cout << a->message() << std::endl;
    if (lt::alert_cast<lt::read_piece_alert>(a)) {
      char *piece_buf = (char*) malloc (((lt::read_piece_alert*)a)->size);
      memcpy(piece_buf, ((lt::read_piece_alert*)a)->buffer.get(),
	     ((lt::read_piece_alert*)a)->size);
      piece_buffer_map[((lt::read_piece_alert*)a)->piece] =
	piece_buf;
    }
  }

}

void FS::wait_for_piece_download(int piece) {

  std::cout << " Waiting for piece " << piece << std::endl;
  /* First check if piece is already downloaded */
  if(!fsmeta->tmeta->thandle.have_piece(piece)){

    //fsmeta->tmeta->thandle.piece_priority(piece, 7);
    volatile bool hve_piece = fsmeta->tmeta->thandle.have_piece(piece);
    while (!hve_piece) {
      get_piece_buffers();
      hve_piece = fsmeta->tmeta->thandle.have_piece(piece);
    }
  }
  std::cout << " Download finished for piece " << piece << std::endl; 
}

int FS::read(const char *filepath, char *buf, size_t _size,
	     off_t _offset, struct fuse_file_info *fileInfo) {
  lt::torrent_handle thandle = fsmeta->tmeta->thandle;
  boost::shared_ptr<const lt::torrent_info> ti = thandle.torrent_file();

  int file_index = get_file_index(filepath);
  int file_total_size = get_file_size(file_index);
  int size = (int) _size;
  int offset = (int) _offset;
  lt::peer_request pr;
  int piece_length = ti->piece_length();
  char temp_buf[piece_length];
  int read_ptr = 0;
    
  std::cout << "bitFS read operation "                   \
	    << "\tRequested data starts at : " << _offset  \
    /*     << "\tActual File size: " << file_total_size             \*/
    /* << "\tRequested file : " << filepath                 \ */
    /* << "\tFile index: " << file_index                    \ */
	    << "\tRequested size: " << _size << std::endl;

  if(_offset >= file_total_size)
    return 0;


  /* Get size of one piece and see if read request spans over multiple pieces */
  int piece_size;;
  int curr_offset = offset;
  int remaining_size = std::min(size, file_total_size - offset);

  /* For efficiency calculate all piece first and update their
     priority. */
  std::map<int,bool> required_pieces;
  int required_count;
  while (remaining_size > 0) {
    pr = ti->map_file(file_index, curr_offset, remaining_size);
    piece_size = ti->piece_size(pr.piece);

    if (!thandle.have_piece(pr.piece)) {
      required_pieces[pr.piece] = false;
      std::cout << "adding piece " << pr.piece << std::endl;
      required_count++;

      // set highest priority for this piece
      fsmeta->tmeta->thandle.piece_priority(pr.piece, 7);

      if (pr.piece > 1)
	fsmeta->tmeta->thandle.piece_priority(pr.piece - 1, 1);
      if (pr.piece < ti->num_pieces() - 1)
	fsmeta->tmeta->thandle.piece_priority(pr.piece + 1, 7);

    }
        
    // do calculations & go to next piece
    int read_size = std::min(piece_size - pr.start, remaining_size);
    curr_offset += read_size;
    remaining_size -= read_size;
  }

  fsmeta->tmeta->fsmtx.lock();

  /* added an extra loop to go on and read all available pieces
     This might imporve read time */
  for (auto const &p : required_pieces) {
    std::cout << " piece " << p.first;
    if (fsmeta->tmeta->thandle.have_piece(p.first)) {
      fsmeta->tmeta->thandle.read_piece(p.first);    
    }
  }   

  /* Get size of one piece and see if read request spans over multiple pieces */
  curr_offset = offset;
  remaining_size = std::min(size, file_total_size - offset);
    
  while (remaining_size > 0) {
    pr = ti->map_file(file_index, curr_offset, remaining_size);
    piece_size = ti->piece_size(pr.piece);
        
    // Download that piece
    wait_for_piece_download(pr.piece);

    // read that piece
    volatile bool has_buff = (piece_buffer_map.find(pr.piece) != piece_buffer_map.end());
    if (!has_buff)
      fsmeta->tmeta->thandle.read_piece(pr.piece);
    while (!has_buff) {
      get_piece_buffers();
      has_buff = piece_buffer_map.find(pr.piece) != piece_buffer_map.end();
    }

    memcpy(temp_buf, piece_buffer_map[pr.piece], piece_size);
        
    // copy correct data into buf
    int read_size = std::min(piece_size - pr.start, remaining_size);
    memcpy(buf + read_ptr, temp_buf + pr.start, read_size);
        
    // do calculations & go to next piece
    curr_offset += read_size;
    remaining_size -= read_size;
    read_ptr += read_size;
  }

  fsmeta->tmeta->fsmtx.unlock();
    
  return read_ptr;
}

int FS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	
  (void) offset;
  (void) fi;

  std::string strpath(path);
  if (strcmp(path, "/") != 0)
    strpath = strpath + "/";

  if (!has_dir(strpath.c_str()))
    return -ENOENT;

  for (auto const &str : entries[strpath]) {
    filler(buf, str.c_str(), NULL, 0);
  }

  return 0;
}

void* FS::init_bitfs(struct fuse_conn_info *conn) {

  char *torrent_file_path = (char*) fuse_get_context()->private_data;
  std::cout << "Torrent file: " << torrent_file_path << "\n";
  fsmeta = new bitfs_metadata();
  fsmeta->tmeta = new torrent_metadata(torrent_file_path);
  torrent_metadata *tmeta = fsmeta->tmeta;

  /* set listen interfaces */
  tmeta->sett.set_str(lt::settings_pack::listen_interfaces,
		      "0.0.0.0:6881");
  tmeta->sett.set_bool(lt::settings_pack::strict_end_game_mode, false);

  // /* set alerts for this torrent */
  // lt::alert::progress_notification |
  tmeta->sett.set_int(lt::settings_pack::alert_mask,
		      lt::alert::progress_notification |
		      lt::alert::peer_log_notification);
  tmeta->sett.set_bool(lt::settings_pack::enable_dht, false);
  tmeta->sett.set_bool(lt::settings_pack::smooth_connects, false);
  tmeta->sett.set_int(lt::settings_pack::connection_speed, 10000);

  tmeta->sett.set_int(lt::settings_pack::choking_algorithm,
		      lt::settings_pack::fixed_slots_choker);
  tmeta->sett.set_int(lt::settings_pack::unchoke_slots_limit, 1000);

  //tmeta->sett.set_int(lt::settings_pack::unchoke_interval, 1);
  tmeta->sett.set_int(lt::settings_pack::optimistic_unchoke_interval, 1);

  tmeta->sett.set_int(lt::settings_pack::num_optimistic_unchoke_slots, 20);
    
  tmeta->sett.set_int(lt::settings_pack::auto_manage_interval, 1);
    
  tmeta->sett.set_int(lt::settings_pack::min_reconnect_time, 2);

  // tmeta->sett.set_int(lt::settings_pack::max_out_request_queue, 99999);
  // tmeta->sett.set_int(lt::settings_pack::connection_speed, 10000);
  // tmeta->sett.set_int(lt::settings_pack::download_rate_limit, 0);
  // tmeta->sett.set_int(lt::settings_pack::upload_rate_limit, 0);
    

  fsmeta->sess = new lt::session(tmeta->sett);

    
  // tmeta->sett.set_bool(lt::settings_pack::strict_end_game_mode, false);
  // tmeta->sett.set_bool(lt::settings_pack::announce_to_all_trackers, true);
  // tmeta->sett.set_bool(lt::settings_pack::announce_to_all_tiers, true);

  // // /* set alerts for piece download */
  // // fsmeta->sess->set_alert_notify(alert_handler);
        
  // /* set save path for pieces of this torrent - Cache */
  // tmeta->params.save_path = tmeta->disk_cache_path.string();
  //tmeta->params.flags &= ~libtorrent::add_torrent_params::flag_auto_managed;
  tmeta->params.save_path = tmeta->disk_cache_path.string();
  //tmeta->params.flags = libtorrent::add_torrent_params::flag_paused;

  /* setup torrent info object */
  tmeta->params.ti = boost::make_shared<lt::torrent_info>(
							  std::string(tmeta->torrent_file_path->string()),
							  boost::ref(tmeta->ec), 0);
  if (tmeta->ec) {
    fprintf(stderr, "Error:%s\n", tmeta->ec.message().c_str());
    return NULL;
  }

  /* add torrent to session */
  tmeta->thandle = fsmeta->sess->add_torrent(tmeta->params, tmeta->ec);
  if (tmeta->ec) {
    fprintf(stderr, "Error:%s\n", tmeta->ec.message().c_str());
    return NULL;
  }

  //std::this_thread::sleep_for(std::chrono::milliseconds(100));

  /* Don't immediately start downloading */

  /* Fill in directory map */
  std::vector<std::string> strs;
  lt::file_storage files = tmeta->params.ti->orig_files();
  for (int i = 0; i < tmeta->params.ti->num_files(); i++) {
    /* Set lowest priority for all files */
    std::string path = files.file_path(i);
    std::string dirpath = "/";
    boost::split(strs, path, boost::is_any_of("/"));
    int j;
    dirmap.insert("/");
    for(j = 0; j != strs.size() - 1; j++) {
      entries[dirpath].insert(strs[j]);
      dirpath = dirpath + strs[j];
      dirpath = dirpath  + "/";
      dirmap.insert(dirpath);
    }
    filenames.insert(dirpath + strs[j] + "/");
    entries[dirpath].insert(strs[j]);
  }


  /* set piece_priority to zero for all piece */
  for (int i = 10; i < tmeta->params.ti->num_pieces(); i++) {
    tmeta->thandle.piece_priority(i, 0);
  }    
    

  //tmeta->thandle.set_download_limit(1024 * 1024);
    
  // /* Print torrent data */
    
  // for (auto const &str : filenames) {
  //     std::cout << str << "\n";
  // }

  // for (auto const &ent : entries) {
  //     std::cout << ent.first << "\n";
  //     for (auto const &str : ent.second) {
  //         std::cout << "\t" << str << "\n";
  //     }
  // }        

  //tmeta->thandle.resume();
    
  std::cout << "Piece length: " << tmeta->params.ti->piece_length() \
	    << "\nNumber of pieces: " << tmeta->params.ti->num_pieces() \
	    << "\nCache path: " << tmeta->params.save_path  \
	    << std::endl;

  return NULL;
}

std::string stat_to_string(int num) {
  switch (num) {
  case 1: return "checking_files";
  case 2: return "downloading_metadata";
  case 3: return "downloading";
  case 4: return "finished";
  case 5: return "seeding";
  case 6: return "allocating";
  case 7: return "checking_resume_data";
  default:
    break;
  }
  return "ERROR";
}

std::string get_peer_info_message(int index) {
  std::string message;
  if (index & 1)
    message +=  "we are interested in pieces from this peer || ";
  if (index & 2)
    message +=  "we have choked this peer || ";
  if (index & 4)
    message +=  "the peer is interested in us || ";
  if (index & 8)
    message +=  "the peer has choked us || ";
  if (index & 16)
    message +=  "peer supports the extension protocol. || ";
  if (index & 32)
    message +=  "The connection was initiated by us, the peer has a listen port open, and that port is the same as in the address of this peer. If this flag is not set, this peer connection was opened by this peer connecting to us. || ";
  if (index & 64)
    message +=  "The connection is opened, and waiting for the handshake. Until the handshake is done, the peer cannot be identified. || ";
  if (index & 128)
    message +=  "The connection is in a half-open state (i.e. it is being connected). || ";
  if (index & 512)
    message +=  "The peer has participated in a piece that failed the hash check, and is now on parole, which means we're only requesting whole pieces from this peer until it either fails that piece or proves that it doesn't send bad data. || ";
  if (index & 1024)
    message +=  "This peer is a seed (it has all the pieces). || ";
  if (index & 2048)
    message +=  "This peer is subject to an optimistic unchoke. It has been unchoked for a while to see if it might unchoke us in message +=  an earn an upload/unchoke slot. If it doesn't within some period of time, it will be choked and another peer will be optimistically unchoked. || ";
  if (index & 4096)
    message +=  "This peer has recently failed to send a block within the request timeout from when the request was sent. We're currently picking one block at a time from this peer. || ";
  if (index & 8192)
    message +=  "This peer has either explicitly (with an extension) or implicitly (by becoming a seed) told us that it will not downloading anything more, regardless of which pieces we have. || ";
  if (index & 16384)
    message +=  "This means the last time this peer picket a piece, it could not pick as many as it wanted because there were not enough free ones. i.e. all pieces this peer has were already requested from other peers. || ";
  if (index & 32768)
    message +=  "This flag is set if the peer was in holepunch mode when the connection succeeded. This typically only happens if both peers are behind a NAT and the peers connect via the NAT holepunch mechanism. || ";
  if (index & 65536)
    message +=  "indicates that this socket is runnin on top of the I2P transport. || ";
  if (index & 131072)
    message +=  "indicates that this socket is a uTP socket || ";
  if (index & 262144)
    message +=  "indicates that this socket is running on top of an SSL (TLS) channel || ";
  if (index & 1048576)
    message +=  "this connection is obfuscated with RC4 || ";
  if (index & 2097152)
    message +=  "the handshake of this connection was obfuscated with a diffie-hellman exchange || ";
  return message;
}
