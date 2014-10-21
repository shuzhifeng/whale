/*
* Copyright (C) Xinjing Cho
*/
#include <cstdarg>
#include <memory>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <whale_message.h>

namespace whale {

	std::string string_format(const std::string fmt_str, ...) {
	    w_int_t final_n, n = ((w_int_t)fmt_str.size()) << 2; /* reserve 4 times as much as the length of the fmt_str */
	    std::unique_ptr<char[]> formatted;
	    va_list ap;

	    while(1) {
	        formatted.reset(new char[n]); /* wrap the plain char array into the unique_ptr */
	        strcpy(&formatted[0], fmt_str.c_str());

	        va_start(ap, fmt_str);
	        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
	        va_end(ap);

	        if (final_n < 0 || final_n >= n)
	            n += abs(final_n - n + 1);
	        else
	            break;
	    }
	    return std::string(formatted.get());
	}

	static std::string w_addr_to_json(const std::string & key_name, 
									  const w_addr_t & addr) {
		return std::move(string_format("\"%s\":{\"ip\":\"%s\","
											"\"port\":%u}",
											key_name.c_str(),
											inet_ntoa(addr.addr.sin_addr),
											addr.addr.sin_port));
	}

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
	make_message_from_request_vote(request_vote_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,%s,"
							"\"last_log_idx\":%d,"
							"\"last_log_term\":%d}",
							r.term,
							std::move(w_addr_to_json("candidate_id", r.candidate_id)).c_str(),
							r.last_log_idx,
							r.last_log_term
							)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = htonl(MESSAGE_REQUEST_VOTE);
		m->len = htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_message_from_request_vote_res(request_vote_res_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,"
							"\"vote_granted\":%s}",
							r.term,
							r.vote_granted ? "true" : "false"
							)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = htonl(MESSAGE_REQUEST_VOTE_RES);
		m->len = htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_message_from_append_entries(append_entries_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,%s,"
							"\"prev_log_idx\":%d"
							"\"prev_log_term\":%d,"
							"%s,"
							"\"leader_commit\":%d}",
							r.term,
							std::move(w_addr_to_json("leader_id", r.leader_id)).c_str(),
							r.prev_log_idx,
							r.prev_log_term,
							std::move(log_entries_to_json("entries", r.entries)).c_str(),
							r.leader_commit
							)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = htonl(MESSAGE_APPEND_ENTRIES);
		m->len = htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}

	message_t *
	make_message_from_append_entries_res(append_entries_res_t & r) {
		std::string  json(std::move(string_format("{\"term\":%d,"
							"\"success\":%s}",
							r.term,
							r.success ? "true" : "false"
							)));
		message_t * m = reinterpret_cast<message_t *>(new char[sizeof(message_t) + json.size()]);

		m->msg_type = htonl(MESSAGE_APPEND_ENTRIES_RES);
		m->len = htonl(sizeof(message_t) + json.size());
		strcpy(m->data, json.c_str());

		return m;
	}


	request_vote_t *
	make_request_vote_from_message(message_t & m) {
		return nullptr;
	}

	request_vote_res_t *
	make_request_vote_res_from_message(message_t & m) {
		return nullptr;
	}

	append_entries_t *
	make_append_entries_from_message(message_t & m) {
		return nullptr;
	}

	append_entries_res_t *
	make_append_entries_res_from_message(message_t & m) {
		return nullptr;
	}
}