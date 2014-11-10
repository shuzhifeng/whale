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

#include <whale_message.h>

namespace whale {
	static std::string log_entry_to_json(const log_entry & entry) {
		return std::move(string_format("{\"term\":%d,"
										"\"index\":%d,"
										"\"data\":\"%s\"}",
										entry.term,
										entry.index,
										entry.data.c_str()));
	}

	static std::string log_entries_to_json(const std::string & key_name,
										   const std::vector<log_entry> logs) {
		std::string json = key_name + ":[";
		for (const log_entry & entry : logs) {
			if (json.size() > key_name.size() + 2) {
				json.append(",");
			}

			json.append(std::move(log_entry_to_json(entry)));
		}

		return json.append("]");
	}

	message_t *
	make_msg_from_request_vote(const request_vote_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,%s,"
							"\"last_log_idx\":%d,"
							"\"last_log_term\":%d}",
							r.term,
							std::move(w_addr_to_json("candidate_id", r.candidate_id)).c_str(),
							r.last_log_idx,
							r.last_log_term
							)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_REQUEST_VOTE);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_msg_from_request_vote_res(const request_vote_res_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,"
							"\"vote_granted\":%d}",
							r.term,
							r.vote_granted)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_REQUEST_VOTE_RES);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_msg_from_append_entries(const append_entries_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,%s,"
							"\"prev_log_idx\":%d"
							"\"prev_log_term\":%d,"
							"%s,"
							"\"leader_commit\":%d,}"
							"\"heartbeat\": %d}",
							r.term,
							std::move(w_addr_to_json("leader_id", r.leader_id)).c_str(),
							r.prev_log_idx,
							r.prev_log_term,
							std::move(log_entries_to_json("entries", r.entries)).c_str(),
							r.leader_commit,
							r.heartbeat)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_APPEND_ENTRIES);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_msg_from_append_entries_res(const append_entries_res_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,"
							"\"success\":%d"
							"\"heartbeat\":%d}",
							r.term,
							r.success,
							r.heartbeat)));

		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = ::htonl(MESSAGE_APPEND_ENTRIES_RES);
		m->len = ::htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}


	request_vote_t *
	make_request_vote_from_msg(const message_t & m) {
		struct xson_context              ctx;
		struct xson_element             *root;
		std::unique_ptr<request_vote_t>  r;
		char                             ip_buf[50] = {0};
		w_int_t                          port;
		
		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		r = std::unique_ptr<request_vote_t>(new request_vote_t);

		if (xson_get_intptr_by_expr(root, "term", &r->term))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "last_log_idx", &r->last_log_idx))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "last_log_term", &r->last_log_term))
			return nullptr;

		if (xson_get_string_by_expr(root, "candidate_id.ip", ip_buf, sizeof(ip_buf)))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "candidate_id.port", &port))
			return nullptr;

		inet_aton(ip_buf, &r->candidate_id.addr.sin_addr);
		r->candidate_id.addr.sin_port = ::htons(port);

		return r.release();
	}

	request_vote_res_t *
	make_request_vote_res_from_msg(const message_t & m) {
		struct xson_context                  ctx;
		struct xson_element                 *root;
		std::unique_ptr<request_vote_res_t>  r;
		w_int_t                              vote_granted;

		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		r = std::unique_ptr<request_vote_res_t>(new request_vote_res_t);

		if (xson_get_intptr_by_expr(root, "term", &r->term))
			return nullptr;
		
		if (xson_get_intptr_by_expr(root, "vote_granted", &vote_granted))
			return nullptr;

		r->vote_granted = vote_granted;

		return r.release();
	}

	append_entries_t *
	make_append_entries_from_msg(const message_t & m) {
		struct xson_context                ctx;
		struct xson_element               *root;
		std::unique_ptr<append_entries_t>  a;
		char                               ip_buf[50] = {0};
		char                               expr_buf[50] = {0};
		w_int_t                            port;
		w_int_t                            array_size;
		w_int_t                            heartbeat;

		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		if (xson_get_intptr_by_expr(root, "term", &a->term))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "prev_log_idx", &a->prev_log_idx))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "prev_log_term", &a->prev_log_term))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "leader_commit", &a->leader_commit))
			return nullptr;

		if (xson_get_string_by_expr(root, "leader_id.ip", ip_buf, sizeof(ip_buf)))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "leader_id.port", &port))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "heartbeat", &heartbeat))
			return nullptr;

		inet_aton(ip_buf, &a->leader_id.addr.sin_addr);
		a->leader_id.addr.sin_port = ::htons(port);
		a->heartbeat = heartbeat;

		array_size = xson_get_arraysize_by_expr(root, "entries");

		for(int i = 0; i < array_size; ++i) {
			int                  string_size;
			std::unique_ptr<char[]>  p;

			a->entries.push_back(log_entry());

			sprintf(expr_buf, "entries[%d].term", i);

			if (xson_get_intptr_by_expr(root, expr_buf, (intptr_t*)&a->entries.back().term))
				return nullptr;

			sprintf(expr_buf, "entries[%d].index", i);

			if (xson_get_intptr_by_expr(root, expr_buf, (intptr_t*)&a->entries.back().index))
				return nullptr;

			sprintf(expr_buf, "entries[%d].data", i);

			string_size = xson_get_stringsize_by_expr(root, expr_buf);

			if (string_size < 0)
				return nullptr;

			p = std::unique_ptr<char[]>(new char[string_size]);

			if (xson_get_string_by_expr(root, expr_buf, p.get(), string_size))
				return nullptr;

			a->entries.back().data.assign(p.get(), string_size);
		}

		return a.release();
	}

	append_entries_res_t *
	make_append_entries_res_from_msg(const message_t & m) {
		struct xson_context                    ctx;
		struct xson_element                   *root;
		std::unique_ptr<append_entries_res_t>  a;
		w_int_t                                success;
		w_int_t                                heartbeat;

		if (xson_init(&ctx, m.data))
			return nullptr;
		
		if (xson_parse(&ctx, &root) != XSON_RESULT_SUCCESS)
			return nullptr;

		a = std::unique_ptr<append_entries_res_t>(new append_entries_res_t);

		if (xson_get_intptr_by_expr(root, "term", &a->term))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "success", &success))
			return nullptr;

		if (xson_get_intptr_by_expr(root, "heartbeat", &heartbeat))
			return nullptr;

		a->success = success;
		a->heartbeat = heartbeat;

		return a.release();
	}
}