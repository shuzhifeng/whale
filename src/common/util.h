/*
* Copyright (C) Xinjing Cho
*/
#ifndef UTIL_H_
#define UTIL_H_
#include <string>
#include <cstdlib>

#include <define.h>

namespace whale {
	std::string string_format(const std::string fmt_str, ...);
	std::string w_addr_to_json(const std::string & key_name, const w_addr_t & addr);
}
#endif