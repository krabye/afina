#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

namespace Afina {
namespace Allocator {

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;
/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators

struct Block {
    size_t size;
    Block *next;
    Block *prev;
};

struct Info {
    size_t size;
};

class Simple {
public:
    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const;

private:
    void *_base;
    const size_t _base_len;
    Block *first_free_block;
    size_t bytes_free;
    char *ds;
    char *ds_begin;

    Block* merge(Block*, Block*);
};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
