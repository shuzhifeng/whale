/*
* Copyright (C) Xinjing Cho
*/
#include <log.h>

#include <whale_sm.h>

namespace whale {
	w_rc_t state_machine::init() {
		w_rc_t rc;

		std::string * map_file = cfg->get("map_file");

		if (map_file == nullptr) {
			log_error("must specify map_file in conf file.");
			return WHALE_CONF_ERROR;
		}

		map = std::unique_ptr<file_mmap>(new file_mmap(*map_file, sizeof(file_mapped_t),
			  MAP_FILE_PROT, MAP_FILE_FLAGS));
		
		rc = map->map();

		if (rc != WHALE_GOOD)
			return rc;

		std::string * log_file = cfg->get("log_file");

		if (log_file == nullptr) {
			log_error("must specify log_file in conf file.");
			return WHALE_CONF_ERROR;
		}
		
		log = std::unique_ptr<logger>(new logger(*log_file));
		
		rc = log->init();

		if (rc != WHALE_GOOD)
			return rc;

		return WHALE_GOOD;
	}
}