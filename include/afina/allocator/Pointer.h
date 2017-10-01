#ifndef AFINA_ALLOCATOR_POINTER_H
#define AFINA_ALLOCATOR_POINTER_H

namespace Afina {
namespace Allocator {
// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Simple;

class Pointer {
private:
	void *inner_ptr;
public:

	friend Simple;
    Pointer();

    Pointer(const Pointer &);
    Pointer(Pointer &&);

    Pointer &operator=(const Pointer &);
    Pointer &operator=(Pointer &&);

    void *get() const;
};

} // namespace Allocator
} // namespace Afina

#endif // AFINA_ALLOCATOR_POINTER_H
