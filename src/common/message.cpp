/*
* Copyright (C) Xinjing Cho
*/
#include <cstdarg>
#include <memory>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <xson/parser.h>

#include <message.h>
#include <util.h>

namespace whale {
	
	message_t * make_msg_from_cmd_request(const cmd_request_t & c) {
		std::string  json(std::move(string_format("{\"cmd\":\"%s\"}",
		                                          c.cmd.c_str())));

		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_CMD_REQUEST);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t * make_msg_from_cmd_request_res(const cmd_request_res_t & cr) {
		std::string  json(std::move(string_format("{%s,"
						  "\"res\":%d}",
						  std::move(w_addr_to_json("leader", cr.leader)).c_str(),
						  cr.res
						  )));

		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_CMD_REQUEST);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	cmd_request_t 	  * make_cmd_request_from_msg(const message_t & m) {
		struct xson_context                  ctx;
		struct xson_element                 *root;
		std::unique_ptr<cmd_request_t>       c;
		std::unique_ptr<char[]>              p;
		w_int_t                              string_size;

		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		c = std::unique_ptr<cmd_request_t>(new cmd_request_t);

		string_size = xson_get_stringsize_by_expr(root, "cmd");

		if (string_size < 0)
			return nullptr;

		p = std::unique_ptr<char[]>(new char[string_size]);

		if (xson_get_string_by_expr(root, "cmd", p.get(), string_size))
				return nullptr;

		c->cmd.assign(p.get(), string_size);

		return c.release();
	}

	cmd_request_res_t * make_cmd_request_res_from_msg(const message_t & m) {
		struct xson_context                 ctx;
		struct xson_element                *root;
		std::unique_ptr<cmd_request_res_t>  cr;
		char                                ip_buf[50] = {0};
		w_int_t                             port;
		w_int_t                             res;

		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		cr = std::unique_ptr<cmd_request_res_t>(new cmd_request_res_t);

		if (xson_get_string_by_expr(root, "leader.ip", ip_buf, sizeof(ip_buf)))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "leader.port", &port))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "res", &res))
			return nullptr;

		inet_aton(ip_buf, &cr->leader.addr.sin_addr);
		cr->leader.addr.sin_port = ::htons(port);
		cr->res = res;

		return cr.release();
	}
}
