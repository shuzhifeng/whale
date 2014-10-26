/*
* Copyright (C) Xinjing Cho
*/

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

}