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

std::set<std::string> dirmap;
std::set<std::string> filenames;
std::map<std::string, std::set<std::string>> entries;
std::map<int, char*> piece_buffer_map;

typedef struct torrent_metadata {

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

public:
    torrent_metadata(std::string torrent_file) {
        torrent_file_path = new bfs::path(torrent_file);
        disk_cache_path = bfs::unique_path("/tmp/%%%%-%%%%-%%%%-%%%%"); 
    }
  
} torrent_metadata;


typedef struct bitfs_metadata {

    /* session object for libtorrent - same object for all torrents */
    lt::session *sess;

    /* metadata of torrent file mounted in this session */
    torrent_metadata *tmeta;
        
} bitfs_metadata;

typedef struct session_stat{

	/*Session start time*/
	std::time_t start_time;
	
	/*Session end time*/
	std::time_t end_time;
	
	/*Pieces downloaded*/
	int piece_download_counter;	

	/*Total download size*/
	int download_size;

	/*Total file size */
	int file_size;

	/*Cache hit counter*/
	int cache_hit_counter;

	/*Cache hit rate*/
	double cache_hit_rate;

    /*Bandwidth utilization*/
	//double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	
} session_stat;


static struct fuse_operations bitfs_operations;

bitfs_metadata *fsmeta;

session_stat stats;

int enable_caching = 0;

void initialize_stats(){
	std::cout<<"In initialize stats function"<<std::endl;
	stats.start_time =  time(0);
	stats.cache_hit_counter = 0;
	stats.piece_download_counter = 0;
	stats.file_size = fsmeta->tmeta->params.ti->total_size();
	std::cout<<"Initialization successful"<<std::endl;

}

void display_stats(){
		stats.end_time = time(0);
		double session_time = stats.end_time - stats.start_time;
		double download_size = stats.piece_download_counter *  fsmeta->tmeta->params.ti->piece_length() / 1024 / 1024;
		double bandwidth_utilization = download_size  / session_time;
		stats.cache_hit_rate = stats.cache_hit_counter * 100 / stats.piece_download_counter;

	    	std::cout << "Session statistics " << std::endl ;  
		std::cout << "-----------------------------------" << std::endl ;
              	std::cout<< "Start time : " << ctime(&stats.start_time) << std::endl ;
             	std::cout<< "End time : " << ctime(&stats.end_time) << std::endl ;
		std::cout<< "Session Time : " << session_time << " seconds" << std::endl ; 
	 	std::cout<< "Pieces Downloaded : " << stats.piece_download_counter << std::endl ;
		std::cout<< "Total Download Size : " << download_size << "MB"<< std::endl ;
		std::cout<< "Total File Size : " << stats.file_size / 1024 / 1024 << " MB"<< std::endl ;
		if(enable_caching){
			std::cout<< "Cache hit counter : "<< stats.cache_hit_counter << std::endl ;
			std::cout<< "Cache hit rate : " << stats.cache_hit_rate << " % " << std::endl ;
		}		
		std::cout<< "Network utilization : " << bandwidth_utilization << " MBPS"<< std::endl;

}


bool has_file(const char* path) {
    std::string strpath(path);
    return std::find(filenames.begin(), filenames.end(), strpath) != filenames.end();
}

bool has_dir(const char* path) {
    std::string strpath(path);
    return dirmap.find(strpath) != dirmap.end();    
}

static int bitfs_getattr(const char *path, struct stat *stbuf) {

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
        stbuf->st_size = fsmeta->tmeta->params.ti->total_size();
    } else {
        return -ENOENT;
    }

    return 0;
}

static int bitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
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

/* Return index of given filename in file_storage array. If file not found
   returns -1 */
static int get_file_index(const char *filepath) {
    int idx = -1;
    boost::shared_ptr<lt::torrent_info> ti = fsmeta->tmeta->params.ti;
    lt::file_storage files = ti->orig_files();
    for (int i = 0; i < ti->num_files(); i++) {
        if (std::strcmp(filepath + 1, files.file_path(i).c_str()) == 0) {
            idx = i;
            break;
        }
    }
    return idx;
}

static int get_file_size(int index) {
    boost::shared_ptr<lt::torrent_info> ti = fsmeta->tmeta->params.ti;
    lt::file_storage files = ti->orig_files();
    return files.file_size(index);
}

static int bitfs_open(const char *path, struct fuse_file_info *fi)
{
    std::string strpath(path);
    strpath += "/";
    if (!has_file(strpath.c_str()))
        return -ENOENT;
    
    return 0;
}


