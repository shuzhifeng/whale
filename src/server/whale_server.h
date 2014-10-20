/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_SERVER_H_
#define WHALE_SERVER_H_

#include <string>

#include <define.h>
#include <whale_log.h>
#include <whale_config.h>

namespace whale {

	class whale_server {
	public:
		whale_server(std::string cfg_file);

	private:
		unique_ptr<logger> log;
		unique_ptr<config> cfg;
	};

}


#endif