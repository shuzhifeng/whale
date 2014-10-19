/*
* Copyright (C) Xinjing Cho
*/
#ifndef LOG_H_
#define LOG_H_

namespace whale {

/* simple logging utility */
void __log_stderr_print(const char * filename, const char * func, int line,
					  const char *fmt, ...);

#define log_error(...)\
	__log_stderr_print(__FILE__, __func__, __LINE__, __VA_ARGS__);

}
#endif