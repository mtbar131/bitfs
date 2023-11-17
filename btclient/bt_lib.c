#include "bt_lib.h"

static int min(int a, int b) {
    if (a > b)
	return b;
    return a;
}

int piece_size(int piece, metainfo_t *metainfo) {
    if (piece >= metainfo->info->num_pieces)
	return -1;
    int file_len = metainfo->info->file_length;
    int piece_len = metainfo->info->piece_length;
    return min(file_len - piece_len * piece, piece_len);
}

static void parse_metainfo(metainfo_t *metainfo, bencode_t *value,
			   const char *key) {
    if (strcmp(key, "announce") == 0) {
	int len;
	const char *url;
	bencode_string_value(value, &url, &len);
	metainfo->announce = (char*) malloc (len);
	strncpy(metainfo->announce, url, len);
	metainfo->announce[len] = 0;
    } else if (strcmp(key, "info") == 0) {
	int info_dict_len;
	const char *start;
	bencode_dict_get_start_and_len(value, &start, &info_dict_len);
	char *dict_str = (char*) malloc (info_dict_len + 1);
	memcpy(dict_str, start, info_dict_len);
	metainfo->info->infohash = getSHA1(dict_str, info_dict_len);
	while (bencode_dict_has_next(value)) {
	    const char *tempkey;
	    int templen;
	    bencode_t *tempvalue = (bencode_t*) malloc (sizeof(bencode_t));
	    bencode_dict_get_next(value, tempvalue, &tempkey, &templen);
	    char *keybuf = (char*) malloc (templen + 1);
	    strncpy(keybuf, tempkey, templen);
	    keybuf[templen] = 0;
	    parse_metainfo(metainfo, tempvalue, keybuf);
	}
	metainfo->info->num_pieces = (int) ceil((double) metainfo->info->file_length /
						(double) metainfo->info->piece_length);
    } else if (strcmp(key, "piece length") == 0) {
	long int plen;
	bencode_int_value(value, &plen);
	metainfo->info->piece_length = plen;
    } else if (strcmp(key, "pieces") == 0) {
	const char *piece_hashes;
	int len;
	bencode_string_value(value, &piece_hashes, &len);
	metainfo->info->piece_hashes = (char*) malloc (len);
	strncpy(metainfo->info->piece_hashes, piece_hashes, len);
    } else if (strcmp(key, "name") == 0) {
	const char *name;
	int len;
	bencode_string_value(value, &name, &len);
	metainfo->info->filename = (char*) malloc (len);
	strncpy(metainfo->info->filename, name, len);
    } else if (strcmp(key, "length") == 0) {
	long int plen;
	bencode_int_value(value, &plen);
	metainfo->info->file_length = plen;
    }
}


metainfo_t* parse_metainfo_file(char *filepath) {
    struct stat stat_buf;
    stat(filepath, &stat_buf);
    char *metainfo_buffer = (char*) malloc ((int) (stat_buf.st_size + 1));
    int metainfo_len = 0;
    char read_buf[1024];
    int fd = open(filepath, O_RDONLY);
    if (fd <= 0) {
	perror("Unable to open metainfo file. Exiting\n");
	exit(0);
    }

    int read_count = 0;
    while ((read_count = read(fd, read_buf, 1024)) > 0) {	
    	memcpy(metainfo_buffer + metainfo_len, read_buf, read_count);
    	metainfo_len += read_count;
    }
    metainfo_buffer[metainfo_len] = 0;

    bencode_t *bendata = (bencode_t*) malloc (sizeof(bencode_t));
    bencode_init(bendata, metainfo_buffer, metainfo_len);
    
    metainfo_t *metainfo = (metainfo_t*) malloc (sizeof(metainfo_t));
    metainfo->info = (metainfo_info_t*) malloc (sizeof(metainfo_info_t));

    while (bencode_dict_has_next(bendata)) {
	bencode_t *value = (bencode_t*) malloc (sizeof(bencode_t));
	const char *key;
	char *keybuf;
	int keylen;
	bencode_dict_get_next(bendata, value, &key, &keylen);
	keybuf = (char*) malloc (keylen + 1);
	strncpy(keybuf, key, keylen);
	keybuf[keylen] = 0;
	parse_metainfo(metainfo, value, keybuf);
    }
    return metainfo;
}


