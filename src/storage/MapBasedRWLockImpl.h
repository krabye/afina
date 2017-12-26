#ifndef AFINA_STORAGE_MAP_BASED_RWLOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_RWLOCK_IMPL_H

#include <map>
#include <string>
#include <deque>
#include <pthread.h>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

class RDLockGuard {
public:
	RDLockGuard(pthread_rwlock_t &lock) : _rwlock(lock) {
		if (pthread_rwlock_rdlock(&_rwlock)) {
			throw std::runtime_error("RWLock rdlock failed\n");
		}
	}
	~RDLockGuard() {
		if (pthread_rwlock_unlock(&_rwlock)) {
			throw std::runtime_error("RWLock unlock (rdlock) failed\n");
		}
	}
private:
	pthread_rwlock_t &_rwlock;
};

class WRLockGuard {
public:
	WRLockGuard(pthread_rwlock_t &lock) : _rwlock(lock) {
		if (pthread_rwlock_wrlock(&_rwlock)) {
			throw std::runtime_error("RWLock rdlock failed\n");
		}
	}
	~WRLockGuard() {
		if (pthread_rwlock_unlock(&_rwlock)) {
			throw std::runtime_error("RWLock unlock (wrlock) failed\n");
		}
	}
private:
	pthread_rwlock_t &_rwlock;
};

class MapBasedRWLockImpl : public Afina::Storage {
public:
	MapBasedRWLockImpl(size_t max_size = 1024);
	~MapBasedRWLockImpl();

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
	mutable pthread_rwlock_t rwlock;

	size_t _max_size;

	size_t time = 0;

	size_t count = 0;

	std::map<std::string, std::string> _backend;

	std::map<size_t, std::string> _timestamps;
	std::map<std::string, size_t> _inv_timestamps;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_RWLOCK_IMPL_H
