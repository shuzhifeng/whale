#sources
WHALE_SRC = server/whale_config.cpp common/file_mmap.cpp common/log.cpp common/util.cpp common/message.cpp \
            server/whale_message.cpp server/whale_server.cpp server/whale_log.cpp  \
            server/main.cpp
#object files
WHALE_OBJ = $(WHALE_SRC:.cpp=.o)
#executable
PROGRAM = main
#compiler
CC = g++

#includes
INCLUDE = -Icommon -Iserver
#linker params
LINKPARAMS = -lxson -lcheetah
#options for development
CFLAGS = --std=c++11 -g -O0 -Wall -Werror -DNOLOG
#options for release
#CFLAGS = --std=c++11 -g -O2 -Wall -Werror

all:
	$(CC) -o $(PROGRAM) $(CFLAGS) $(INCLUDE) $(WHALE_SRC) $(LINKPARAMS)

clean:
	-rm $(PROGRAM)
	-rm *.o