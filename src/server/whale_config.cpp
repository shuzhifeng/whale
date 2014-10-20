/*
* Copyright (C) Xinjing Cho
*/
#include <string>
#include <map>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <log.h>

#include <whale_config.h>

namespace whale {

	void config::getline(w_int_t fd, std::string & linebuf){
		static int 		payload = 0;
		static char 	buf[CONF_BUFSIZE];
		static char	   *start = buf;
		static char    *end = buf;
		static bool		eof = false;
		int			nread = 0;

	again:

		end = strchr(start, '\n');

		if (end) {
			linebuf.append(start, end);
			
			start = end = end + 1;

			return;
		}

		linebuf.append(start, buf + payload);

		start = end = buf + payload;

		if (eof)
			return;

		payload = 0;

		/* fill up the whole buffer */
		while ((nread = read(fd, buf + payload,
							 CONF_BUFSIZE - payload)) > 0){
			payload += nread;
		}

		/* reached eof, treat it as the last line*/
		if (payload != CONF_BUFSIZE) {
			eof = true;
			/* strchr might reach a spot after buf + payload */
			memset(buf + payload, 0, CONF_BUFSIZE - payload);
		}

		start = end = buf;

		goto again;
	}

	w_rc_t config::parse() {
		w_int_t	fd = ::open(cfg_file.c_str(), CONF_FILE_FLAGS);
		w_int_t	lineno = 1;

		if (fd == -1) {
			log_error("failed to open \"%s\": %s", cfg_file.c_str(),
												   strerror(errno));
			return WHALE_ERROR;
		}

		while (true){
			std::string line;

			getline(fd, line);

			if (line.empty())
				break;
			else if (line[0] == '#')
				continue;

			char * p = strtok((char *)line.data(), "=");
			std::string key(p);

			p = strtok(NULL, "=");
			std::string value(p);

			p = strtok(NULL, "=");
			
			if (p != NULL) {
				log_error("syntax error at line %d: at most one \'=\' could appear on each line.",
						 lineno);
				return WHALE_CONF_ERROR;
			}

			conf_map.insert(std::pair<std::string, std::string>(key, value));

			++lineno;
		}

		return WHALE_GOOD;
	}
}