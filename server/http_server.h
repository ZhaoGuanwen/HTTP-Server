#pragma once

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#define SIZE 1024

void not_found(int sock);
void bad_request(int sock);
void method_not_allowed(int sock);
void internal_server_error(int sock);
void echo_errno(int sock, int err_code);
void usage(const char *proc);
int start_up(const char *ip, int port);
int get_line(int sock, char *line, int len);
void echo_www(int sock, const char *path, ssize_t size);
void clear_header(int sock);
void exec_cgi(int sock, const char *method, char *path, char *query_string);
void *accept_request(void *arg);
