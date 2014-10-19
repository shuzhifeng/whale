/*
* Copyright (C) Xinjing Cho
*/
#include <string>

#include <sys/mman.h>

#include <define.h>

namespace whale {

#define MMAP_FILE_MODE		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
#define MMAP_FILE_FLAGS		O_CREAT | O_RDWR

class file_mmap {
public:
	file_mmap(std::string map_file_, size_t size_,
			  w_int_t prot_, w_int_t flags_,
			  off_t off_ = 0, void * hint_ = nullptr)
		:map_file(map_file_), size(size_), prot(prot_), flags(flags_),
		 off(off_), hint(hint_), addr(nullptr), fd(-1) {}

	/*
	* open @map_file and mmap it.
	* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
	*/
	w_rc_t map();

	/*
	* unmmap @map_file and close @map_file .
	* Return: WHALE_GOOD on success, WHALE_ERROR on failure.
	*/
	w_rc_t unmap();

	void * get_addr() { return addr; }
	
private:
	std::string	map_file;
	size_t	 	size;
	w_int_t		prot;
	w_int_t		flags;
	off_t		off;
	void		*hint;
	void 		*addr;
	w_int_t		fd;
};

}