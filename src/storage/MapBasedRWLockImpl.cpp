#include "MapBasedRWLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

MapBasedRWLockImpl::MapBasedRWLockImpl(size_t max_size) : _max_size(max_size) {
	if (pthread_rwlock_init(&rwlock, NULL)) {
		throw std::runtime_error("RWLock init failed\n");
	}
}

MapBasedRWLockImpl::~MapBasedRWLockImpl() {
	if (pthread_rwlock_destroy(&rwlock)) {
		throw std::runtime_error("RWLock destroy failed\n");
	}
}

// See MapBasedRWLockImpl.h
bool MapBasedRWLockImpl::Put(const std::string &key, const std::string &value) {
	WRLockGuard lock(rwlock);
	if(_backend.find(key) == _backend.end()) {
		if(count > _max_size - 1) {
			std::string key_tmp = _timestamps.rbegin()->second;
			_timestamps.erase(_timestamps.rbegin()->first);
			_inv_timestamps.erase(key_tmp);
			_backend.erase(key_tmp);
			count--;
		}
		if((_backend.emplace(std::make_pair(key, value))).second) {
			_timestamps.emplace(std::make_pair(time, key));
			_inv_timestamps.emplace(std::make_pair(key, time));
			time++;
			count++;
			return true;
		}
	} else {
		_backend.at(key) = value;
		size_t stamp = _inv_timestamps.at(key);
		_inv_timestamps.at(key) = time;
		_timestamps.erase(stamp);
		_timestamps.emplace(std::make_pair(time, value));
		time++;
		return true;
	}
	return false;
}

// See MapBasedRWLockImpl.h
bool MapBasedRWLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
	WRLockGuard lock(rwlock);
	if(_backend.find(key) == _backend.end()) {
		if(count > _max_size - 1) {
			std::string key_tmp = _timestamps.rbegin()->second;
			_timestamps.erase(_timestamps.rbegin()->first);
			_inv_timestamps.erase(key_tmp);
			_backend.erase(key_tmp);
			count--;
		}
		if((_backend.emplace(std::make_pair(key, value))).second) {
			_timestamps.emplace(std::make_pair(time, key));
			_inv_timestamps.emplace(std::make_pair(key, time));
			time++;
			count++;
			return true;
		}
	}
	return false;
}

// See MapBasedRWLockImpl.h
bool MapBasedRWLockImpl::Set(const std::string &key, const std::string &value) {
	WRLockGuard lock(rwlock);
	if(_backend.find(key) != _backend.end()) {
		_backend.at(key) = value;
		size_t stamp = _inv_timestamps.at(key);
		_inv_timestamps.at(key) = time;
		_timestamps.erase(stamp);
		_timestamps.emplace(std::make_pair(time, value));
		time++;
		return true;
	}
	return false;
}

// See MapBasedRWLockImpl.h
bool MapBasedRWLockImpl::Delete(const std::string &key) { 
	WRLockGuard lock(rwlock);
	auto it = _backend.find(key);
	if(it != _backend.end()) {
		_backend.erase(it);
		size_t stamp = _inv_timestamps.at(key);
		_inv_timestamps.erase(key);
		_timestamps.erase(stamp);
		count--;
		return true;
	}
	return false;
}

// See MapBasedRWLockImpl.h
bool MapBasedRWLockImpl::Get(const std::string &key, std::string &value) const {
	RDLockGuard lock(rwlock);
	if(_backend.find(key) != _backend.end()) {
		value = _backend.at(key);
		return true;
	}
	return false;
}

} // namespace Backend
} // namespace Afina
