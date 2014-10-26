/*
* Copyright (C) Xinjing Cho
*/
#include <memory>

#include <sys/mman.h>

#include <cheetah/reactor.h>

#include <define.h>

#include <file_mmap.h>
#include <whale_log.h>
#include <whale_config.h>

namespace whale {
	class whale_server;

	typedef struct peer_s{
		w_addr_t 		addr;
		struct event 	e;
		w_int_t			next_idx;
		w_int_t			match_idx;
		whale_server   *server;
	} peer_t;

	#define WHALE_PEER_SELF peer_t {{0}, {0}, 0, 0}

	/* stuff need to stay persistent on disk*/
	typedef struct file_mapped_s{
		w_int_t 	current_term;	/* current term */
		peer_t		voted_for;		/* candidate veted for */
	} file_mapped_t;


	#define FOLLOWER	0
	#define CANDIDATE	1
	#define LEADER		2

	#define MAP_FILE_PROT 	PROT_WRITE | PROT_READ
	#define MAP_FILE_FLAGS	MAP_SHARED

	#define DEFAULT_LISTEN_PORT 29999

	typedef struct {
		bool operator()(const w_addr_t &a1, const w_addr_t &a2) {
			return a1.addr.sin_addr.s_addr < a2.addr.sin_addr.s_addr;
		}
	}WADDR_PRED;

	class whale_server {
	public:

		whale_server(std::string file = "whale.conf")
			:cfg_file(file), state(FOLLOWER), commit_index(0), last_applied(0) {}

		/*
		* initialize the server.
		* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
		*/
		w_rc_t init();

	private:

		inline file_mapped_t * get_fmapped() {
			return static_cast<file_mapped_t *>(map->get_addr());
		}
		std::unique_ptr<file_mmap> 		map;
		std::unique_ptr<logger>			log;
		std::unique_ptr<config> 		cfg;
		std::string                     cfg_file;
		std::map<w_addr_t, peer_t,
				 WADDR_PRED>            peers;
		struct reactor                  r;
		w_int_t							state;
		w_int_t							commit_index;
		w_int_t							last_applied;
		w_int_t                         listen_port;

	};

}