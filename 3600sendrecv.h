/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SENDRECV_H__
#define __3600SENDRECV_H__

#include <stdio.h>
#include <stdarg.h>

#define WINDOW_SIZE 15
#define SEND_TIMEOUT 80000 //microseconds
#define DEBUG_SEND_TIMEOUT 0 //seconds
#define RECV_TIMEOUT 15     //seconds
#define MAX_TIMEOUTS 100
#define RETRANSMIT 5

typedef int checksum_t;
typedef char bool_t;

typedef struct header_t {
  unsigned int magic:14;
  unsigned int ack:1;
  unsigned int eof:1;
  unsigned short length;
  unsigned int sequence;
} header;

unsigned int MAGIC;

void dump_packet(unsigned char *data, int size);
header *make_header(int sequence, int length, int eof, int ack);
header *get_header(void *data);
int read_header_sequence(void* data);
int read_header_length(void* data);
int read_header_eof(void* data);
int read_header_magic(void* data);
char *get_data(void *data);
checksum_t get_checksum(void *data,int dataLen);
char *timestamp();
void mylog(char *fmt, ...);
checksum_t checksum(char* data,int len);
bool_t check_checksum(char* data, int len);
#endif

