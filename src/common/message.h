/*
* Copyright (C) Xinjing Cho
*/

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <define.h>

namespace whale {
	#define MESSAGE_REQUEST_VOTE 		0
	#define MESSAGE_REQUEST_VOTE_RES	1
	#define MESSAGE_APPEND_ENTRIES		2
	#define MESSAGE_APPEND_ENTRIES_RES	3
	#define MESSAGE_CMD_REQUEST         4
	#define MESSAGE_CMD_REQUEST_RES     5

	#define MESSAGE_PAYLOAD_LEN(m) ((m)->len - sizeof(int32_t))
	#define MESSAGE_SIZE(m)        (::ntohl((m)->len))
	/*
	* Generic message sent over the network.
	*/
	typedef struct message_s {
		/* the folliwing two fields are in network byte order */
		uint32_t len;           /* size of the struct */
		uint32_t msg_type;      /* message type */
		char 	 data[];        /* actual payload */
	} message_t;


	typedef std::shared_ptr<message_t> msg_sptr;
	typedef std::unique_ptr<message_t> msg_uptr;

	typedef struct cmd_request_s {
		std::string cmd;
	} cmd_request_t;

	typedef struct cmd_reuqest_res_s {
		w_addr_t    leader;
		bool        res;
	} cmd_request_res_t;

	message_t * make_msg_from_cmd_request(const cmd_request_t & c);
	message_t * make_msg_from_cmd_request_res(const cmd_request_res_t & cr);

	cmd_request_t 		* make_cmd_request_from_msg(const message_t & m);
	cmd_request_res_t 	* make_cmd_request_res_from_msg(const message_t & m);
}
#endif