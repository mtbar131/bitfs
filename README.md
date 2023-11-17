# bitFS
A Bittorrent File System.

bitFS is a Bittorrent based mountable filesystem. Any torrent can be mounted
using bitFS and can be accessed via POSIX file system calls. bitFS downloads
data of a file from BitTorrent network in on demand fashion. As soon as a file
read request is received (in fuse) bitFS will initiate download of that data.
bitFS is a read-only file system.


bitFS uses libtorrent-1.1.2, libfuse-2.7 and C++ boost-1.63 libraries.

libtorrent provides the implementation of Bittorrent protocol (which mainly involves,
connecting to tracker, connecting to peers and downloading content from peers
using tcp sockets following BitTorrent protocol specification)

libfuse allows providing custom implementation for POSIX calls

boost libraries are mainly used because libtorrent is dependent on them.

Links
libtorrent : https://github.com/arvidn/libtorrent
libfuse: https://github.com/libfuse/libfuse
Boost: http://www.boost.org/

Other references used:
https://code.google.com/archive/p/fuse-examplefs/
http://libtorrent.org/tutorial.html

Even though current implementation of bitFS uses libtorrent for
Bittorrent protocol implementation, we did write our own very simple
implementation of BitTorrent protocol which can be found in 'btclient' directory.
This implementation was not used in bitFS because it was yet not sophisticated enough to
handle multi-threaded socekts for creating connections to multiple seeders, this in turn
slows down torrent download speed which in turns causes bad performance.

Our own implementation of bitTorrent protocol uses only 1 library for decoding binary
encoded files (viz. .torrent files).
heapless-bencode: https://github.com/willemt/heapless-bencode


Project Directory structure is as follows:
.
├── bitfs_metadata.cpp - Handles bitFS metadata functions
├── bitfs_metadata.hpp
├── client.cpp  - A client code which runs fuse_main
├── FS.cpp - The class which provides implementation for fuse POSIX calls
├── FS.hpp
├── Makefile
├── notes.txt
├── README.md
├── seeder.cpp - Seeder written to seed without any limits
├── tests - small tests
│   └── test1.c
├── torrent_metadata.cpp - A class that handles torrent_metadata
└── torrent_metadata.hpp

'btclient' directory contains our own implementation of bittorrent protocol.

├── btclient
│   ├── bencode
│   ├── bt_client.c
│   ├── bt_lib.c
│   ├── bt_lib.h
│   ├── example.torrent
│   ├── Makefile
│   └── peerlist

