This is a very simple implementation of bittorrent protocol.
Directory structure is as follows

├── bencode
├── bt_client.c - a simple client application which uses this bittorrent protocol.
├── bt_lib.c - main functions rquired for bittorrent protocol.
├── bt_lib.h - header file for client
├── example.torrent
├── Makefile
└── peerlist - list of all peers.

References used
https://wiki.theory.org/BitTorrentSpecification
http://www.kristenwidman.com/blog/33/how-to-write-a-bittorrent-client-part-1/
http://www.kristenwidman.com/blog/71/how-to-write-a-bittorrent-client-part-2/
https://www.cs.swarthmore.edu/~aviv/classes/f12/cs43/labs/lab5/lab5.pdf

