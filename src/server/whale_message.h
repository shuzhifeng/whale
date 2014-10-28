/*
* Copyright (C) Xinjing Cho
*/

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <vector>

#include <define.h>

#include <whale_log.h>

namespace whale {

	#define MESSAGE_REQUEST_VOTE 		0
	#define MESSAGE_REQUEST_VOTE_RES	1
	#define MESSAGE_APPEND_ENTRIES		2
	#define MESSAGE_APPEND_ENTRIES_RES	3

	#define MESSAGE_PAYLOAD_LEN(m) ((m)->len - sizeof(int32_t))
	#define MESSAGE_SIZE(m)        (::ntohl((m)->len))
	/*
	* Generic message sent over the network.
	*/
	typedef struct message_s {
		/* the folliwing two fields are in network byte order */
		int32_t len;		/* size of the struct */
		int32_t	msg_type;	/* message type */
		char 	data[];		/* actual payload */
	} message_t;

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

	message_t * make_message_from_request_vote(request_vote_t & r);
	message_t * make_message_from_request_vote_res(request_vote_res_t & r);
	message_t * make_message_from_append_entries(append_entries_t & r);
	message_t * make_message_from_append_entries_res(append_entries_res_t & r);

	request_vote_t 		* make_request_vote_from_message(message_t & m);
	request_vote_res_t 	* make_request_vote_res_from_message(message_t & m);
	append_entries_t 	* make_append_entries_from_message(message_t & m);
	append_entries_res_t * make_append_entries_res_from_message(message_t & m);
}
#endif