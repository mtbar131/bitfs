// This code implements a BitTorrent seeder which seeds to any client without any
// limits on sharing usage. Also the choke/unchok interval is set to 1 sec
// and choking algorithm is changed to 'fixed_slots_checker' so that any client
// which wants a piece will be immediately served. 
// This code uses Bittorrent implementation API provided by 'libtorrent-1.1.2'
// Referred from libtorrent example code.


#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <boost/make_shared.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert_types.hpp>


#define clear() printf("\033[H\033[J")

namespace lt = libtorrent;

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


int main(int argc, char** argv)
{
 
  lt::add_torrent_params params;
  lt::settings_pack s;
  lt::session *sess;
  char template_path[] = "/tmp/seeder-XXXXXX";
  std::string pathstr(mkdtemp(template_path));
  lt::error_code ec;
  lt::torrent_handle th;

  s.set_str(lt::settings_pack::listen_interfaces,
	    "0.0.0.0:6881");
  s.set_int(lt::settings_pack::alert_mask,
	    lt::alert::peer_log_notification);
  s.set_bool(lt::settings_pack::strict_end_game_mode, false);
  // s.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, true);
  s.set_bool(lt::settings_pack::smooth_connects, false);
  s.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::fixed_slots_choker);
  s.set_int(lt::settings_pack::seed_choking_algorithm, lt::settings_pack::round_robin);
  s.set_int(lt::settings_pack::unchoke_slots_limit, -1);
  s.set_int(lt::settings_pack::max_allowed_in_request_queue, 9999999);
  s.set_int(lt::settings_pack::connection_speed, 10000);
  /* set lower unchoke interval so that torrent is seeded constantly */
  //s.set_int(lt::settings_pack::unchoke_interval, 1);
  s.set_int(lt::settings_pack::optimistic_unchoke_interval, 1);

  s.set_int(lt::settings_pack::send_buffer_low_watermark, 1024*1024);
  s.set_int(lt::settings_pack::send_buffer_watermark, 1024*1024);
  s.set_int(lt::settings_pack::send_buffer_watermark_factor, 512);

  s.set_int(lt::settings_pack::seeding_piece_quota, 2000);
  s.set_int(lt::settings_pack::num_optimistic_unchoke_slots, 20);
  // s.set_int(lt::settings_pack::download_rate_limit, 0);
  // s.set_int(lt::settings_pack::upload_rate_limit, 0);
  /* For automanaged torrents */
  s.set_int(lt::settings_pack::active_seeds, 512);
  s.set_int(lt::settings_pack::auto_manage_interval, 1);
  s.set_int(lt::settings_pack::seed_time_limit, 1024 * 60 * 60);
  s.set_bool(lt::settings_pack::enable_dht, false);
  // int flags =
  // 	libtorrent::session::add_default_plugins |
  // 	libtorrent::session::start_default_features;
  sess = new lt::session(s);



  params.save_path = pathstr;

  params.ti = boost::make_shared<lt::torrent_info>(std::string(argv[1]), boost::ref(ec), 0);
  if (ec) {
    fprintf(stderr, "%s\n", ec.message().c_str());
    return 1;
  }
  
  th = sess->add_torrent(params, ec);
  if (ec) {
    fprintf(stderr, "%s\n", ec.message().c_str());
    return 1;
  }

  // wait for the user to end
  while (1) {
    lt::torrent_status const& stats = th.status();
    std::cout << "Torrent status: " << stat_to_string(stats.state) << std::endl;
    std::cout << "Torrent progress: " << stats.progress * 100 << "%\n";
    double total_download = stats.total_download / 1024;
    double total_upload = stats.total_upload / 1024;
    std::cout << "Total downloaded: " << total_download << " KB (" << total_download / 1024 << " MB)\n";
    std::cout << "Total uploaded: " << total_upload << " KB (" << total_upload / 1024 << " MB)\n";
    std::cout << "Upload rate: " << stats.upload_rate << " Bytes per second\n";
    std::cout << "Total seeders: " << stats.num_seeds << std::endl;
    std::cout << "Total Peers: " << stats.num_peers << std::endl;

    std::vector<lt::alert*> alerts;
    sess->pop_alerts(&alerts);
    for (lt::alert const* a : alerts) {
      std::cout << a->message() << std::endl;
    }

    std::vector<libtorrent::peer_info> infovector;
    th.get_peer_info(infovector);
    for (auto const &pinfo : infovector) {
      std::cout << "Peer: " << pinfo.ip << "\n";
      std::cout << "Peer flags: " << get_peer_info_message(pinfo.flags) << "\n";
    }
    std::cout<<"\n\n\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
  return 0;
}

