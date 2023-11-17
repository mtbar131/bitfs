#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "bt_lib.h"

int main(int argc, char **argv) {
    if (argc != 2) {
	exit(0);
    }

    // first read the the torrent file as a binary string
    metainfo_t *metainfo = parse_metainfo_file(argv[1]);
    
    printf("announce: %s\n", metainfo->announce);
    printf("piece length: %d\n", metainfo->info->piece_length);
    printf("file name: %s\n", metainfo->info->filename);
    printf("file length: %d\n", metainfo->info->file_length);
    printf("Piece count: %d\n", metainfo->info->num_pieces);

    // read ip list and connect to peers
    peer_list *pl = get_peer_list();

    // handshake with peer
    char *delim = ":";
    char *ip = strtok(pl->infoarr[0].url, delim);
    int port = atoi(strtok(NULL, delim));
    int sockfd = peer_tcp_connect(ip, port);
    char outbuf[1024];
    char mbuf[1024];
    handshake_msg_t *hresponse;
    bt_msg_t *response;
    hresponse = bt_handshake(sockfd, metainfo);

    strcpy(outbuf, "Peer ID: ");
    strncat(outbuf, hresponse->peerid, 21);
    printf("%s\n", outbuf);

    response = bt_send_message(sockfd, UNCHOKE, metainfo);

    response = bt_send_message(sockfd, INTERESTED, metainfo);

    // read bitfield messag
    read(sockfd, mbuf, metainfo->info->num_pieces / 8 + 1 + 5);

	
    //char *data = get_piece(sockfd, 0, metainfo);
    //get_piece(sockfd, 0, metainfo);
    //seeder is sending unchoke message - handle that
    int fd = open("/tmp/file.txt", O_RDWR | O_CREAT);
    int i;
    char *data;
    for (i = 0; i < 1/*metainfo->info->num_pieces*/; i++) {
    	data = get_piece(sockfd, i, metainfo);
    	write(fd, data, piece_size(i, metainfo));
    }
    close(fd);
    return 0;
}

