/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_LOG_H_
#define WHALE_LOG_H_

#include <string>
#include <vector>

#include <define.h>

namespace whale {

	#define ENTRY_LOG_FILE_MODE		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
	#define ENTRY_LOG_FILE_FLAGS	O_CREAT | O_RDWR

	typedef struct log_entry {
		w_int_t		index;
		w_int_t		term;
		std::string	data;
	} log_entry_t;

	#define LOG_ENTRY_SENTINEL log_entry_t{0, 0, ""}

	class logger {
	public:

		logger(std::string log_filename):log_file(log_filename),
			  fd(-1), entries({LOG_ENTRY_SENTINEL}) {}

		/*
		* Bring the log entries in the log file into memory.
		*/
		w_rc_t init();

		/*
		* Write log entries(not in the log file) into log file.
		*/
		w_rc_t write();

		std::vector<log_entry_t> & get_entries() {
			return entries;
		}

		w_int_t get_last_log_idx() {
			return entries.back().index;
		}
	private:
		std::string					log_file;
		w_int_t 					fd;
		std::vector<log_entry_t> 	entries;
	};

}
#endif