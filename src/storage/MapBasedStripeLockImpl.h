#ifndef AFINA_STORAGE_MAP_BASED_STRIPELOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_STRIPELOCK_IMPL_H

#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <deque>
#include <functional>

#include <afina/Storage.h>
#include "MapBasedSimpleImpl.h"

namespace Afina {
namespace Backend {

class MapBasedStripeLockImpl : public Afina::Storage {
public:
	MapBasedStripeLockImpl(size_t n_buckets, size_t max_size = 1024);
	~MapBasedStripeLockImpl();

	// Implements Afina::Storage interface
	bool Put(const std::string &key, const std::string &value) override;

	// Implements Afina::Storage interface
	bool PutIfAbsent(const std::string &key, const std::string &value) override;

	// Implements Afina::Storage interface
	bool Set(const std::string &key, const std::string &value) override;

	// Implements Afina::Storage interface
	bool Delete(const std::string &key) override;

	// Implements Afina::Storage interface
	bool Get(const std::string &key, std::string &value) const override;

private:
	std::mutex *locks;
	std::vector<MapBasedSimpleImpl> buckets;

	size_t _max_size;

	size_t time = 0;

	size_t count = 0;

	size_t _n_buckets;

	std::map<std::string, std::string> _backend;

	std::map<size_t, std::string> _timestamps;
	std::map<std::string, size_t> _inv_timestamps;

	std::hash<std::string> hash_fn;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_STRIPELOCK_IMPL_H
