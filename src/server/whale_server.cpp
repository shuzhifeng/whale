/*
* Copyright (C) Xinjing Cho
*/
#include <cstdlib>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <log.h>

#include <whale_server.h>

namespace whale {
	static auto is_ip = [](std::string str)->bool {
		char *  p = strtok(const_cast<char*>(str.data()), ".");
		w_int_t cnt = 0, val;

		while (p) {
			++cnt;
			val = std::stol(p);

			if (val > 255 || val < 0)
				return false;

			p = strtok(NULL, ".");
		}

		return cnt == 4;
	};

	w_rc_t whale_server::init() {
		w_rc_t rc;

		cfg = std::unique_ptr<config>(new config(cfg_file));

		rc = cfg->parse();

		if (rc != WHALE_GOOD)
			return rc;

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

		std::string * s_listen_port = cfg->get("listen_port");

		if(s_listen_port == nullptr)
			listen_port = DEFAULT_LISTEN_PORT;
		else
			listen_port = std::stoi(*s_listen_port);

		if (listen_port < 0 || listen_port > 65535) {
			log_error("listen_port out of range(0-65535)");
			return WHALE_CONF_ERROR;
		}

		std::string * peer_ips = cfg->get("peers");
		char        * p;

		if (peer_ips == nullptr) {
			log_error("peers is required");
			return WHALE_CONF_ERROR;
		}

		p = strtok(const_cast<char*>(peer_ips->data()), " ");

		while (p) {
			peer_t  peer;
			char   *port_p;
			w_int_t port;

			if (!is_ip(p)) {
				log_error("invalid peer ip: %s", p);
				return WHALE_CONF_ERROR;
			}

			memset(&peer, 0, sizeof(peer));

			port_p = strchr(p, ':');

			*port_p = '\0';

			if (p)
				sscanf(port_p + 1, "%d", &port);
			else
				port = listen_port;

			if (port > 65535 || port < 0) {
				log_error("peer's port out of range(0-65535)");
				return WHALE_CONF_ERROR;
			}

			/* convert to network byte order */
			peer.addr.addr.sin_port = htons(port);

			inet_aton(p, &peer.addr.addr.sin_addr);

			peer.addr.addr.sin_family = AF_INET;

			peer.server = this;

			this->peers.insert(std::pair<w_addr_t, peer_t>(peer.addr, peer));

			p = strtok(NULL, " ");
		}

		return WHALE_GOOD;
	}
}