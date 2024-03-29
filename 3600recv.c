/* CS3600, Spring 2013
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

int main() {
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

    void* windowCache [ WINDOW_SIZE ];
    for ( int x =0; x < WINDOW_SIZE; x++ ) {
        windowCache[ x ] = NULL;
    }

    // first, open a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // next, construct the local port
    struct sockaddr_in out;
    out.sin_family = AF_INET;
    out.sin_port = htons(0);
    out.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &out, sizeof(out))) {
        perror("bind");
        exit(1);
    }

    struct sockaddr_in tmp;
    int len = sizeof(tmp);
    if (getsockname(sock, (struct sockaddr *) &tmp, (socklen_t *) &len)) {
        perror("getsockname");
        exit(1);
    }

    mylog("[bound] %d\n", ntohs(tmp.sin_port));

    // wait for incoming packets
    struct sockaddr_in in;
    socklen_t in_len = sizeof(in);

    // construct the socket set
    fd_set socks;

    // construct the timeout
    struct timeval t;
    t.tv_sec = RECV_TIMEOUT;
    t.tv_usec = 0;

    // our receive buffer
    int buf_len = 1500;
    void* buf = malloc(buf_len);
    unsigned int nextSequence = 0;

    // wait to receive, or for a timeout
    while (1) {
        FD_ZERO(&socks);
        FD_SET(sock, &socks);
        if (select(sock + 1, &socks, NULL, NULL, &t)) {
            int received;
            if ((received = recvfrom(sock, buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                perror("recvfrom");
                exit(1);
            }
            //      dump_packet(buf, received);
            t.tv_sec = RECV_TIMEOUT;
            unsigned int headerSequence = read_header_sequence( buf );
            unsigned int headerMagic = read_header_magic( buf );
            int headerLength = read_header_length( buf );
            int headerEof = read_header_eof( buf );
            char *data = get_data(buf);
            //basic check to see if the packet is valid
            if (headerMagic == MAGIC ) {
                    //Clear old entries before losing the pointer
                    if ( windowCache[ headerSequence % WINDOW_SIZE ] ) {
                        free( windowCache [ headerSequence % WINDOW_SIZE ] );
                        windowCache[ headerSequence % WINDOW_SIZE ] = 0 ;
                    }
                    //Calculate the checksums to check the data for errors, compare to attached checksum, only store in recv buff if it passes this check
                    if ( get_checksum(data,headerLength ) == checksum(data, headerLength ) ){ 
                        mylog("Checksum: %d\n",get_checksum(data,headerLength));
                        windowCache[ headerSequence % WINDOW_SIZE ] = (void*)malloc( sizeof( header ) + read_header_length( buf ) );
                        memcpy( windowCache[ headerSequence % WINDOW_SIZE ], buf, sizeof( header ) + read_header_length( buf ) );
                    } 
                    int ackLength = 0;

                    //If the seuqnece matches the next sequence that is needed, progress the sequence number
                    if ( headerSequence == nextSequence ) {
                        write(1, data, headerLength);
                        nextSequence++;
                        void* nextPacket = NULL;
                        nextPacket = windowCache[ nextSequence % WINDOW_SIZE ];
                        while ( nextPacket && read_header_sequence( nextPacket ) == (signed int)nextSequence ) {
                            //Fill in additional packets that were previously stored in the recv and increment seq
                            write( 1, get_data( nextPacket ), read_header_length( nextPacket ) );
                            nextSequence++;
                            nextPacket = windowCache[ nextSequence % WINDOW_SIZE ];
                        }
                        ackLength = headerLength;
                    }

                    mylog("[recv data] %d (%d) %s\n", headerSequence, headerLength, "ACCEPTED (in-order)");
                    mylog("[send ack] %d\n", nextSequence - 1);

                    header *responseheader = make_header( nextSequence - 1, 0, headerEof, 1 );

                    if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
                        perror("sendto");
                        exit(1);
                    }
//                }

                if (headerEof) {
                    mylog("[recv eof]\n");
                    mylog("[completed]\n");
                    exit(0);
                }
            }
        } else {
            mylog("[error] timeout occurred\n");
            exit(1);
        }
    }

    return 0;
}
