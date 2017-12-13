#include "MapBasedStripeLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

MapBasedStripeLockImpl::MapBasedStripeLockImpl(size_t n_buckets,size_t max_size)
 : _n_buckets(n_buckets), _max_size(max_size) {
 	locks = new std::mutex[_n_buckets];
	for (int i = 0; i < _n_buckets; ++i) {
		buckets.emplace_back(_max_size / n_buckets);
	}
}

MapBasedStripeLockImpl::~MapBasedStripeLockImpl() {
	delete[] locks;
}

// See MapBasedStripeLockImpl.h
bool MapBasedStripeLockImpl::Put(const std::string &key, const std::string &value) {
	size_t ind = hash_fn(key) % _n_buckets;
	std::unique_lock<std::mutex> guard(locks[ind]);

	return buckets[ind].Put(key, value);
}

// See MapBasedStripeLockImpl.h
bool MapBasedStripeLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
	size_t ind = hash_fn(key) % _n_buckets;
	std::unique_lock<std::mutex> guard(locks[ind]);

	return buckets[ind].PutIfAbsent(key, value);
}

// See MapBasedStripeLockImpl.h
bool MapBasedStripeLockImpl::Set(const std::string &key, const std::string &value) {
	size_t ind = hash_fn(key) % _n_buckets;
	std::unique_lock<std::mutex> guard(locks[ind]);

	return buckets[ind].Set(key, value);
}

// See MapBasedStripeLockImpl.h
bool MapBasedStripeLockImpl::Delete(const std::string &key) { 
	size_t ind = hash_fn(key) % _n_buckets;
	std::unique_lock<std::mutex> guard(locks[ind]);

	return buckets[ind].Delete(key);
}

// See MapBasedStripeLockImpl.h
bool MapBasedStripeLockImpl::Get(const std::string &key, std::string &value) const {
	size_t ind = hash_fn(key) % _n_buckets;
	std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&locks[ind]));

	return buckets[ind].Get(key, value);
}

} // namespace Backend
} // namespace Afina
