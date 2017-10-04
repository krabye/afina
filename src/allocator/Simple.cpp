#include <afina/allocator/Simple.h>
#include <afina/allocator/Error.h>
#include <afina/allocator/Pointer.h>
#include <string.h>

namespace Afina {
namespace Allocator {

Simple::Simple(void *base, size_t size) : _base(base), _base_len(size) {
	ds_begin = (char*)base + size - sizeof(char*);
	ds = ds_begin;
	//store descriptor pointer on itself
	*((char**)ds) = ds;
	bytes_free = size - sizeof(char*);

	first_free_block = (Block*)base;
	first_free_block->size = bytes_free;
	first_free_block->next = nullptr;
	first_free_block->prev = nullptr;
}

/**
 * TODO: semantics
 * @param N size_t
 */
Pointer Simple::alloc(size_t N) { 
	// std::cout << "OK\n";
	if(first_free_block == nullptr)
		throw AllocError(AllocErrorType::NoMemory, "Not enough memory");

	//Get or create free pointer
	char *prev = ds;
	char *next = *((char**)ds);
	while(next != *((char**)next)) {
		prev = next;
		next = *((char**)next);
	}
	if(prev == ds) {
		Block* next_block = first_free_block;
		while(next_block->next != nullptr)
			next_block = next_block->next;
		if((char*)next_block + next_block->size == ds_begin) {
			next_block->size -= sizeof(char*);
			next = (char*)((char*)next_block + next_block->size);
			ds_begin = next;
		} else {
			throw AllocError(AllocErrorType::NoMemory, "Not enough memory for descriptor");
		}
	}

	//in @next we have last free poointer, in @prev is previous to it
	*((char**)prev) = prev;
	char *inner_ptr = next;
	Pointer ptr;
	ptr.inner_ptr = (void*)inner_ptr;
	Block *block = first_free_block;

	while(block != nullptr) {
		if(N + sizeof(Block) <= block->size) {
			if(N < block->size - 2*sizeof(Block)) {
				*((char**)inner_ptr) = (char*)block + sizeof(Block);
				// printf("%lu %lu\n", ptr.get(), reinterpret_cast<char*>(ptr.get()));

				Block *new_block = (Block*)((char*)block + sizeof(Block) + N);
				new_block->size = block->size - N - sizeof(Block);
				new_block->next = block->next;
				new_block->prev = block->prev;

				if(block->prev != nullptr)
					block->prev->next = new_block;
				if(block->next != nullptr)
					block->next->prev = new_block;

				if(block == first_free_block)
					first_free_block = new_block;

				block->size = N + sizeof(Block);
				block->next = nullptr;
				block->prev = nullptr;

				// printf("END\n");

				return ptr;
			}
			//Case when we can't make new Block from rest of Block
			//Then we allocate all block
			else {
				*((char**)inner_ptr) = (char*)block + sizeof(Block);
				if(block->prev != nullptr)
					block->prev->next = block->next;
				if(block->next != nullptr)
					block->next->prev = block->prev;
				if(block == first_free_block) {
					if(block->next != nullptr) {
						first_free_block = block->next;	
					} else {
						first_free_block = nullptr;
					}
				}

				block->size = block->size;
				block->next = nullptr;
				block->prev = nullptr;

				return ptr;
			}
		}

		block = block->next;
	}

	//put back ds
	*((char**)prev) = next;
	*((char**)next) = next;
	throw AllocError(AllocErrorType::NoMemory, "Not enough memory");
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &p, size_t N) {
	if(p.inner_ptr == nullptr) {
		p = alloc(N);
		return;
	}
	char *ptr = (char*)p.get();
	size_t old_N = ((Block*)(ptr - sizeof(Block)))->size - sizeof(Block);
	free(p);
	p = alloc(N);
	memmove(p.get(), ptr, N > old_N ? old_N : N);
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &p) {
	if(p.inner_ptr == nullptr)
		return;
	char *inner_ptr = (char*)p.inner_ptr;
	char *ptr = *((char**)inner_ptr) - sizeof(Block);
	size_t size = ((Block*)ptr)->size;
	Block *free_block = (Block*)ptr;
	free_block->size = size;
	free_block->next = nullptr;
	free_block->prev = nullptr;
	p.inner_ptr = nullptr;

	//put back ds
	char *prev = ds;
	char *next = *((char**)ds);
	while(next != *((char**)next)) {
		prev = next;
		next = *((char**)next);
	}
	*((char**)next) = inner_ptr;
	*((char**)inner_ptr) = inner_ptr;

	//put back free block

	if(first_free_block == nullptr) {
		first_free_block = free_block;
		return;
	}

	Block *next_block = first_free_block;
	while((char*)next_block < ptr && next_block->next != nullptr)
		next_block = next_block->next;

	if((char*)next_block < (char*)free_block){
		next_block->next = free_block;
		free_block->prev = next_block;
		if((char*)next_block + next_block->size == (char*)free_block)
			merge(next_block, free_block);
	} else {
		if(next_block->prev != nullptr) {
			next_block->prev->next = free_block;
			free_block->prev = next_block->prev;
			next_block->prev = free_block;
			free_block->next = next_block;
			if((char*)free_block->prev + free_block->prev->size == (char*)free_block)
				free_block = merge(free_block->prev, free_block);
			if((char*)free_block + free_block->size == (char*)free_block->next)
				free_block = merge(free_block, free_block->next);
		} else {
			free_block->next = next_block;
			next_block->prev = free_block;
			if((char*)free_block + free_block->size == (char*)free_block->next)
				free_block = merge(free_block, free_block->next);
			if(first_free_block == next_block)
				first_free_block = free_block;
		}
	}
}

Block* Simple::merge(Block *left, Block *right) {
	left->size = left->size + right->size;
	left->next = right->next;
	if (right->next != nullptr)
		right->next->prev = left;
	return left;
}

/**
 * TODO: semantics
 */
void Simple::defrag() {
	Block* block;
	char *p, *to;
	char *descr;

    for(to = p = (char*)_base; p < ds_begin;) {
        block = (Block*)p;
        size_t size = block->size;
        if(block->next == nullptr && block->prev == nullptr && block != first_free_block) {
            memmove(to, p, size);
            descr = ds_begin;
            while(*((char**)descr) != p + sizeof(Block))
            	descr += sizeof(char*);
            *((char**)descr) = to + sizeof(Block);
            
            to += size;
        }
        p += size;
    }

    block = (Block*)to;
    block->size = ds_begin - to;
    block->next = nullptr;
    block->prev = nullptr;
    first_free_block = block;
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

} // namespace Allocator
} // namespace Afina
