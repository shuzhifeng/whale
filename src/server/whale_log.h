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
		int32_t		index;
		int32_t		term;
		std::string	data;
	} log_entry_t;


	typedef std::vector<log_entry_t>::iterator log_entry_it;
	#define LOG_ENTRY_SENTINEL log_entry_t{0, 0, ""}

	#define LOG_ENTRY_LEN(e) ((e).data.size() + 2 * sizeof(int32_t))
	class logger {
	public:

		logger(std::string log_filename):log_file(log_filename),
			  fd(-1), entries({LOG_ENTRY_SENTINEL}) {}

		/*
		* Bring the log entries in log file into memory.
		*/
		w_rc_t init();

		std::vector<log_entry_t> & get_entries() {
			return entries;
		}

		log_entry_t & get_last_log() {
			return entries.back();
		}

		/*
		* commit all entries whose index is less or equal to @end
		*/
		void commit_until(w_int_t end);

		/*
		* erase entries starting at @start and all that follow it.
		*/
		void chop(log_entry_it start);

		/*
		* append entries in @new_entries.
		*/
		void append(std::vector<log_entry_t> new_entries);

		/*
		* find the entry with index being @idx.
		* Return: an iterator pointing to that entry, 
		*         an iterator pointing to the end of @entries(i.e. "entries.end()").
		*/
		log_entry_it find_by_idx(int32_t idx);
	private:
		void write(char * buf, size_t len);
		std::string					log_file;
		w_int_t 					fd;
		/* index of next entry in @entries to commit */
		w_int_t                     commit_idx;
		off_t                       pos;
		std::vector<log_entry_t> 	entries;
	};

}
#endif