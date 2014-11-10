/*
* Copyright (C) Xinjing Cho
*/
#include <log.h>
#include <whale_server.h>

int main(int argc, char const *argv[]) {
	whale::whale_server server;
	
	if (server.init() != WHALE_GOOD) {
		whale::log_error("failed to init whale server.");
		return 0;
	}

	server.run();

	return 0;
}