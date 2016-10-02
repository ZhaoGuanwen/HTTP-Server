ROOT_PATH=$(shell pwd)
SERVER=$(ROOT_PATH)/server
CC=gcc
FLAGS=#
LDFLAGS=-lpthread#
NAME=server_httpd
CGI_PATH=cgi

SERVER_SRC=$(shell ls $(SERVER) | grep -E '*.c')
SERVER_OBJ=$(SERVER_SRC:.c=.o)

.PHONY:all
all:$(NAME) cgi

$(NAME):$(SERVER_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
%.o:$(SERVER)/%.c
	$(CC) -c $< $(FLAGS)

.PHONY:cgi
cgi:
	for i in `echo $(CGI_PATH)`;\
	do\
		cd $$i;\
		make;\
		cd -;\
	done

.PHONY:clean
clean:
	rm -rf *.o $(NAME) output
	for i in `echo $(CGI_PATH)`;\
	do\
		cd $$i;\
		make clean;\
		cd -;\
	done

.PHONY:output
output:all
	mkdir -p output/htdocs
	cp $(NAME) output
	cp cgi/math_cgi output/htdocs
	cp htdocs/index.html output/htdocs
	cp -rf htdocs/img output/htdocs


