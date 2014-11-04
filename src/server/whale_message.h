/*
* Copyright (C) Xinjing Cho
*/

#ifndef WHALE_MESSAGE_H_
#define WHALE_MESSAGE_H_
#include <memory>
#include <vector>

#include <define.h>
#include <message.h>
#include <util.h>

#include <whale_log.h>

namespace whale {


	/*
	* JSON format: 
	* {
	*	"term" : 1,
	*	"candidate_id" : {
	*		"ip" : "192.168.1.2",
	*		"port" : 8888
	*	},
	*	"last_log_idx" : 1,
	*	"last_log_term" : 2
	* }
	*/
	typedef struct request_vote_s {
		w_int_t		term;			/* cadidate's term */
		w_addr_t	candidate_id;	/* candidate requesting vote */
		w_int_t		last_log_idx;	/* index of candidate's last log entry */
		w_int_t		last_log_term;	/* term of candidate's last log entry */
	} request_vote_t;

	typedef std::shared_ptr<request_vote_t> rv_sptr;
	typedef std::unique_ptr<request_vote_t> rv_uptr;

	/*
	* JSON format: 
	* {
	*	"term" : 1,
	*	"vote_granted" : true
	* }
	*/
	typedef struct request_vote_res_s {
		w_int_t		term;			/* current term on the server, for candidate to update itself */
		bool		vote_granted;	/* true if candidate got a vote */
	} request_vote_res_t;
	
	typedef std::shared_ptr<request_vote_res_t> rvr_sptr;
	typedef std::unique_ptr<request_vote_res_t> rvr_uptr;
	/*
	* JSON format: 
	* {
	*	"term" : 1,
	*	"leader_id" : {
	*		"ip": "192.168.1.2",
	*		"port": 8888
	*	},
	*	"prev_log_idx" : 0,
	*	"prev_log_term" : 0,
	*	"etnries":[
	*		{
	*			"term"  : 1,
	*			"index" : 1,
	*			"data"  : "set a 1"
	*		},
	*		{
	*			"term"  : 1,
	*			"index" : 2,
	*			"data"  : "add a 1"
	*		}
	*	],
	*	"leader_commit": 2
	* }
	*/
	typedef struct append_entries_s {
		w_int_t					term;			/* leader's term */
		w_addr_t				leader_id;		/* leader's id */
		w_int_t					prev_log_idx;	/* log index of log entry immediately preceeding new ones */
		w_int_t					prev_log_term;	/* term of prev_log_idx entry */
		std::vector<log_entry> 	entries;		/* log entries to store (empty for heartbeat message) */
		w_int_t					leader_commit;	/* leader's commit_idx */
	} append_entries_t;

	typedef std::shared_ptr<append_entries_t> ae_sptr;
	typedef std::unique_ptr<append_entries_t> ae_uptr;

	/*
	* JSON format: 
	* {
	*	"term" : 1,
	*	"success" : true
	* }
	*/
	typedef struct append_entries_res_s {
		w_int_t		term;		/* current term on the server, for candidate to update itself */
		bool		success;	/* true if follower contained entry matching prev_log_idx and prev_log_term*/
	} append_entries_res_t;

	typedef std::shared_ptr<append_entries_res_t> aer_sptr;
	typedef std::unique_ptr<append_entries_res_t> aer_uptr;

	message_t * make_msg_from_request_vote(const request_vote_t & r);
	message_t * make_msg_from_request_vote_res(const request_vote_res_t & r);
	message_t * make_msg_from_append_entries(const append_entries_t & r);
	message_t * make_msg_from_append_entries_res(const append_entries_res_t & r);

	request_vote_t 		* make_request_vote_from_msg(const message_t & m);
	request_vote_res_t 	* make_request_vote_res_from_msg(const message_t & m);
	append_entries_t 	* make_append_entries_from_msg(const message_t & m);
	append_entries_res_t * make_append_entries_res_from_msg(const message_t & m);
}
#endif