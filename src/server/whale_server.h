/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_SERVER_H_
#define WHALE_SERVER_H_

#include <string>

#include <common/define.h>
#include <whale_log.h>
#include <whale_config.h>

namespace whale {

#define FOLLOWER	0
#define CANDIDATE	1
#define LEADER		2

class whale_server {
public:
	whale_server(std::string cfg_file);

private:
	unique_ptr<logger> log;
	unique_ptr<config> cfg;
};

}


#endif