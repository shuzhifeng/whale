/*
* Copyright (C) Xinjing Cho
*/
#include <memory>

#include <sys/mman.h>

#include <cheetah/event.h>

#include <define.h>

#include <file_mmap.h>
#include <whale_log.h>
#include <whale_config.h>

namespace whale {
	typedef struct peer_s{
		w_addr_t 		addr;
		struct event 	e;
		w_int_t			next_idx;
		w_int_t			match_idx;
	} peer_t;

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

	class state_machine {
	public:

		state_machine(std::unique_ptr<logger> &log)
			: state(FOLLOWER), commit_index(0), last_applied(0)
		{}

		/*
		* initialize the state machine.
		* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
		*/
		w_rc_t init();

	private:

		inline file_mapped_t * get_fmapped() {
			return static_cast<file_mapped_t *>(map->get_addr());
		}
		std::unique_ptr<file_mmap> 		map;
		std::unique_ptr<logger>			log;
		std::shared_ptr<config> 		cfg;
		w_int_t							state;
		w_int_t							commit_index;
		w_int_t							last_applied;
		
	};

}