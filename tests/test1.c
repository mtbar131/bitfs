#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<time.h>

char buf1[9000009];
char buf2[9000009];


int read_bufs(int infd, int vfd, long offset, long read_size) {
    lseek(infd, offset, SEEK_SET);
    lseek(vfd, offset, SEEK_SET);
    int in_read_count = read(infd, buf1, read_size);
    int v_read_count = read(vfd, buf2, read_size);
    if (in_read_count != v_read_count) {
        printf("Input read count: %d || verify source read count: %d\n",
               in_read_count, v_read_count);
        return 0;
    }
    return 1;
}

int compare_reads(long size) {
    int i = 0;
    for (i = 0; i < size; i++) {
        if (buf1[i] != buf2[i]) {
            printf("Read values not same at %i Byte\n", i);
            return 1;
        }
    }
    return 0;
}

void test(int id, int infd, int vfd, long offset, long size) {
    int retVal = read_bufs(infd, vfd, offset, size);
    if (!retVal) {
        printf("Test:%d failed due to read buffers failure. Could not verify. ", id);
        return;
    }
    if (compare_reads(size)) {
        printf("Test:%d failed! Read data does not match. ", id);
    } else {
        printf("Test:%d Successful! ", id);
    }
    printf("\n");
}

int main(int argc, char **argv) {

    if (argc != 3) {
        printf("Usage: ./%s <input-path> <verify-path>\n", argv[0]);
        return 0;
    }
    
    char *input_path = argv[1];
    char *verify_path = argv[2];

    int infd = open(input_path, O_RDONLY);
    int vfd = open(verify_path,O_RDONLY);

    test(1, infd, vfd, 0, 10007);
    test(2, infd, vfd, 0, 100070);
    test(3, infd, vfd, 0, 524288);
    test(4, infd, vfd, 0, 524289);
    test(5, infd, vfd, 0, 924289);
    test(6, infd, vfd, 0, 1048575);
    test(7, infd, vfd, 0, 1048576);
    test(8, infd, vfd, 524289, 1048576);
    test(9, infd, vfd, 0, 924289);
    test(10, infd, vfd, 9048576, 924289);
    test(11, infd, vfd, 868745216, 534288);
    
    return 0;
}
