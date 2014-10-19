/*
* Copyright (C) Xinjing Cho
*/
#include <cstdio>
#include <cstdarg>
#include <ctime>

#include <log.h>

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
	
}