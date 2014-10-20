/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_CONFIG_H_
#define WHALE_CONFIG_H_

#include <string>
#include <map>

#include <define.h>

namespace whale {

	#define CONF_FILE_FLAGS	O_RDONLY
	#define CONF_BUFSIZE	4096
	class config {
	public:

		config(std::string cfg_filename):cfg_file(cfg_filename) {}

		/*
		* Parse the configuration file and fill up fields of this class.
		*/
		w_rc_t parse();

		std::string * get(const char * key) { 
			std::map<std::string, std::string>::iterator it = conf_map.find(key);
			if (it == conf_map.end())
				return nullptr;

			return &it->second;
		}
	private:
		/* read one line from @fd into @linebuf */
		void getline(w_int_t fd, std::string & linebuf);
		std::string 						cfg_file;
		std::map<std::string, std::string>  conf_map;
	};

}

#endif