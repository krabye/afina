#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {

Pointer::Pointer() : inner_ptr(nullptr) {}
Pointer::Pointer(const Pointer &p) {
	inner_ptr = p.inner_ptr;
}
Pointer::Pointer(Pointer &&p) {
	inner_ptr = p.inner_ptr;
	p.inner_ptr = nullptr;
}

Pointer &Pointer::operator=(const Pointer &p) {
	inner_ptr = p.inner_ptr;
	return *this;
}
Pointer &Pointer::operator=(Pointer &&p) {
	inner_ptr = p.inner_ptr;
	p.inner_ptr = nullptr;
	return *this;
}

void* Pointer::get() const { 
	return inner_ptr == nullptr ? nullptr : *((void**)inner_ptr);
}

} // namespace Allocator
} // namespace Afina
