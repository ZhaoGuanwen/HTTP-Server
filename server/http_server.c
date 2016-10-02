#include "http_server.h"

void not_found(int sock)
{
	char buf[1024];
	
	sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
	send(sock, buf, strlen(buf), 0);

	sprintf(buf, "Content-type: text/html\r\n\r\n");
	send(sock, buf, strlen(buf), 0);
	
	sprintf(buf, "<html><h1>404 NOT FOUND</h1></html>");
	send(sock, buf, strlen(buf), 0);
}

void bad_request(int sock)
{
	char buf[1024];
	
	sprintf(buf, "HTTP/1.0 400 Bad Request\r\n");
	send(sock, buf, strlen(buf), 0);

	sprintf(buf, "Content-type: text/html\r\n\r\n");
	send(sock, buf, strlen(buf), 0);
	
	sprintf(buf, "<html><h1>400 Bad Request</h1></html>");
	send(sock, buf, strlen(buf), 0);
}

void method_not_allowed(int sock)
{
	char buf[1024];
	
	sprintf(buf, "HTTP/1.0 405 Method Not Allowed\r\n");
	send(sock, buf, strlen(buf), 0);

	sprintf(buf, "Content-type: text/html\r\n\r\n");
	send(sock, buf, strlen(buf), 0);
	
	sprintf(buf, "<html><h1>405 Method Not Allowed</h1></html>");
	send(sock, buf, strlen(buf), 0);

}

void internal_server_error(int sock)
{
	char buf[1024];
	
	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	send(sock, buf, strlen(buf), 0);

	sprintf(buf, "Content-type: text/html\r\n\r\n");
	send(sock, buf, strlen(buf), 0);
	
	sprintf(buf, "<html><h1>500 Internal Server Error</h1></html>");
	send(sock, buf, strlen(buf), 0);

}

void echo_errno(int sock, int err_code)
{
	switch(err_code){
		case 400:
			bad_request(sock);
			break;
		case 404:
			not_found(sock);
			break;
		case 405:
			method_not_allowed(sock);
			break;
		case 500:
			internal_server_error(sock);
			break;
		default:
			break;
	}

	close(sock);
}

void usage(const char *proc)
{
	assert(proc);

	printf("usage: %s [ip] [port]\n", proc);
}

int start_up(const char *ip, int port)
{
	assert(ip);
	assert(port > 0);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if( sock < 0 ){
		perror("socket");
		exit(1);
	}

	//Address already in use
	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = inet_addr(ip);
	local.sin_port = htons(port);

	if( bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0 ){
		perror("bind");
		exit(2);
	}

	if( listen(sock, 5) < 0 ){
		perror("listen");
		exit(3);
	}

	return sock;
}

int get_line(int sock, char *line, int len)
{
	char ch = 0;
	ssize_t s = -1;
	int i = 0;
	while( i < len && ch != '\n' ){
		s = recv(sock, &ch, 1, 0);
		if( s > 0 ){
			if( ch == '\r' ){ // 1.\r\n 2.\n 3.\r
				s = recv(sock, &ch, 1, MSG_PEEK);
				if( s > 0 && ch == '\n'){ // /r/n -> /n
					recv(sock, &ch, 1, 0);
				}else{ // /r -> /n
					ch = '\n';
				}
			}
			line[i++] = ch;
		}else{
			ch = '\n';
		}
	}

	line[i] = '\0';

	return i;
}

void echo_www(int sock, const char *path, ssize_t size)
{
	int fd = open(path, O_RDONLY);
	if( fd < 0 ){
		echo_errno(sock, 500); //Error: Internal Server Error
		return;
	}

	char buf[SIZE];
	sprintf(buf, "HTTP/1.0 200 OK\r\n\r\n"); 
	send(sock, buf, strlen(buf), 0); //send respons header line
	if( sendfile(sock, fd, NULL, size) < 0 ){
		echo_errno(sock, 500); //Error: Internal Server Error
		return;
	}

	close(fd);
}

void clear_header(int sock)
{
	int ret = 0;
	char line[SIZE];

	do{
		ret = get_line(sock, line, sizeof(line)-1);
	}while( ret > 0 && strcmp(line, "\n") != 0 );
}

