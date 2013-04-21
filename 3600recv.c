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
    void* buffer = malloc(buf_len);
    void * buf; 
    unsigned int nextSequence = 0;
   
    // wait to receive, or for a timeout
    void**windowCache = calloc(sizeof(void*), WINDOW_SIZE); 
    int count = 0; 
    while (1) {
        buf = buffer; 
        FD_ZERO(&socks);
        FD_SET(sock, &socks);
        if (select(sock + 1, &socks, NULL, NULL, &t)) {
            int received;
            if ((received = recvfrom(sock, buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                perror("recvfrom");
                exit(1);
            }
            count++; 
            //      dump_packet(buf, received);
            t.tv_sec = RECV_TIMEOUT;
            header *myheader = get_header(buf);
            char *data = get_data(buf);
            fprintf(stderr,"checksum %d\n",checksum(data,myheader->length)); 
            if (myheader->magic == MAGIC && get_checksum(data,myheader->length) == checksum(data,myheader->length) ) {
                        windowCache[ myheader->sequence % WINDOW_SIZE ] = buf;
                        fprintf(stderr,"Cached sequence # %d \n",myheader->sequence); 
            }
            while(1) {
                buf = windowCache[ nextSequence% WINDOW_SIZE ]; 
                if (buf) {
                    myheader = get_header(buf);
                    data = get_data(buf);            
                    if (read_header_sequence( buf ) == (signed int)nextSequence) {                     
                        write( 1, get_data( buf ), read_header_length( buf ) );
                        mylog("[recv data] %d (%d) %s\n", myheader->sequence, myheader->length, "ACCEPTED (in-order)");
                        nextSequence++;
                    } else if (myheader->eof) {
                        mylog("[recv eof]\n");
                        mylog("[completed]\n");
                        exit(0);                
                    } else {
                        break; 
                    }
                } else {
                    break; 
                }
            }  
            if (count % WINDOW_SIZE == 0) {
            mylog("[send ack] %d\n", nextSequence);
            header *responseheader = make_header( nextSequence, 0, myheader->eof, 1 );
            if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
                perror("sendto");
                exit(1);
            }
            count = 0; 
            }
               
        } else {
            mylog("[error] timeout occurred\n");
            exit(1);
        }
    }
      
    return 0;
}
