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
		time_t  t;
		tm     *now;
		va_list list;

		time(&t);
		now = localtime(&t);
		
		fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d ", now->tm_year + 1900,
		        now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min,
		        now->tm_sec);
		fprintf(stderr, "[%s]:[%s]:[line %d]: ", filename, func, line);
		

		va_start(list, fmt);
		vfprintf(stderr, fmt, list);
		va_end(list);

		fputc('\n', stderr);
	}
	
}