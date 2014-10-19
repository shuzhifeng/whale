/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_CONFIG_H_
#define WHALE_CONFIG_H_

#include <string>

#include <define.h>

namespace whale {

	class config {
	public:

		config(std::string cfg_filename):cfg_file(cfg_filename) {}

		/*
		* Parse the configuration file and fill up fields of this class.
		*/
		w_rc_t init();

	private:
		std::string cfg_file;
		std::string log_file;
	};
}

#endif