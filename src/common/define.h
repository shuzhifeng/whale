/*
* Copyright (C) Xinjing Cho
*/

#ifndef DEFINES_H_
#define DEFINES_H_

#include <cstdint>
#include <cstddef>
#include <cerrno>

#include <unistd.h>

typedef intptr_t 	w_int_t;
typedef uintptr_t	w_uint_t;

typedef enum {
	WHALE_OOM=-2,
	WHALE_ERROR,
	WHALE_GOOD
}w_rc_t;

#endif