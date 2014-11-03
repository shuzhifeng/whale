/*
* Copyright (C) Xinjing Cho
*/
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <log.h>
#include <whale_log.h>

namespace whale {

	w_rc_t logger::init() {
		fd = ::open(log_file.c_str(), ENTRY_LOG_FILE_FLAGS, ENTRY_LOG_FILE_MODE);

		if (fd == -1) {
			log_error("failed to open log file \"%s\"", log_file.c_str());
			return WHALE_ERROR;
		}

		std::unique_ptr<char[]> p;
		size_t                  bufsize = 0;
		size_t                  len;
		size_t                  n;
		w_int_t                 nread;

		while(true) {
			n = 0;
			while(n < sizeof(uint32_t)) {
				nread = ::read(fd, (char *)&len + n, 
					           sizeof(uint32_t) - n);

				if (nread <= 0) {
					if (nread == 0)
						break;
					else if (nread == -1 && errno == EINTR) 
						continue;

					log_error("failed to read log file: %s", 
						                    strerror(errno));
					::abort();
				}
				n += nread;
			}

			if (nread == 0)break;

			if (len > bufsize) {
				p.reset(new char[len]);
				bufsize = len;
			}

			while(n < len) {
				nread = ::read(fd, p.get() + n, len - n);
				
				if (nread <= 0) {
					if (nread == 0)
						break;
					else if (nread == -1 && errno == EINTR) 
						continue;

					log_error("failed to read log file: %s", 
						                    strerror(errno));
					::abort();
				}
				n += nread;
			}

			if (nread == 0)break;

			entries.push_back({0, 0, ""});

			entries.back().index = *(int32_t*)p.get();
			entries.back().term = *((int32_t*)p.get() + 1);
			entries.back().data.append(p.get() + 2 * sizeof(int32_t), 
				                        bufsize - 2 * sizeof(int32_t));
		}
		
		this->pos = ::lseek(fd, 0, SEEK_CUR);
		this->commit_idx = this->entries.end() - this->entries.begin();
		return WHALE_GOOD;
	}

	log_entry_it logger::find_by_idx(int32_t idx) {
		log_entry_t v{idx, 0, ""};

		auto it = std::lower_bound(entries.begin(), entries.end(), v,
			                       [](const log_entry_t & lhs,
			                          const log_entry_t & rhs)->bool {
			                          return lhs.index < rhs.index;
			                       });

		if (it == entries.end() || it->index != idx)
			return entries.end();
		else
			return it;
	}


	void logger::write(char * buf, size_t len) {
		size_t  n = 0;
		w_int_t nwrite = 0;

		while (true) {
			nwrite = ::write(this->fd, buf + n, len - n);
				
			if (nwrite <= 0) {
				if (nwrite == 0)
					break;
				else if (nwrite == -1 && errno == EINTR) 
					continue;

				log_error("failed to write log file: %s", 
					                    strerror(errno));
				::abort();
			}
			n += nwrite;
		}
	}
	
	/*
	* commit all entries whose index is less or equal to @end
	*/
	void logger::commit_until(w_int_t end) {
		std::unique_ptr<char[]> p;
		size_t                  bufsize = 0;

		for (;this->commit_idx < end && 
			  this->entries.begin() + this->commit_idx < this->entries.end();
			  ++this->commit_idx) {
			log_entry_t & e = this->entries[this->commit_idx];
			
			if (LOG_ENTRY_LEN(e) > bufsize) {
				bufsize = LOG_ENTRY_LEN(e);
				p.reset(new char[bufsize]);
			}

			*(uint32_t*)p.get() = LOG_ENTRY_LEN(e);
			*((int32_t *)((uint32_t*)p.get() + 1)) = e.index;
			*((int32_t *)((uint32_t*)p.get() + 1) + 1) = e.term;
			memcpy(((int32_t *)((uint32_t*)p.get() + 1) + 1),
				  e.data.c_str(),
				  e.data.size());

			this->write(p.get(), LOG_ENTRY_LEN(e));
		}

		if (p.get()) {
			this->pos = ::lseek(this->fd, 0, SEEK_CUR);
		}
	}

	/*
	* erase entries starting at @start and those follow it.
	*/
	void logger::chop(log_entry_it start) {
		off_t dec = 0;
		w_int_t idx = start - this->entries.begin();

		if (idx < commit_idx) {
			log_entry_it it;
			for(it = start; 
				it != this->entries.end() &&
				it - this->entries.begin() <= commit_idx;
				++it) {
				dec += LOG_ENTRY_LEN(*it);
			}

			this->commit_idx = idx;
			this->pos -= dec;

			::ftruncate(fd, this->pos);
			::lseek(fd, this->pos, SEEK_SET);
		}

		this->entries.erase(start, this->entries.end());
	}

	/*
	* append entries in @new_entries.
	*/
	void logger::append(std::vector<log_entry_t> new_entries) {
		this->entries.insert(this->entries.begin(), 
			                 new_entries.begin(), new_entries.end());
	}
}