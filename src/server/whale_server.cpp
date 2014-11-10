/*
* Copyright (C) Xinjing Cho
*/
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <log.h>

#include <cheetah/reactor.h>

#include <whale_server.h>
#include <whale_message.h>

namespace whale {
	typedef std::map<w_addr_t, peer_t>::iterator peer_it;
	typedef std::unique_ptr<message_t>      m_uptr;

	std::random_device rd;

	#define NEXT_TIMEOUT(mi, ma) ((mi) + rd() % ((ma) - (mi) + 1))

	/*
	* set @fd to nonblocking mode.
	* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
	*/
	static w_rc_t set_fd_nonblocking(el_socket_t fd) {
		int flags = 0;

		if ((flags = fcntl(fd, F_GETFL)) == -1) {
			log_error("failed to fcntl(fd, F_GETFL) on fd[%d]: %s",
			          fd, ::strerror(errno));
			return WHALE_ERROR;
		}

		if ((flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1){
			log_error("failed to set nonblocking mode for fd[%d]: %s",
			          fd, ::strerror(errno));
			return WHALE_ERROR;
		}

		return WHALE_GOOD;
	}

	/*
	* create a tcp socket and listen on it.
	* Return: file descriptor of that socket on success, -1 on failure.
	*/
	static el_socket_t make_listen_fd(w_addr_t * addr) {
		el_socket_t fd;

		fd = ::socket(AF_INET, SOCK_STREAM, 0);

		if (fd == -1) {
			log_error("failed to ::socket: %s", ::strerror(errno));
			return -1;
		}

		if (::bind(fd, (struct sockaddr*)&addr->addr,
		           sizeof(struct sockaddr))) {
			log_error("failed to ::bind: %s", ::strerror(errno));
			return -1;
		}
		
		if (::listen(fd, WHALE_BACKLOG)) {
			log_error("failed to ::listen: %s", ::strerror(errno));
			return -1;
		}

		return fd;
	}

	/*
	* asynchronous connect syscall callback
	*/
	static void 
	connect_callback(el_socket_t fd, short res_flags, void *arg) {
		peer_t *peer = static_cast<peer_t*>(arg);

		if (::connect(fd, (struct sockaddr*)&peer->addr.addr,
			          sizeof(struct sockaddr))) {
			if (errno != EINPROGRESS) {
				log_error("failed to ::connect to fd[%d]: %s",
			          fd, ::strerror(errno));
				peer->server->remove_event_if_in_reactor(&peer->connect_e);
				TEMP_FAILURE_RETRY(close(fd));
				return;
			}
			log_error("failed to ::connect to fd[%d], try later: %s",
			          fd, ::strerror(errno));
			return;
		}

		/* connected */
		peer->server->set_up_peer_events(peer, fd);

		peer->server->remove_event_if_in_reactor(&peer->connect_e);
	}
	/*
	* connect to a peer. 
	* if succeed, peer.conncted will be set.
	* Return: file descriptor of that peer on success, WHALE_ERROR on failure.
	*/
	static el_socket_t connect_to_peer(peer_t & peer) {
		el_socket_t        fd;

		fd = ::socket(AF_INET, SOCK_STREAM, 0);

		if (fd == -1) {
			log_error("failed to ::socket: %s", ::strerror(errno));
			return WHALE_ERROR;
		}

		if (set_fd_nonblocking(fd) != WHALE_GOOD) {
			log_error("failed to set_fd_nonblocking for fd[%d]", fd);
			goto fail;
		}

		if (::connect(fd, (struct sockaddr*)&peer.addr.addr,
			          sizeof(struct sockaddr))) {
			if (errno != EINPROGRESS) {
				log_error("failed to ::connect to fd[%d]: %s",
			          fd, ::strerror(errno));
				goto fail;
			}
			log_error("failed to ::connect to fd[%d], try later: %s",
			          fd, ::strerror(errno));

			peer.server->remove_event_if_in_reactor(&peer.connect_e);

			event_set(&peer.connect_e, fd, E_READ, connect_callback, &peer);
			if (reactor_add_event(peer.server->get_reactor(), &peer.connect_e)) {
				log_error("failed to reactor_add_event connect event: %s",
		                  ::strerror(errno));
				goto fail;
			}
			
			return WHALE_ERROR;
		}
		peer.connected = true;
		return fd;

	fail:
		peer.server->remove_event_if_in_reactor(&peer.connect_e);
		TEMP_FAILURE_RETRY(close(fd));

		return WHALE_ERROR;
	}

	/*
	* gets called when a peer connect to this machine.
	*/
	static void
	server_listen_callback(el_socket_t fd, short res_flags, void *arg) {
		whale_server * s = static_cast<whale_server *>(arg);
		s->handle_listen_fd();
	}

	/*
	* gets called when a peer connect to this machine.
	*/
	static void
	serving_callback(el_socket_t fd, short res_flags, void *arg) {
		whale_server * s = static_cast<whale_server *>(arg);
		s->handle_serving_listen_fd();
	}

	/*
	* gets called when candidate or leader sends data as a follower.
	*/
	static void
	peer_callback(el_socket_t fd, short res_flags, void *arg) {
		peer_t       *peer = static_cast<peer_t*>(arg);
		whale_server *s = peer->server;

		if (res_flags & E_READ) {
			s->handle_read_from_peer(peer);
		}

		if (res_flags & E_WRITE) {
			s->handle_write_to_peer(peer);
		}
	}

	/*
	* gets called to send heartbeat to servers
	*/
	static void
	hearbeat_callback(el_socket_t fd, short res_flags, void *arg) {
		whale_server * s = static_cast<whale_server*>(arg);
		s->send_heartbeat();
		s->reset_heartbeat_timer();
	}

	/*
	* handles read and write messages from/to a peer as a candidate or leader.
	*/
	static void
	server_callback(el_socket_t fd, short res_flags, void *arg) {
		peer_t       *peer = static_cast<peer_t*>(arg);
		whale_server *s = peer->server;

		if (res_flags & E_READ) {
			s->handle_read_from_peer(peer);
		}

		if (res_flags & E_WRITE) {
			s->handle_write_to_peer(peer);
		}
	}

	/*
	* gets called after a amount of time randomly generated by the last 
	* NEXT_TIMEOUT call.
	*/
	static void
	elec_timeout_callback(el_socket_t fd, short res_flags, void *arg) {
		whale_server * s = static_cast<whale_server*>(arg);
		s->turn_into_candidate();
	}

	/*
	* gets called after WHALE_RECONNECT_TIMEOUT ms to reconnect
	* to those peers not connected to this machine yet.
	*/
	static void 
	peer_reconnect_callback(el_socket_t, short res_flags, void*arg){
		peer_t * peer = static_cast<peer_t*>(arg);

		if(peer->connected) return;

		el_socket_t fd;

		fd = connect_to_peer(*peer);

		if (fd >= 0) {
			peer->server->set_up_peer_events(peer, fd);
		} else {
			/* reset timer */
			peer->server->reset_reconnect_timer(peer);
		}
	}

	/*
	* tests whether @str is a ip string(xxx.xxx.xxx.xxx[:port]).
	*/
	static bool is_ip(std::string str) {
		std::unique_ptr<char[]> b = std::unique_ptr<char[]>(new char[str.size() + 1]);
		::strcpy(b.get(), str.c_str());
		char *save_ptr;
		char *p = ::strtok_r(b.get(), ".", &save_ptr);
		w_int_t cnt = 0, val;

		while (p) {
			++cnt;
			val = std::stol(p);

			if (val > 255 || val < 0)
				return false;

			p = ::strtok_r(NULL, ".", &save_ptr);
		}

		return cnt == 4;
	}

	static const char * get_peer_hostname(w_addr_t & addr) {
		struct hostent * e;

		if ((e = gethostbyaddr(&addr.addr.sin_addr, sizeof(in_addr), AF_INET)) == nullptr) {
			log_error("error on gethostbyaddr: %s", ::hstrerror(h_errno));
			return nullptr;
		}

		return e->h_name;
	}

	void whale_server::set_up_peer_events(peer_t * p, el_socket_t fd) {
		/* connected */
		struct event * e = &p->e;

		event_set(e, fd, E_READ | E_WRITE, server_callback, p);

		if (reactor_remove_event(p->server->get_reactor(),
		                         &p->timeout_e) == -1)
		    log_error("failed to reactor_remove_event "
		              " reconnecting timer event: %s", ::strerror(errno));

		/* remove @e from the reactor if @e was previously in the reactor.*/
		if (!event_in_reactor(e) &&
		    reactor_remove_event(p->server->get_reactor(), e) == -1) {
			log_error("failed to reactor_remove_event "
		              " stale event: %s", ::strerror(errno));
		}

		/* add @e back to the reactor. */
		if (reactor_add_event(p->server->get_reactor(), e) == -1) {
			log_error("failed to reactor_add_event "
		              "new event: %s", ::strerror(errno));
		}
	}
	/*
	* compare candidate's log to receiver's.
	* Return: true if candidate's log is at least as up-to-date as receiver's, false otherwise.
	* @last_log_idx: candidate's last log index.
	* @last_log_term: candidate's last log term.
	*/
	bool whale_server::compare_log_to_local(w_int_t last_log_idx,
	                                        w_int_t last_log_term) {
		return last_log_term > this->log->get_last_log().term ||
			   (last_log_term == this->log->get_last_log().term &&
			   	last_log_idx >= this->log->get_last_log().index);
	}

	void whale_server::process_request_vote(peer_t * p, msg_sptr msg) {
		static char dummy[50];

		rv_uptr rv{make_request_vote_from_msg(*msg)};
		bool    granted = false;

		/*
		* grant if all of following conditions are true:
		*     1. candidate's term >= currentTerm
		*     2. votedFor is null(haven't voted for anyone) or the candidate in question.
		*     3. candidate's log is at least as up-to-date as receiver's.
		*/
		if (rv->term >= get_fmapped()->current_term &&
			(!::memcmp(&get_fmapped()->voted_for, dummy,
		               sizeof(struct sockaddr_in)) || 
			 !::memcmp(&get_fmapped()->voted_for, &rv->candidate_id.addr,
			           sizeof(struct sockaddr_in))) &&
			compare_log_to_local(rv->last_log_idx, rv->last_log_term)) {

			::memcpy(&get_fmapped()->voted_for, &p->addr.addr,
			         sizeof(struct sockaddr_in));
			get_fmapped()->current_term = rv->term;
			this->map->sync();
			granted = true;

			/* reset election timeer event */
			reset_elec_timeout_event();
		}

		/*
		* make request vote result message accordingly.
		*/
		msg_q_elt elt{0, 0};
		elt.msg = msg_sptr(make_msg_from_request_vote_res({
				                     get_fmapped()->current_term, granted}));

		p->write_queue.push(elt);

		handle_write_to_peer(p);
	}

	void whale_server::send_heartbeat() {
		append_entries_t a;
		a.term = get_fmapped()->current_term;
		::memcpy(&a.leader_id.addr, &this->self.addr, sizeof(struct sockaddr_in));
		a.prev_log_idx = this->log->get_last_log().index;
		a.prev_log_term = this->log->get_last_log().term;
		a.leader_commit = this->commit_index;
		a.heartbeat = true;

		/* make a generic message out of append entries struct */
		msg_sptr p = msg_sptr(make_msg_from_append_entries(a));

		for (auto & it : this->servers) {
			if (!it.second.connected) continue;
			it.second.write_queue.push({0, 0, p});
			handle_write_to_peer(&it.second);
		}
	}

	void whale_server::reset_heartbeat_timer() {
		/* 
		*  connect is in progress or peer is not online,
		*  setup a timer to reconnect after a period of time.
		*/
		struct event * hb_timeout_e = &this->hb_timeout_event;

		remove_event_if_in_reactor(hb_timeout_e);

		event_set(hb_timeout_e, WHLAE_HEARTBEAT_TIMEOUT, E_TIMEOUT,
		          hearbeat_callback, this);

		if (reactor_add_event(&this->r, hb_timeout_e) == -1)
		    log_error("failed to reactor_add_event for"
		              " hearbeat timeer event: %s", ::strerror(errno));
	}
	/*
	* sends initial append entires(heartbeat) to servers to claim to 
	* be a leader, and start a timeout event to send heartbeat repeatedly 
	* after a period of time to prevent followers from starting a new election.
	*/
	void whale_server::claim_leadership() {
		this->state = LEADER;
		/* remove election timer */
		remove_event_if_in_reactor(&this->elec_timeout_event);
		send_heartbeat();
	}

	void whale_server::turn_into_follower(w_int_t term) {
		get_fmapped()->current_term = term;
		this->state = FOLLOWER;
		this->vote_count = 0;
		/* start an election timer */
		reset_elec_timeout_event();
		this->map->sync();
	}

	void whale_server::process_request_vote_res(peer_t * p, msg_sptr msg) {
		rvr_uptr rvr{make_request_vote_res_from_msg(*msg)};
		/* no use of request for request_vote_res */
		p->request_queue.pop();

		/*
		* ignore if current role is not candidate or 
		* we are in a later term (we've become a leader
		* or a follower, or we've had a collision and 
		* started a new term).
		*/
		if (this->state != CANDIDATE ||
			rvr->term < get_fmapped()->current_term) {
			return;
		}

		if (rvr->vote_granted) {
			if (++this->vote_count > (this->peers.size() + 1) / 2) {
				/* whoo, got majority of votes! */
				claim_leadership();
				this->vote_count = 0;
			}
		} else if (rvr->term >= get_fmapped()->current_term) {
			/* a leader is elceted, turn into a follower. */
			turn_into_follower(rvr->term);
		}

	}

	void whale_server::process_append_entries(peer_t * p, msg_sptr msg) {
		ae_uptr ae{make_append_entries_from_msg(*msg)};
		bool    success = false;

		/*
		* reply false if term < currentTerm
		*/
		if (ae->term < get_fmapped()->current_term) {
			goto send_message;
		} else if(ae->term > get_fmapped()->current_term) {
			/* new leader, update self */
			get_fmapped()->current_term = ae->term;
			this->map->sync();
		}

		if (ae->entries.empty()) { /* heartbeat message */
			this->cur_leader = p;
			success = true;
			goto send_message;
		} else { /* normal appending message */
			/* log consistency Check: 
			*  replay false if log doesn’t contain an entry 
			*  at prevLogIndex whose term matches prevLogTerm.
			*/
			log_entry_it it = this->log->find_by_idx(ae->prev_log_idx);

			if (it == this->log->get_entries().end() ||
			    it->term != ae->prev_log_term)
				goto send_message;
		}

		/*
		* log consistency check passed, extraneous entries deletion:
		* If an existing entry conflicts with a new one (same index
		* but different terms), delete the existing entry and all that
        * follow it.
		*/
		for (auto & new_entry : ae->entries ) {
			log_entry_it it = this->log->find_by_idx(new_entry.index);

			if (it == this->log->get_entries().end() ||
				it->term != new_entry.term) {
				this->log->chop(it);
				break;
			}
		}

		this->log->append(ae->entries);
		success = true;

		if (ae->leader_commit > this->commit_index)
			this->commit_index = std::min(ae->leader_commit,
			                              (w_int_t)ae->entries.back().index);
	send_message:

		/*
		* make append entries result message accordingly.
		*/
		msg_q_elt elt{0, 0};
		elt.msg = msg_sptr(make_msg_from_append_entries_res({
				                     get_fmapped()->current_term, success}));

		p->write_queue.push(elt);

		handle_write_to_peer(p);
	}

	void whale_server::push_append_entries(peer_t * p, size_t start) {
		append_entries_t         a;
		std::vector<log_entry_t> entries = this->log->get_entries();
		log_entry_t             &e = entries[entries.size() - 2];
		
		a.prev_log_idx = e.index;
		a.prev_log_term = e.term;
		a.term = get_fmapped()->current_term;
		a.leader_commit = this->commit_index;
		::memcpy(&a.leader_id.addr, &this->self.addr, sizeof(struct sockaddr_in));
		a.heartbeat = false;

		for (size_t i = start; i < entries.size(); ++i ) {
			a.entries.push_back(entries[i]);
		}

		/*
		* make request vote result message accordingly.
		*/
		msg_q_elt elt{0, 0};
		elt.msg = msg_sptr{make_msg_from_append_entries(a)};

		p->write_queue.push(elt);
	}

	/**
	* If there exists an N such that N > commitIndex, a majority
	* of matchIndex[i] ≥ N, and log[N].term == currentTerm:
	* set commitIndex = N.
	*/
	void whale_server::leader_adjust_commit_index() {
		std::vector<log_entry_t> &entries = this->log->get_entries();
		w_int_t                   min_n = this->log->get_last_log().index;
		w_int_t                   max_n = -1;

		for (auto & it : this->servers) {
			if (it.second.match_idx > this->commit_index) {
				log_entry_it eit = this->log->find_by_idx(it.second.match_idx);

				if (eit != entries.end() &&
				    eit->term == get_fmapped()->current_term) {
					min_n = std::min(it.second.match_idx, min_n);
					max_n = std::max(it.second.match_idx, max_n);
				}
			}
		}

		for (w_int_t i = max_n; i >= min_n; --i) {
			w_uint_t maj = 0;
			for (auto & it : this->servers) {
				if (it.second.match_idx > i) {
					++maj;
				}
			}

			if (maj + 1 > (this->servers.size() + 1) / 2) {
				this->commit_index = i;
				break;
			}
		}

	}

	void whale_server::apply_log() {
		if (this->commit_index > this->last_applied) {
			this->last_applied = this->commit_index;
			this->log->commit_until(this->commit_index);
		}
	}

	/*
	* notify client that a command message has been 
	* succesfully processed by the system.
	*/
	void whale_server::reply_success_to_client(peer_t * client) {
		cmd_request_res_t cmdr;
		cmdr.res = true;

		message_queue_elt_s elt{0, 0};
		elt.msg = msg_sptr{make_msg_from_cmd_request_res(cmdr)};

		client->write_queue.push(elt);

		this->handle_write_to_peer(client);
	}

	void whale_server::reply_redirect_to_client(peer_t * client) {
		cmd_request_res_t cmdr;
		cmdr.res = false;

		if (this->cur_leader != nullptr) {
			::memcpy(&cmdr.leader.addr, 
				     &this->cur_leader->addr.addr,
				     sizeof(struct sockaddr_in));
		}

		message_queue_elt_s elt{0, 0};
		elt.msg = msg_sptr{make_msg_from_cmd_request_res(cmdr)};

		client->write_queue.push(elt);

		this->handle_write_to_peer(client);
	}

	void whale_server::reply_clients() {
		for (auto & it : this->clients) {
			/* a command in process */
			if (it.second.cur_cmd.use_count()) {
				if (it.second.cur_cmd->term <= get_fmapped()->current_term &&
					it.second.cur_cmd->index <= this->last_applied) {
					reply_success_to_client(&it.second);
					it.second.cur_cmd.reset();
				}
			}
		}
	}
	void whale_server::process_append_entries_res(peer_t * p, msg_sptr msg) {
		aer_uptr aes = aer_uptr{make_append_entries_res_from_msg(*msg)};
		
		if (this->state != LEADER || aes->heartbeat)
			return;

		if (aes->term > get_fmapped()->current_term) {
			/* a leader is reelceted, turn into a follower. */
			turn_into_follower(aes->term);
		} else if (aes->success) {
			p->match_idx = p->next_idx++;
			leader_adjust_commit_index();
			apply_log();
			reply_clients();
		} else {
			p->match_idx = (--p->next_idx - 1);

			/* resend entries starting at p->next_idx */
			if (this->log->get_entries().size() > 1) {
				push_append_entries(p, p->next_idx);
				handle_write_to_peer(p);
			}
		}
	}

	void whale_server::send_append_entries() {
		log_entry_t & last = this->log->get_last_log();

		/**
		* If last log index ≥ nextIndex for a follower: send
		* AppendEntries RPC with log entries starting at nextIndex
		*/
		for (auto & it : this->servers) {
			if (it.second.next_idx < last.index) {
				push_append_entries(&it.second, it.second.next_idx);
				handle_write_to_peer(&it.second);
			}
		}
	}

	void whale_server::process_cmd_request(peer_t * p) {
		p->cur_cmd = p->c_queue.front();
		p->c_queue.pop();

		/* redirect client to real leader */
		if (this->state != LEADER) {
			p->cur_cmd.reset();
			reply_redirect_to_client(p);

			return;
		}

		log_entry_t &e = this->log->get_last_log();
		
		this->log->get_entries().push_back({e.index + 1, 
			                                get_fmapped()->current_term, 
			                                p->cur_cmd->cmd});
		/*
		* for future reply to client.
		*/
		p->cur_cmd->index = e.index + 1;
		p->cur_cmd->term = get_fmapped()->current_term;

		send_append_entries();
	}

	/*
	* handles fully read messages from peer's read_queue.
	*/
	void whale_server::process_messages(peer_t * p) {
		msg_queue & q = p->read_queue;
		while (!q.empty()) {
			msg_q_elt & elt = q.front();

			/* process complete message only */
			if (elt.pin < ::htonl(elt.msg->len))
				break;

			switch (::htonl(elt.msg->msg_type)) {
			case MESSAGE_REQUEST_VOTE:
				process_request_vote(p, elt.msg);
				break;
			case MESSAGE_REQUEST_VOTE_RES:
				process_request_vote_res(p, elt.msg);
				break;
			case MESSAGE_APPEND_ENTRIES:
				process_append_entries(p, elt.msg);
				break;
			case MESSAGE_APPEND_ENTRIES_RES:
				process_append_entries_res(p, elt.msg);
				break;
			case MESSAGE_CMD_REQUEST:
				p->c_queue.push(cmd_sptr{make_cmd_request_from_msg(*elt.msg.get())});
				if (p->cur_cmd.get() == nullptr) {
					process_cmd_request(p);
				}
				break;
			}

			q.pop();
		}
	}

	/* 
	* read as many as messages from peer until ::read() returns EAGAIN,
	* and start processing messages.
	*/
	void whale_server::handle_read_from_peer(peer_t * p) {
		el_socket_t fd = p->e.fd;
		int32_t     nread = 0;
		uint32_t    size;
		msg_q_elt  *elt;

		/* 
		* special case for empty queue
		*/
		if (p->read_queue.empty()) {
			p->read_queue.push({0, 0, msg_sptr()});
		}

	again:
		elt = &p->read_queue.back();

		/* 
		* read the length of the incoming message, 
		* and construct a message with size of @elt->tmp_len.
		*/
		if (elt->msg.use_count() == 0) {

			while (elt->pin < sizeof(uint32_t)) {
				nread = ::read(fd, ((char *)&elt->tmp_len) + elt->pin,
				               sizeof(uint32_t));

				if (nread <= 0) {
					if (nread == 0) { /* peer closed connection */
						peer_cleanup(p);
						return;
					} else if (errno == EINTR) {/* retry */
						continue;
					} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
						goto process_messages_;
					}
					/* error occured */
					log_error("error occured during ::read() from fd[%d]: %s",
						      fd, ::strerror(errno));
					::abort();
				}
				elt->pin += nread;
			}
			/* got the length for @elt->msg */

			elt->msg = msg_sptr(reinterpret_cast<message_t *>(
				                new char[::htonl(elt->tmp_len)]));

			if (elt->msg.use_count() == 0) {
				/* memory shortage, retry later*/
				log_error("failed to allocate %d bytes for message",
						  ::htonl(elt->tmp_len));
				goto process_messages_;
			}

			elt->pin = 0;
		}

		size = MESSAGE_SIZE(elt->msg);

		while (elt->pin < size) {
			nread = ::read(fd, elt->msg.get() + elt->pin, size - elt->pin);

			if (nread <= 0) {
				if (nread == 0) { /* peer closed connection */
					peer_cleanup(p);
					return;
				} else if (errno == EINTR) { /* retry */
					continue;
				} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
					goto process_messages_;
				}
				/* error occured */
				log_error("error occured during ::read() from fd[%d]: %s",
					      fd, ::strerror(errno));
				::abort();
			}
			elt->pin += nread;
		}