void get_piece_buffers() {

    std::vector<lt::alert*> alerts;
    fsmeta->sess->pop_alerts(&alerts);

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


void wait_for_piece_download(int piece) {

    std::cout << " Waiting for piece " << piece << std::endl;
    /* First check if piece is already downloaded */
	if(fsmeta->tmeta->thandle.have_piece(piece)){
		stats.cache_hit_counter += 1;
		stats.piece_download_counter += 1;
	} else {
        fsmeta->tmeta->thandle.piece_priority(piece, 7);
        volatile bool hve_piece = fsmeta->tmeta->thandle.have_piece(piece);
        while (!hve_piece) {
            get_piece_buffers();
            hve_piece = fsmeta->tmeta->thandle.have_piece(piece);
        }
    }
    std::cout << " Download finished for piece " << piece << std::endl; 
}



static int bitfs_read(const char *filepath, char *buf, size_t _size, off_t _offset,
                      struct fuse_file_info *fi)
{

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
              << "\tFile index: " << file_index                    \ 
              << "\tRequested size: " << _size << std::endl;

    if(_offset >= file_total_size)
        return 0;

    fsmeta->tmeta->fsmtx.lock();

    fsmeta->tmeta->thandle.set_download_limit(1024 * 1024);

    /* Get size of one piece and see if read request spans over multiple pieces */
    int piece_size;;
    int curr_offset = offset;
    int remaining_size = std::min(size, file_total_size - offset);

    /* For efficiency calculate all piece first and update their
       priority. */
    while (remaining_size > 0) {
        pr = ti->map_file(file_index, curr_offset, remaining_size);
        piece_size = ti->piece_size(pr.piece);
        
        // set highest priority for this piece
        fsmeta->tmeta->thandle.piece_priority(pr.piece, 7);
        if (pr.piece < ti->num_pieces() - 1)
            fsmeta->tmeta->thandle.piece_priority(pr.piece + 1, 7);
        
        // do calculations & go to next piece
        int read_size = std::min(piece_size - pr.start, remaining_size);
        curr_offset += read_size;
        remaining_size -= read_size;
    }


    /* Get size of one piece and see if read request spans over multiple pieces */
    curr_offset = offset;
    remaining_size = std::min(size, file_total_size - offset);

    
    /* Resume torrent */
    //fsmeta->tmeta->thandle.resume();

    
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

    fsmeta->tmeta->thandle.set_download_limit(1);
    
    fsmeta->tmeta->fsmtx.unlock();
    
    return read_ptr;
}

void* init_bitfs(struct fuse_conn_info *con) {

    char *torrent_file_path = (char*) fuse_get_context()->private_data;
    std::cout << "Torrent file: " << torrent_file_path << "\n";
    fsmeta = new bitfs_metadata();
    fsmeta->tmeta = new torrent_metadata(torrent_file_path);
    torrent_metadata *tmeta = fsmeta->tmeta;

    /* set listen interfaces */
    tmeta->sett.set_str(lt::settings_pack::listen_interfaces,
                        "0.0.0.0:6881,0.0.0.0:6882,0.0.0.0:6883,0.0.0.0:6884");
    tmeta->sett.set_bool(lt::settings_pack::strict_end_game_mode, false);

	int flags =
		libtorrent::session::add_default_plugins |
		libtorrent::session::start_default_features;

    // /* set alerts for this torrent */
    tmeta->sett.set_int(lt::settings_pack::alert_mask,
                        lt::alert::progress_notification);
    
    tmeta->sett.set_bool(lt::settings_pack::enable_dht, false);

    
    fsmeta->sess = new lt::session(tmeta->sett, flags);
    
    // tmeta->sett.set_bool(lt::settings_pack::announce_to_all_trackers, true);
	// tmeta->sett.set_bool(lt::settings_pack::announce_to_all_tiers, true);

    // // /* set alerts for piece download */
    // // fsmeta->sess->set_alert_notify(alert_handler);
        
    // /* set save path for pieces of this torrent - Cache */
    // tmeta->params.save_path = tmeta->disk_cache_path.string();
    // tmeta->params.flags &= ~libtorrent::add_torrent_params::flag_auto_managed;
    tmeta->params.flags = libtorrent::add_torrent_params::flag_paused;
    tmeta->params.save_path = tmeta->disk_cache_path.string();

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
    
    //tmeta->thandle.auto_managed(false);
    //tmeta->thandle.stop_when_ready(true);
    /* Don't immediately start downloading */

    /* Fill in directory map */
    std::vector<std::string> strs;
    lt::file_storage files = tmeta->params.ti->orig_files();
    for (int i = 0; i < tmeta->params.ti->num_files(); i++) {
        /* Set lowest priority for all files */
        //tmeta->thandle.file_priority(i, 0);
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


    // /* Print torrent data */
    
    for (auto const &str : filenames) {
        std::cout << str << "\n";
    }

    for (auto const &ent : entries) {
        std::cout << ent.first << "\n";
        for (auto const &str : ent.second) {
            std::cout << "\t" << str << "\n";
        }
    }
    
    fsmeta->tmeta->thandle.set_download_limit(1);

    tmeta->thandle.resume();
    
    
    std::cout << "Piece length: " << tmeta->params.ti->piece_length() \
              << "Number of pieces: " << tmeta->params.ti->num_pieces() \
              << std::endl;

    return NULL;
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
    bitfs_operations.getattr = bitfs_getattr;
    bitfs_operations.readdir = bitfs_readdir;
    bitfs_operations.open = bitfs_open;
    bitfs_operations.read = bitfs_read;
    bitfs_operations.init = init_bitfs;

    return fuse_args;
}


void print_usage() {
	std::cout<<"1/0 to enable/ disable caching." <<std::endl;
    std::cout<< "Usage: ./bitFS <torrent-file-path> <mount-point> <enable-caching-option>" <<std::endl ;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        print_usage();
        exit(0);
    }

	//enable_caching = atoi(argv[3]);	
	//initialize_stats();
	
	fuse_main(4, init_fuse(argv), &bitfs_operations, (void*) argv[1]);

    //display_stats();

	return 0;
}

