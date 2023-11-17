#-D_FILE_OFFSET_BITS=64 is required for fuse
# -lboost_system -ltorrent is required for libtorrent
# -std=c++11 is required to use c++ version 11
CFLAGS=-lboost_system -lboost_filesystem -ltorrent -std=c++11 `pkg-config fuse --cflags --libs` -Wall


# add library path for libtorrent and libboost
CPP=LD_LIBRARY_PATH=/usr/local/lib/ g++

GOALS=client seeder

EXECUTABLES=client seeder


all: $(GOALS)


client: FS.cpp client.cpp torrent_metadata.cpp torrent_metadata.hpp bitfs_metadata.cpp bitfs_metadata.hpp FS.hpp
	$(CPP) FS.cpp torrent_metadata.cpp bitfs_metadata.cpp  client.cpp -o client $(CFLAGS)

seeder: seeder.cpp
	$(CPP) seeder.cpp -o seeder $(CFLAGS)

clean:
	rm *~ *.o $(EXECUTABLES)
