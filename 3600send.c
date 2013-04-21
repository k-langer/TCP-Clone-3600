/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600sendrecv.h"

static int DATA_SIZE = 1460;

unsigned int sequenceSend = 0;
unsigned int sequenceReceive = -1;
void* windowCache [ WINDOW_SIZE ];
unsigned char firstWindow = 1;

void usage() {
    printf("Usage: 3600send host:port\n");
    exit(1);
}

/**
 * Reads the next block of data from stdin
 */
int get_next_data(char *data, int size) {
    return read(0, data, size);
}

/**
 * Builds and returns the next packet, or NULL
 * if no more data is available.
 */
void *get_next_packet(int sequence, int *len) {
    if ( !firstWindow ) {
        mylog( "[not first]\n" );
        void* cachedPacket = windowCache[ sequence % WINDOW_SIZE ];
        int cachedHeaderSequence = read_header_sequence( cachedPacket );
        if ( cachedHeaderSequence == sequence ) {
            mylog( "[from cache] %i\n", cachedHeaderSequence );
            *len = sizeof( header ) + read_header_length( cachedPacket );
            return cachedPacket;
        }
    }

    char *data = malloc(DATA_SIZE);
    int data_len = get_next_data(data, DATA_SIZE);
    if (data_len == 0) {
        free(data);
        return NULL;
    }

    header *myheader = make_header(sequence, data_len, 0, 0);
    void *packet = malloc(sizeof(header) + data_len+sizeof(checksum_t));
    memcpy(packet, myheader, sizeof(header));
    memcpy(((char *) packet) +sizeof(header), data, data_len);
    checksum_t chksm = checksum(data,data_len); 
    memcpy(((char *) packet) +sizeof(header)+data_len, &chksm, sizeof(checksum_t)); 
    free(data);
    free(myheader);

    *len = sizeof(header) + data_len + sizeof(checksum_t);
    fprintf(stderr,"Checksum %d\n",get_checksum(packet,data_len+sizeof(header)));

    windowCache[ sequence % WINDOW_SIZE ] = packet;

    return packet;
}
int send_next_packet(int sock, struct sockaddr_in out) {
    int packet_len = 0;
    void *packet = get_next_packet(sequenceSend, &packet_len);

    if (packet == NULL) 
        return 0;

    mylog("[send data] %d~%d (%d)\n", read_header_sequence( packet ), sequenceSend, packet_len - sizeof(header));

    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
        perror("sendto");
        exit(1);
    }

    return 1;
}
int send_next_window(int sock, struct sockaddr_in out, unsigned int* packetsSent) {
    *packetsSent = 0;
    for(int i = 0; i < WINDOW_SIZE; i++) {
        if ( !send_next_packet(sock,out) ) {
            break;
        }
        (*packetsSent)++;
        sequenceSend++;
    }
    sequenceSend--;

    if ( firstWindow == 1 ) {
        firstWindow = 0;
    }

    return *packetsSent;
}

void send_final_packet(int sock, struct sockaddr_in out) {
    header *myheader = make_header(sequenceSend+1, 0, 1, 0);
    mylog("[send eof]\n");

    if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
        perror("sendto");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a UDP socket in C, 
   * binding to a empheral port, printing out the port number.
   *
   * I've also included a very simple transport protocol that simply
   * acknowledges every received packet.  It has a header, but does
   * not do any error handling (i.e., it does not have sequence 
   * numbers, timeouts, retries, a "window"). You will
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

    // extract the host IP and port
    if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
        usage();
    }

    char *tmp = (char *) malloc(strlen(argv[1])+1);
    strcpy(tmp, argv[1]);

    char *ip_s = strtok(tmp, ":");
    char *port_s = strtok(NULL, ":");

    // first, open a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // next, construct the local port
    struct sockaddr_in out;
    out.sin_family = AF_INET;
    out.sin_port = htons(atoi(port_s));
    out.sin_addr.s_addr = inet_addr(ip_s);

    // socket for received packets
    struct sockaddr_in in;
    socklen_t in_len;

    // construct the socket set
    fd_set socks;

    // construct the timeout
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = SEND_TIMEOUT;

    unsigned int packetsSent = 0;
    unsigned int consecutiveTimeouts = 0;

    while (send_next_window(sock, out, &packetsSent) && consecutiveTimeouts < MAX_TIMEOUTS) {
        char timeout = 0;
        unsigned int done = 0;
        while ( !timeout && done < packetsSent ) {
            t.tv_usec = SEND_TIMEOUT;
            FD_ZERO(&socks);
            FD_SET(sock, &socks);

            // wait to receive, or for a timeout
            if (select(sock + 1, &socks, NULL, NULL, &t)) {
                consecutiveTimeouts = 0;

                unsigned char buf[10000];
                int buf_len = sizeof(buf);
                int received;
                if ((received = recvfrom(sock, &buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                    perror("recvfrom");
                    exit(1);
                }

                header *myheader = get_header(buf);

                if ((myheader->magic == MAGIC) && ( myheader->sequence <= sequenceSend ) && (myheader->ack == 1)) {
                    mylog("[recv ack] %d\n", myheader->sequence);
                    sequenceReceive = myheader->sequence;
                    done++;
                } else {
                    mylog("[recv corrupted ack] %x %d > %d - %d - %d\n", MAGIC, myheader->sequence, sequenceSend, myheader->ack, myheader->eof);
                }
            } else {
                mylog("[error] timeout occurred\n");
                timeout = 1;
                consecutiveTimeouts++;
                sequenceSend = sequenceReceive + 1;
                break;
            }

        }
        sequenceSend = sequenceReceive + 1;

    }
    if ( consecutiveTimeouts == MAX_TIMEOUTS ) {
        mylog( "[max timeouts hit]\n" );
    } else {
        consecutiveTimeouts = 0;
        while ( consecutiveTimeouts < 2 * MAX_TIMEOUTS ) {
            send_final_packet(sock, out);
            t.tv_sec = 0;
            t.tv_usec = 100000;
            if (select(sock + 1, &socks, NULL, NULL, &t)) {
                unsigned char buf[10000];
                int buf_len = sizeof(buf);
                int received;
                if ((received = recvfrom(sock, &buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                    perror("recvfrom");
                    exit(1);
                }

                header* myheader = get_header(buf);
                if ( ( myheader->magic == MAGIC ) &&  myheader->eof ) {
                    break;
                }
            } else {
                consecutiveTimeouts++;
                mylog( "[timeout on eof]\n");
            }
        }
    }
    mylog("[completed]\n");

    return 0;
}