		/* successfully read an entire message, create a new one for the next*/
		p->read_queue.push({0, 0, msg_sptr()});

		goto again;

	process_messages_:
		process_messages(p);
	}
	

	inline void 
	whale_server::remove_event_if_in_reactor(struct event * e) {
		if (event_in_reactor(e))
			reactor_remove_event(&this->r, e);
	}

	void whale_server::reset_reconnect_timer(peer_t * p) {
		/* 
		*  connect is in progress or peer is not online,
		*  setup a timer to reconnect after a period of time.
		*/
		struct event * timeout_e = &p->timeout_e;

		remove_event_if_in_reactor(timeout_e);

		event_set(timeout_e, WHALE_RECONNECT_TIMEOUT, E_TIMEOUT,
		          peer_reconnect_callback, p);

		if (reactor_add_event(&this->r, timeout_e) == -1)
		    log_error("failed to reactor_add_event for"
		              " reconnecting timeer event: %s", ::strerror(errno));
	}

	void whale_server::peer_cleanup(peer_t * p) {
		p->connected = false;
		msg_queue().swap(p->read_queue);
		msg_queue().swap(p->write_queue);
		remove_event_if_in_reactor(&p->e);
		if (p->need_to_reconnect)
			reset_reconnect_timer(p);

		TEMP_FAILURE_RETRY(close(p->e.fd));
	}

	/*
	* write as many as messages to peer until ::write() returns EAGAIN.
	*/
	void whale_server::handle_write_to_peer(peer_t * p) {
		el_socket_t fd = p->e.fd;

		while (!p->write_queue.empty()) {
			msg_q_elt & elt = p->write_queue.front();
			uint32_t    size = MESSAGE_SIZE(elt.msg);
			int32_t     nwrite = 0;

			if (size == elt.pin) {
				p->write_queue.pop();
				continue;
			}

			while (elt.pin < size) {
				nwrite = ::write(fd, elt.msg.get() + elt.pin, size - elt.pin);

				if(nwrite == -1) {
					/* socket buffer might not have enough available space */
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						break;
					} else if (errno == EINTR) { /* retry */
						continue;
					} else if (errno == EPIPE) { /* peer closed connection */
						peer_cleanup(p);
						return;
					}
					
					/* error occured */
					log_error("error occured during ::write() to fd[%d]: %s",
						      fd, ::strerror(errno));
					::abort();
				}

				elt.pin += nwrite;
			}
		}
	}

	/*
	* reset this local's election timer event.
	*/
	void whale_server::reset_elec_timeout_event() {
		if (event_in_reactor(&this->elec_timeout_event) &&
			reactor_remove_event(&this->r, &this->elec_timeout_event))
			log_error("failed to reactor_remove_event"
			          " election timeout event: %s", ::strerror(errno));

		/* start timer event with a random period of time for election*/
		event_set(&this->elec_timeout_event,
		          NEXT_TIMEOUT(WHALE_MIN_ELEC_TIMEOUT, WHALE_MAX_ELEC_TIMEOUT),
		          E_TIMEOUT, elec_timeout_callback, this);

		if (reactor_add_event(&this->r, &this->elec_timeout_event) == -1)
			log_error("failed to reactor_add_event for"
			          " election timeout event: %s", ::strerror(errno));
	}

	/*
	* election timeout elapsed, starts a new election.
	*/
	void whale_server::turn_into_candidate() {
		/*
		* convert to candidate, set voteFor to null, increment current term.
		*/
		this->state = CANDIDATE;
		::memset(&get_fmapped()->voted_for, 0, sizeof(struct sockaddr_in));
		this->get_fmapped()->current_term++;
		this->map->sync();
		this->vote_count = 1; /* vote for self */
		/*
		* reset elcetion timer in case of collision.
		*/
		this->reset_elec_timeout_event();

		/*
		* broadcasts request vote messages to to connected servers.
		*/
		request_vote_t rv;

		rv.term = this->get_fmapped()->current_term;
		rv.candidate_id = this->self;
		rv.last_log_idx = this->log->get_last_log().index;
		rv.last_log_term = this->log->get_last_log().term;

		/* make a generic message out of request vote struct */
		msg_sptr p = msg_sptr(make_msg_from_request_vote(rv));

		for (auto & it : this->servers) {
			if (!it.second.connected) continue;
			it.second.write_queue.push({0, 0, p});
			handle_write_to_peer(&it.second);
			it.second.request_queue.push({MESSAGE_REQUEST_VOTE, new msg_sptr{p}});
		}
	}

	/*
	* accepts peer connection as peer may restart or restore from partition.
	*/
	void whale_server::handle_listen_fd() {
		w_addr_t    addr;
		el_socket_t peer_fd;
		socklen_t   sock_len = sizeof(struct sockaddr_in);
		peer_it     it;

		peer_fd = TEMP_FAILURE_RETRY(::accept(listen_fd,(struct sockaddr*)&addr.addr, &sock_len));

		if (peer_fd == -1) {
			log_error("failed to ::accept peer connection: %s",
				      ::strerror(errno));
			return;
		}

		it = this->peers.find(addr);

		if (it == std::end(this->peers)) {
			log_error("a peer not in the cfg file trying to connect, rejecting.");
			TEMP_FAILURE_RETRY(close(peer_fd));
			return;
		}

		/* update hostname */
		it->second.addr.name = get_peer_hostname(addr);

		remove_event_if_in_reactor(&it->second.e);

		event_set(&it->second.e, peer_fd, E_READ | E_WRITE,
		          peer_callback, &it->second);

		if (reactor_add_event(&this->r, &it->second.e) == -1)
			log_error("failed to reactor_add_event for peer %s, fd[%d]: %s",
			          it->second.addr.name.c_str(), peer_fd, strerror(errno));
	}

	/*
	* accepts client connections.
	*/
	void whale_server::handle_serving_listen_fd() {
		w_addr_t    addr;
		el_socket_t peer_fd;
		socklen_t   sock_len = sizeof(struct sockaddr_in);
		peer_it     it;

		peer_fd = ::accept(listen_fd, (struct sockaddr*)&addr.addr,
		                   &sock_len);

		if (peer_fd == -1) {
			log_error("failed to ::accept client connection: %s",
				      ::strerror(errno));
			return;
		}
		addr.name = get_peer_hostname(addr);
		
		this->clients.insert(std::pair<w_addr_t, peer_t>(addr, INIT_PEER));

		it = this->clients.find(addr);

		/* client connection, no need to reconnect */
		it->second.need_to_reconnect = false;
		/* update hostname */
		it->second.addr = addr;

		event_set(&it->second.e, peer_fd, E_READ | E_WRITE, peer_callback, &it->second);

		if (reactor_add_event(&this->r, &it->second.e) == -1)
			log_error("failed to reactor_add_event for client %s, fd[%d]: %s",
			          it->second.addr.name.c_str(), peer_fd, strerror(errno));
	}

	void
	whale_server::connect_to_servers() {
		for(auto & it : this->servers) {

			if(it.second.connected) continue;

			el_socket_t fd;

			fd = connect_to_peer(it.second);

			if (fd >= 0) {
				set_up_peer_events(&it.second, fd);
			} else {
				reset_reconnect_timer(&it.second);
			}
		}
	}

	w_rc_t whale_server::init() {
		w_rc_t rc;

		/* parse config file */
		cfg = std::unique_ptr<config>(new config(cfg_file));

		rc = cfg->parse();

		if (rc != WHALE_GOOD)
			return rc;

		/* map_file */
		std::string * map_file = cfg->get("map_file");

		if (map_file == nullptr) {
			log_error("must specify map_file in conf file.");
			return WHALE_CONF_ERROR;
		}

		map = std::unique_ptr<file_mmap>(new file_mmap(*map_file, 
		                                               sizeof(file_mapped_t),
		                                               MAP_FILE_PROT,
		                                               MAP_FILE_FLAGS));
		
		rc = map->map();

		if (rc != WHALE_GOOD)
			return rc;
		/* end of map_file */

		/* log_file */
		std::string * log_file = cfg->get("log_file");

		if (log_file == nullptr) {
			log_error("must specify log_file in conf file.");
			return WHALE_CONF_ERROR;
		}
		
		log = std::unique_ptr<logger>(new logger(*log_file));
		
		rc = log->init();

		if (rc != WHALE_GOOD)
			return rc;
		/* end of log_file */

		/* listen_ip */
		std::string * s_listen_ip = cfg->get("listen_ip");

		if (s_listen_ip == nullptr) {
			log_error("listen_ip is required");
			return WHALE_CONF_ERROR;
		} else if(!is_ip(*s_listen_ip)) {
			log_error("listen_ip is invalid");
			return WHALE_CONF_ERROR;
		}

		this->self.addr.sin_addr.s_addr = inet_addr(s_listen_ip->c_str());
		/* end of listen_ip */

		/* listen_port */
		std::string * s_listen_port = cfg->get("listen_port");

		if (s_listen_port == nullptr)
			listen_port = DEFAULT_LISTEN_PORT;
		else
			listen_port = std::stoi(*s_listen_port);

		if (listen_port < 0 || listen_port > 65535) {
			log_error("listen_port out of range[0-65535]");
			return WHALE_CONF_ERROR;
		}
		/* end of listen_port */

		this->self.name = get_peer_hostname(this->self);


		/* serving_port */

		std::string * s_serving_port = cfg->get("serving_port");

		if (s_serving_port == nullptr)
			serving_port = DEFAULT_SERVING_PORT;
		else
			serving_port = std::stoi(*s_serving_port);

		if (serving_port < 0 || serving_port > 65535) {
			log_error("serving_port out of range[0-65535]");
			return WHALE_CONF_ERROR;
		}
		/* serving_port */

		/* peers */
		char *p;
		char *save_ptr;
		if (cfg->get("peers") == nullptr) {
			log_error("peers is required");
			return WHALE_CONF_ERROR;
		}

		std::unique_ptr<char[]> peer_ips = std::unique_ptr<char[]>(
			                               new char[cfg->get("peers")->size() + 1]);
		::strcpy(peer_ips.get(), cfg->get("peers")->c_str());

		p = ::strtok_r(peer_ips.get(), " ", &save_ptr);

		while (p) {
			char   *port_p;
			int     port;

			if (!is_ip(p)) {
				log_error("invalid peer ip: %s", p);
				return WHALE_CONF_ERROR;
			}

			peer_t peer = INIT_PEER;

			/*
			* internal peer connection,
			* must be scheduled for reconnecting after connection closed.
			*/
			peer.need_to_reconnect = true;

			port_p = strchr(p, ':');

			if (port_p)
				::sscanf(port_p + 1, "%d", &port);
			else
				port = listen_port;

			if (port > 65535 || port < 0) {
				log_error("peer's port out of range[0-65535]");
				return WHALE_CONF_ERROR;
			}

			/* convert to network byte order */
			peer.addr.addr.sin_port = ::htons(port);

			peer.addr.addr.sin_addr.s_addr = ::inet_addr(p);

			peer.addr.addr.sin_family = AF_INET;

			peer.server = this;

			peer.addr.name = get_peer_hostname(peer.addr);

			this->peers.insert(std::pair<w_addr_t, peer_t>(peer.addr, peer));

			this->servers.insert(std::pair<w_addr_t, peer_t>(peer.addr, peer));

			p = ::strtok_r(NULL, " ", &save_ptr);
		}
		/* end of peers */

		/* fire the reactor up */
		reactor_init_with_signal_timer(&r, NULL);

		/* start listening event for peer connection */
		w_addr_t    server_addr;

		bzero(&server_addr.addr, sizeof(struct sockaddr_in));
		server_addr.addr.sin_family = AF_INET;
		server_addr.addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
		server_addr.addr.sin_port = ::htons(listen_port);

		if ((this->listen_fd = make_listen_fd(&server_addr)) == -1) {
			log_error("failed to make_listen_fd: %s", ::strerror(errno));
			return WHALE_ERROR;
		}

		event_set(&this->listen_event, this->listen_fd, E_READ,
		          server_listen_callback, this);

		if (reactor_add_event(&this->r, &this->listen_event) == -1) {
			log_error("failed to reactor_add_event for listen_event[%d]: %s",
			          this->listen_fd, ::strerror(errno));
			return WHALE_ERROR;
		}

		/* start serving event for client connection */
		w_addr_t    serving_addr;

		bzero(&serving_addr.addr, sizeof(struct sockaddr_in));
		serving_addr.addr.sin_family = AF_INET;
		serving_addr.addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
		serving_addr.addr.sin_port = ::htons(serving_port);

		if ((this->serving_fd = make_listen_fd(&serving_addr)) == -1) {
			log_error("failed to make_listen_fd: %s", ::strerror(errno));
			return WHALE_ERROR;
		}

		event_set(&this->serving_event, this->serving_fd, E_READ,
		          serving_callback, this);

		if (reactor_add_event(&this->r, &this->serving_event) == -1) {
			log_error("failed to reactor_add_event for serving_event[%d]: %s",
			          this->serving_fd, ::strerror(errno));
			return WHALE_ERROR;
		}

		/* don't know who is leader yet */
		this->cur_leader = nullptr;

		connect_to_servers();
		reset_elec_timeout_event();

		return WHALE_GOOD;
	}

	/* crank up the reactor */
	void whale_server::run() {
		struct timeval timeout = {0, 100000};
		
		reactor_loop(&this->r, &timeout, 0);
	}
}