peer_list* get_peer_list() {
    peer_list *pl = (peer_list*) malloc (sizeof(peer_list));
    pl->size = 0;
    pl->infoarr = (peer_info*) malloc (50 * sizeof(peer_info));
    
    int peerfile_fd = open("peerlist", O_RDONLY);
    char buf[1024];
    int read_count;
    while (1) {
	int i = 0;
	int ch;
	
	while ((read_count = read(peerfile_fd, &ch, 1)) > 0 &&
	    ch != '\n') {
	    // printf("%c", ch);
	    buf[i++] = ch;
	}	
	buf[i] = 0;

	if (read_count <= 0)
	    break;

	if (pl->size < 50) {
	    pl->infoarr[pl->size].url = (char*) malloc (64);
	    pl->infoarr[pl->size].id = (char*) malloc (128);
	    strncpy(pl->infoarr[pl->size].url, buf, i);
	    pl->size++;
	} else {
	    break;
	}
    }
    return pl;
}

unsigned char *getSHA1(char *str, int strlen) {
    unsigned char *digest = (unsigned char*) malloc (SHA_DIGEST_LENGTH);
    SHA1((const unsigned char*)str, strlen, digest);
    return digest;
}

/* Returns socket discriptor for this tcp connection */
/* Code referred from http://www.linuxhowtos.org/data/6/client.c */
int peer_tcp_connect(char *ip, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket\n");
	return -1;
    }

    server = gethostbyname(ip);
    if (server == NULL) {
        perror("ERROR, no such host\n");
	return -1;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
	return -1;
    }

    printf("Successfully connected to %s:%d\n", ip, port);
    return sockfd;
}

int peer_tcp_disconnect(int sockfd) {
   return close(sockfd);
}

/* Returns number of bytes read from socket */
static int tcp_send(int sockfd, 
		    char *send_buf, int send_buf_len) {
    int nsent;
    nsent = write(sockfd, send_buf, send_buf_len);
    if (nsent < 0) {
         perror("ERROR writing to socket\n");
	 return -1;
    }

    return nsent;
}

static int tcp_recv(int sockfd,
		    char *recv_buf, int recv_buf_len) {
    int nrecv;

    nrecv = read(sockfd, recv_buf, recv_buf_len);
    if (nrecv < 0) {
	perror("ERROR reading from socket\n");
	return -1;
    }

    return nrecv;
}



handshake_msg_t* bt_handshake(int sockfd, metainfo_t *metainfo) {
    char* mbuf = (char*) malloc (128);
    handshake_msg_t request;
    int HANDSHAKE_MSG_LEN = 68;
	
    bzero((void*) &request, sizeof(handshake_msg_t));
    bzero((void*)mbuf, 128);

    request.prtlen = 0x13; // hex 13 => dec 19 length of protocol name
    memcpy(request.protoname, "BitTorrent protocol", 19); // protocol name
    memcpy(request.infohash, metainfo->info->infohash, 20); // info hash of torrent
    memcpy(request.peerid, "-LT112-1234567898765", 20); // Client ID
    memcpy(mbuf, &request, HANDSHAKE_MSG_LEN);
    
    tcp_send(sockfd, mbuf, HANDSHAKE_MSG_LEN);
    tcp_recv(sockfd, mbuf, HANDSHAKE_MSG_LEN);
    
    return (handshake_msg_t*) (mbuf);
}


static bt_msg_t* bt_choke(int sockfd, metainfo_t *metainfo) {
    char* mbuf = (char*) malloc (16);
    bt_msg_t request;
    int CHOKE_MSG_LEN = 5;

    bzero((void*) &request, sizeof(bt_msg_t));
    bzero((void*)mbuf, 16);

    request.length = htonl(0x1); // hex 1 => dec 1 == length of unchoke msg
    request.bt_type = 0x0; // type of unchoke msg
    // 5 is length of handshake message:
    // 4 Bytes for length + 1 Byte  for ID
    memcpy(mbuf, &request, CHOKE_MSG_LEN); 
    
    tcp_send(sockfd, mbuf, CHOKE_MSG_LEN);

    return (bt_msg_t*) (mbuf);
}

