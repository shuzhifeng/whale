#sources
WHALE_SRC = server/whale_config.cpp server/whale_log.cpp server/whale_message.cpp \
            server/whale_server.cpp common/log.cpp common/file_mmap.cpp common/util.cpp common/message.cpp
#object files
WHALE_OBJ = $(WHALE_SRC:.cpp=.o)
#executable
#PROGRAM = libxson.so
#compiler
CC = g++

#includes
INCLUDE = -Icommon -Iserver
#linker params
LINKPARAMS = -g -O0
#options for development
CFLAGS = --std=c++11 -g -O0 -Wall -Werror
#options for release
#CFLAGS = --std=c++11 -g -O2 -Wall -Werror

compile:
	$(CC) -c $(CFLAGS) $(INCLUDE) $(WHALE_SRC)

clean:
	-rm *.o