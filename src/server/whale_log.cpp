/*
* Copyright (C) Xinjing Cho
*/

#include <cstdio>
#include <cstdarg>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <whale_log.h>

namespace whale {

	void __log_stderr_print(const char * filename, const char * func, int line,
							const char *fmt, ...){
		time_t t;
		va_list list;

		time(&t);
		fprintf(stderr, "%s ", ctime(&t));
		fprintf(stderr, "[%s]:[%s]:[line %d]: ", filename, func, line);
		

		va_start(list, fmt);
		vfprintf(stderr, fmt, list);
		va_end(list);

		fputc('\n', stderr);
	}

	w_int_t logger::init(){
		fd = ::open(log_file.c_str(), ENTRY_LOG_FILE_FLAGS, ENTRY_LOG_FILE_MODE);

		if (fd == -1) {
			log_error("failed to open log file \"%s\"", log_file.c_str());
			std::terminate();
		}

		return WHALE_GOOD;
	}

	w_int_t	logger::write(){

		return WHALE_GOOD;
	}

}