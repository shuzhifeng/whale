/*
* Copyright (C) Xinjing Cho
*/
#include <cstdarg>
#include <memory>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <util.h>

namespace whale {
	std::string string_format(const std::string fmt_str, ...) {
	    w_int_t final_n, n = ((w_int_t)fmt_str.size()) << 2; /* reserve 4 times as much as the length of the fmt_str */
	    std::unique_ptr<char[]> formatted;
	    va_list ap;

	    while(1) {
	        formatted.reset(new char[n]); /* wrap the plain char array into the unique_ptr */
	        strcpy(&formatted[0], fmt_str.c_str());

	        va_start(ap, fmt_str);
	        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
	        va_end(ap);

	        if (final_n < 0 || final_n >= n)
	            n += abs(final_n - n + 1);
	        else
	            break;
	    }
	    return std::string(formatted.get());
	}

	std::string w_addr_to_json(const std::string & key_name, 
									  const w_addr_t & addr) {
		return std::move(string_format("\"%s\":{\"ip\":\"%s\","
											"\"port\":%u}",
											key_name.c_str(),
											::inet_ntoa(addr.addr.sin_addr),
											::ntohs(addr.addr.sin_port)));
	}
}