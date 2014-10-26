/*
* Copyright (C) Xinjing Cho
*/
#include <cstdlib>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <log.h>

#include <cheetah/reactor.h>

#include <whale_server.h>

namespace whale {

	el_socket_t make_listen_fd(w_addr_t * addr) {
		el_socket_t fd;

		fd = ::socket(AF_INET, SOCK_STREAM, 0);

		if (fd == -1) {
			log_error("failed to ::socket: %s", strerror(errno));
			return -1;
		}
			return -1;

		if (::bind(fd, (struct sockaddr*)&addr->addr,
		           sizeof(struct sockaddr))) {
			log_error("failed to ::bind: %s", strerror(errno));
			return -1;
		}
		
		if (::listen(fd, WHALE_BACKLOG)) {
			log_error("failed to ::listen: %s", strerror(errno));
			return -1;
		}

		return fd;
	}
	static void server_callback(el_socket_t fd, short res_flags, void *arg) {

	}
	static bool is_ip(std::string str) {
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
	}

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
				::sscanf(port_p + 1, "%d", &port);
			else
				port = listen_port;

			if (port > 65535 || port < 0) {
				log_error("peer's port out of range(0-65535)");
				return WHALE_CONF_ERROR;
			}

			/* convert to network byte order */
			peer.addr.addr.sin_port = ::htons(port);

			inet_aton(p, &peer.addr.addr.sin_addr);

			peer.addr.addr.sin_family = AF_INET;

			peer.server = this;

			this->peers.insert(std::pair<w_addr_t, peer_t>(peer.addr, peer));

			p = strtok(NULL, " ");
		}

		el_socket_t fd;
		w_addr_t    server_addr;

		server_addr.addr.sin_family = AF_INET;
		server_addr.addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.addr.sin_port = htons(listen_port);

		if ((fd = make_listen_fd(&server_addr)) == -1) {
			log_error("failed to make_listen_fd: %s", strerror(errno));
			return WHALE_ERROR;
		}

		event_set(&this->listen_event, fd, E_READ, server_callback, this);

		reactor_init_with_timer(&r, NULL);

		if (reactor_add_event(&this->r, &this->listen_event) == -1) {
			log_error("failed to reactor_add_event: %s", strerror(errno));
			return WHALE_ERROR;
		}

		return WHALE_GOOD;
	}
}