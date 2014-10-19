/*
* Copyright (C) Xinjing Cho
*/
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <file_mmap.h>
#include <log.h>

namespace whale {
	w_rc_t file_mmap::map() {
		fd = ::open(map_file.c_str(), MMAP_FILE_FLAGS, MMAP_FILE_MODE);

		if (fd == -1) {
			log_error("failed to open \"%s\" : %s", map_file.c_str(), 
													strerror(errno));
			return WHALE_ERROR;
		}

		addr = ::mmap(hint, size, prot, flags, fd, off);

		if (addr == MAP_FAILED) {
			log_error("failed to mmap \"%s\" : %s", map_file.c_str(), 
													strerror(errno));
			return WHALE_ERROR;
		}

		return WHALE_GOOD;
	}

	w_rc_t file_mmap::unmap(){
		if (::munmap(addr, size) == -1) {
			log_error("failed to unmmap \"%s\": %s", map_file.c_str(),
											   		 strerror(errno));
			return WHALE_ERROR;
		}

		::close(fd);

		return WHALE_GOOD;
	}
}