static bt_msg_t* bt_unchoke(int sockfd, metainfo_t *metainfo) {
    char* mbuf = (char*) malloc (16);
    bt_msg_t request;
    int UNCHOKE_MSG_LEN = 5;

    bzero((void*) &request, sizeof(bt_msg_t));
    bzero((void*)mbuf, 16);
    // all messages should be sent in BigEndian order
    // Hower my machine uses little endian order so conver this 4byte int to
    // Bit endian order
    request.length = htonl(0x1); 
    request.bt_type = 0x1;
    // 5 is length of handshake message:
    // 4 Bytes for length + 1 Byte  for ID
    memcpy(mbuf, &request, UNCHOKE_MSG_LEN);
    
    tcp_send(sockfd, mbuf, UNCHOKE_MSG_LEN);

    return (bt_msg_t*) (mbuf);
}

static bt_msg_t* bt_interested(int sockfd, metainfo_t *metainfo) {
    char* mbuf = (char*) malloc (16);
    bt_msg_t request;
    int INTERESTED_MSG_LEN = 5;

    bzero((void*) &request, sizeof(bt_msg_t));
    bzero((void*)mbuf, 16);

    request.length = htonl(0x1); // hex 1 => dec 1 == length of unchoke msg
    request.bt_type = 0x2; // type of unchoke msg
    // 5 is length of handshake message:
    // 4 Bytes for length + 1 Byte  for ID
    memcpy(mbuf, &request, INTERESTED_MSG_LEN); 
    
    tcp_send(sockfd, mbuf, INTERESTED_MSG_LEN);

    return (bt_msg_t*) (mbuf);    
}


bt_msg_t* bt_send_message(int sockfd,
		      message_type type,
		      metainfo_t *metainfo) {
    
    switch (type) {
    case KEEP_ALIVE:
	break;
    case CHOKE:
	return bt_choke(sockfd, metainfo);
    case UNCHOKE:
	return bt_unchoke(sockfd, metainfo);
    case INTERESTED:
	return bt_interested(sockfd, metainfo);
    default:
	break;
    }

    return NULL;
}

static char* get_block(int sockfd, int size) {
    int nread = 0;
    char *blk_buf = (char*) malloc(size + 128);
    int remaining = size;
    int offset = 0;

    while (remaining > 0) {
	nread = tcp_recv(sockfd, blk_buf + offset, remaining);
	remaining -= nread;
	offset += nread;
    }

    return blk_buf;
}

char* get_piece(int sockfd, int piece, metainfo_t *metainfo) {
    bt_msg_t msg;
    char mbuf[128];
    char* piece_buf = (char*) malloc (metainfo->info->piece_length);
    int remaining = piece_size(piece, metainfo);
    uint32_t block_size = 16384;
    uint32_t offset = 0;
    int REQUEST_MSG_LEN = 17;
    int PIECE_HEADER_LEN = 13;
    // Get all blocks
    while (remaining > 0) {
	int read_size = min(remaining, block_size);

	bzero((void*) mbuf, 128);
	msg.length = htonl(0xd);
	msg.bt_type = 0x6;
	msg.payload.request.index = htonl(piece);
	msg.payload.request.begin = htonl(offset);
	msg.payload.request.length = htonl(read_size);
	memcpy(mbuf, &msg, REQUEST_MSG_LEN);

	tcp_send(sockfd, mbuf, REQUEST_MSG_LEN);
	/* Get piece meg response */
	char *blk = get_block(sockfd, read_size + PIECE_HEADER_LEN);
	int i;
	for (i = 0; i < 23; i++)
	    printf("%02x ",blk[i]);
	printf("\n");
	memcpy(piece_buf + offset, blk + PIECE_HEADER_LEN, read_size);
	remaining -= read_size;
	offset += read_size;
	//free(blk);
    }

    return piece_buf;
}

void* peerlisten(void *args) {
    int port = 6881;
    int sockfd, newsockfd, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
	     sizeof(serv_addr)) < 0) 
	perror("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    
    printf("Listening on port 6881....\n");
    while (1) {
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t*)&clilen);
	if (newsockfd < 0) 
	    perror("ERROR on accept");
	printf("Accepted connection\n");
	n = 1;
	while (n < 0) {
	    bt_msg_t *msg;
	    bzero(buffer, 255);
	    msg = (bt_msg_t*) buffer;
	    n = read(newsockfd, buffer, 5);
	    printf("Got message type %x\n", msg->bt_type);
	}
    }

    return 0; 
}