void exec_cgi(int sock, const char *method, char *path, char *query_string)
{
	char line[SIZE];
	int ret = 0;
	int content_len = -1;
	if( strcasecmp(method, "GET") == 0 ){ //GET: clear the message header
		clear_header(sock); 
	}else{ //POST: get Content-Length
		do{
			ret = get_line(sock, line, sizeof(line)-1);
			if( ret > 0 && strncasecmp(line, "Content-Length: ", 16) == 0 ){
				content_len = atoi(&line[16]);
			}
		}while( ret > 0 && strcmp(line, "\n") != 0 );

		if( content_len = -1 ){
			echo_errno(sock, 400); //Error: Bad Request
			return;
		}
	}

	//create pipe
	int input_cgi[2];
	int output_cgi[2];

	if( pipe(input_cgi) < 0 ){
		echo_errno(sock, 500); //Error: Internal Server Error
		return;
	}
	if( pipe(output_cgi) < 0 ){
		echo_errno(sock, 500); //Error: Internal Server Error
		return;
	}

	char method_env[SIZE];
	char query_string_env[SIZE];
	char content_len_env[SIZE];

	pid_t id = fork();
	if( id == 0 ){ //child
		close(input_cgi[1]);
		close(output_cgi[0]);

		dup2(input_cgi[0], 0);
		dup2(output_cgi[1], 1);

		sprintf(method_env, "METHOD=%s", method);
		putenv(method_env);

		if( strcasecmp(method, "GET") == 0 ){
			sprintf(query_string_env, "QUERY_STRING=%s", query_string);
			putenv(query_string_env);
		}else{
			sprintf(content_len_env, "CONTENT_LENGTH=%d", content_len);
			putenv(content_len_env);
		}

		execl(path, path, NULL);

		exit(1);
	}else{ //father
		close(input_cgi[0]);
		close(output_cgi[1]);
		
		char ch = 0;
		if( strcasecmp(method, "POST") == 0 ){
			int i = 0;
			for( ; i < content_len; i++ ){
				recv(sock, &ch, 1, 0);
				write(input_cgi[1], &ch, 1);
			}
		}

		while( read(output_cgi[0], &ch, 1) > 0 ){
			send(sock, &ch, 1, 0);
		}

		waitpid(id, NULL, 0);
	}

	return;
}

// GET / HTTP/1.0
void *accept_request(void *arg)
{
	int sock = (int)arg;

	int cgi = 0;
	char line[SIZE];
	char method[SIZE];
	char url[SIZE];
	char path[SIZE];
	char *query_string = NULL;

	
	//get request-line
	int line_len = get_line(sock, line, sizeof(line)-1);
	printf("\nline: %s", line);
	
	//get request method
	int i = 0, j = 0;
	while( i < sizeof(line)-1 && j < sizeof(method) && !isspace(line[i]) ){
		method[j] = line[i];
		i++, j++;
	}
	method[j] = '\0';
	printf("method: %s\n", method);

	//get request url
	j = 0;
	while( isspace(line[i]) && i < strlen(line) ){
		i++;
	}
	while( i < sizeof(line) && j < sizeof(url)-1 && !isspace(line[i]) ){
		url[j] = line[i];
		i++, j++;
	}
	url[j] = '\0';
	printf("url: %s\n", url);

	if( strcasecmp(method, "POST") != 0 &&\
				strcasecmp(method, "GET") != 0 ){
		echo_errno(sock, 405); //Error: Method Not Allowed
		return (void *)-1;
	}

	if( strcasecmp(method, "POST") == 0 ){
		cgi = 1;
	}

	if( strcasecmp(method, "GET") == 0 ){
		query_string = url;
		while( *query_string != '\0' ){
			if( *query_string == '?' ){
				cgi = 1;
				*query_string = '\0';
				query_string++;
				break;
			}
			query_string++;
		}
	}

	//get path
	sprintf(path, "htdocs%s", url);
	if( path[strlen(path)-1] == '/' ){
		strcat(path, "index.html");
	}

	printf("path: %s\n", path);

	struct stat st;
	if( stat(path, &st) < 0 ){
		echo_errno(sock, 404); //Error: Not Found
		return (void *)-2;
	}else{
		if( S_ISDIR(st.st_mode) ){
			strcat(path, "index.html");
		}else{
			if( (st.st_mode & S_IXUSR) || \
			    (st.st_mode & S_IXGRP) || \
				(st.st_mode & S_IXGRP) ){
				cgi = 1;
			}
		}
	}

	if( !cgi ){
		clear_header(sock);
		echo_www(sock, path, st.st_size);
	}else{
		exec_cgi(sock, method, path, query_string);
	}

	close(sock);
	return (void *)0;
}

int main(int argc, char *argv[])
{
	if( argc != 3){
		usage(argv[0]);
		return 4;
	}

	int listen_sock = start_up(argv[1], atoi(argv[2]));

	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	while( 1 ){
		int new_sock = accept(listen_sock, (struct sockaddr *)&client, &len);
		if( new_sock < 0 ){
			perror("accept");
			continue;
		}

		pthread_t id;
		pthread_create(&id, NULL, accept_request, (void *)new_sock);
		pthread_detach(id);
	}

	return 0;
}
