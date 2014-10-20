/*
* Copyright (C) Xinjing Cho
*/

#ifndef DEFINE_H_
#define DEFINE_H_

#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <string>

#include <unistd.h>
#include <netinet/in.h>

typedef intptr_t 	w_int_t;
typedef uintptr_t	w_uint_t;

typedef enum {
	WHALE_CONF_ERROR = -4,
	WHALE_MMAP_ERROR,
	WHALE_OOM,
	WHALE_ERROR,
	WHALE_GOOD
}w_rc_t;

typedef struct w_addr_s {
	struct sockaddr_in 	addr;
	std::string 		name;
} w_addr_t;

#endif