/*
* Copyright (C) Xinjing Cho
*/
#include <memory>
#include <random>
#include <queue>
#include <cstdlib>

#include <sys/mman.h>

#include <cheetah/reactor.h>

#include <define.h>

#include <file_mmap.h>
#include <whale_log.h>
#include <whale_config.h>
#include <whale_message.h>

namespace whale {
	class whale_server;

	typedef struct message_queue_elt_s {
		/* position at which we should resume processing(read or write) */
		uint32_t    pin;
		/* temporary length for @msg */
		uint32_t    tmp_len;
		msg_sptr    msg;
	}msg_q_elt;

	typedef std::queue<msg_q_elt> msg_queue;

	typedef struct reply_queue_elt_s {
		/* reuqest message type */
		w_int_t type;
		/* holds that request sent to peer */
		void   *data;
		~reply_queue_elt_s() {
			if (type == MESSAGE_REQUEST_VOTE) 
				delete static_cast<request_vote_t *>(data);
			else if(type == MESSAGE_APPEND_ENTRIES)
				delete static_cast<append_entries_t *>(data);
			else if(type == MESSAGE_CMD_REQUEST)
				delete static_cast<cmd_request_t *>(data);
			::abort();
		}
	}reply_queue_elt_s;

	typedef std::queue<reply_queue_elt_s> wait_queue;

	typedef struct peer_s{
		w_addr_t 		addr;
		struct event 	e;
		/* timeout event to reconnect to peer */
		struct event    timeout_e;
		w_int_t			next_idx;
		w_int_t			match_idx;
		/* back pointer to server */
		whale_server   *server;
		/* if the peer is connected */
		bool            connected;
		/* messages read from peer */
		msg_queue       read_queue;
		/* messages to be written to peer */
		msg_queue       write_queue;
		/* 
		* request messages that are wating for replies
		* in the order of being sent out.
		*/
		wait_queue      request_queue;
	} peer_t;

	#define INIT_PEER    {  \
		.addr =  {{0}, ""}, \
		.e = {0},           \
		.timeout_e = {0},   \
		.next_idx = 0,      \
		.match_idx = 0,     \
		.server = 0,        \
		.connected = 0      \
	}

	/* stuff need to stay persistent on disk*/
	typedef struct file_mapped_s{
		w_int_t 	       current_term;	/* current term */
		struct sockaddr_in voted_for;		/* candidate veted for */
	} file_mapped_t;

	#define FOLLOWER	0
	#define CANDIDATE	1
	#define LEADER		2

	#define MAP_FILE_PROT 	PROT_WRITE | PROT_READ
	#define MAP_FILE_FLAGS	MAP_SHARED

	#define DEFAULT_LISTEN_PORT     29999
	#define DEFAULT_SERVING_PORT    29998
	#define WHALE_BACKLOG           10
	#define WHALE_MIN_ELEC_TIMEOUT  150
	#define WHALE_MAX_ELEC_TIMEOUT  300
	#define WHALE_RECONNECT_TIMEOUT 1000
	#define WHLAE_HEARTBEAT_TIMEOUT 50

	typedef struct {
		bool operator()(const w_addr_t &a1, const w_addr_t &a2) {
			return a1.addr.sin_addr.s_addr < a2.addr.sin_addr.s_addr ||
				   (a1.addr.sin_addr.s_addr == a2.addr.sin_addr.s_addr &&
				   	a1.addr.sin_port < a2.addr.sin_port);
		}
	}WADDR_PRED;

	std::random_device rd;

	#define NEXT_TIMEOUT(mi, ma) ((mi) + rd() % ((ma) - (mi) + 1))

	class whale_server {
	public:

		whale_server(std::string file = "whale.conf")
			:cfg_file(file), state(FOLLOWER), commit_index(0), last_applied(0) {}

		/*
		* initialize the server.
		* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
		*/
		w_rc_t init();

		void handle_listen_fd();
		void handle_read_from_peer(peer_t * p);
		void handle_write_to_peer(peer_t * p);
		void handle_serving_listen_fd();
		void handle_read_from_client(peer_t * p);
		void handle_write_to_client(peer_t * p);
		void connect_to_servers();
		void send_request_votes();
		void send_heartbeat();
		void process_messages(peer_t * p);
		void process_request_vote(peer_t * p, msg_sptr msg);
		void process_request_vote_res(peer_t * p, msg_sptr msg);
		void process_append_entries(peer_t * p, msg_sptr msg);
		void process_append_entries_res(peer_t * p, msg_sptr msg);
		void reset_heartbeat_timer();
		void reset_elec_timeout_event();
		void reset_reconnect_timer(peer_t * p);
		void turn_into_candidate();
		void turn_into_follower();
		void claim_leadership();
		struct reactor * get_reactor() {return &r;};
	private:
		void remove_event_if_in_reactor(struct event * e);
		void peer_cleanup(peer_t * p);
		bool compare_log_to_local(w_int_t last_log_idx, w_int_t last_log_term);
		inline file_mapped_t * get_fmapped() {
			return static_cast<file_mapped_t *>(map->get_addr());
		}
		std::unique_ptr<file_mmap> 		map;
		std::unique_ptr<logger>			log;
		std::unique_ptr<config> 		cfg;
		std::string                     cfg_file;
		std::map<w_addr_t, peer_t,
				 WADDR_PRED>            peers, servers, clients;
		w_int_t							state;
		w_int_t							commit_index;
		w_int_t							last_applied;
		struct reactor                  r;
		struct event                    elec_timeout_event;
		/* heartbeat timer */
		struct event                    hb_timeout_event;
		/* peer-used only */
		w_int_t                         listen_port;
		struct event                    listen_event;
		el_socket_t                     listen_fd;
		/* serving for clients */
		w_int_t                         serving_port;
		struct event                    serving_event;
		el_socket_t                     serving_fd;
		w_addr_t                        self;
		peer_t                         *cur_leader;
		w_uint_t                        vote_count;
		
	};

}