/*
* Copyright (C) Xinjing Cho
*/
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <log.h>
#include <whale_log.h>

namespace whale {

	w_rc_t logger::init() {
		fd = ::open(log_file.c_str(), ENTRY_LOG_FILE_FLAGS, ENTRY_LOG_FILE_MODE);

		if (fd == -1) {
			log_error("failed to open log file \"%s\"", log_file.c_str());
			return WHALE_ERROR;
		}

		return WHALE_GOOD;
	}

	w_rc_t	logger::write() {

		return WHALE_GOOD;
	}


	log_entry_it logger::find_by_idx(w_int_t idx) {
		log_entry_t v{idx, 0, ""};

		auto it = std::lower_bound(entries.begin(), entries.end(), v,
			                       [](const log_entry_t & lhs,
			                          const log_entry_t & rhs)->bool {
			                          return lhs.index < rhs.index;
			                       });

		if (it == entries.end() || it->index != idx)
			return entries.end();
		else
			return it;
	}

	/*
	* erase entries starting at @start and those follow it.
	*/
	void chop(log_entry_it start) {

	}

	/*
	* append entries in @new_entries.
	*/
	void append(std::vector<log_entry_t> new_entries) {

	}
}