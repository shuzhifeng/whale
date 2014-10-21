/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_SERVER_H_
#define WHALE_SERVER_H_

#include <memory>

#include <string>

#include <define.h>
#include <whale_log.h>
#include <whale_config.h>

namespace whale {

	class whale_server {
	public:
		whale_server(std::string cfg_file);

	private:
		std::unique_ptr<logger> log;
		std::shared_ptr<config> cfg;
	};

}


#endif