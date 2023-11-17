




// void print_download_info(lt::torrent_handle th) {
//   std::vector<partial_piece_info> data;
//   th.get_download_queue(data);
//   std::cout << "Partial piece info" << std::endl;
//   for (std::vector<partial_piece_info>::iterator i = data.begin(),
//          end(data.end()); i != end; ++i) {
//     std::cout << i->piece_index << "\n";
//   }
// }

// void bitfs_piece_finished_alert_handler() {
//   std::vector<lt::alert*> alerts;
//   s->pop_alerts(&alerts);

//   for (lt::alert const* a : alerts) {
//     std::cout << a->message() << std::endl;
//   }
// }

// int main(int argc, char* argv[])
// {
//   if (argc != 2)
// 	{
//       fputs("usage: ./simple_client torrent-file\n"
// 			"to stop the client, press return.\n", stderr);
//       return 1;
// 	}

//   settings_pack sett;
//   sett.set_str(settings_pack::listen_interfaces, "0.0.0.0:6881");
//   sett.set_int(lt::settings_pack::alert_mask,
//                lt::alert::progress_notification);
//   s = new lt::session(sett);

//   /* Set pieced download finished alert */
//   s->set_alert_notify(bitfs_piece_finished_alert_handler);
  
//   add_torrent_params p;
//   p.save_path = "/tmp";
//   error_code ec;
//   p.ti = boost::make_shared<torrent_info>(std::string(argv[1]), boost::ref(ec), 0);
//   if (ec)
// 	{
//       fprintf(stderr, "%s\n", ec.message().c_str());
//       return 1;
// 	}
//   lt::torrent_handle th = s->add_torrent(p, ec);
//   /* This will add the torrent and make it ready but won't start
//      donwloading unless we ask it to */
//   //  th.stop_when_ready(true);
//   if (ec)
// 	{
//       fprintf(stderr, "%s\n", ec.message().c_str());
//       return 1;
// 	}

//   std::this_thread::sleep_for(std::chrono::milliseconds(10000));
//   print_download_info(th);
//   /* Set priority for 101th piece to max (i.e. 7) */
//   th.piece_priority(101, 7);
//   th.piece_priority(102, 7);
//   th.piece_priority(103, 7);
//   //  th.resume();
//   std::this_thread::sleep_for(std::chrono::milliseconds(30000));
//   print_download_info(th);
//   return 0;
// }

