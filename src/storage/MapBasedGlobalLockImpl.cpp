#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
	// std::unique_lock<std::mutex> guard(_lock);
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

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
	// std::unique_lock<std::mutex> guard(_lock);
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

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
	// std::unique_lock<std::mutex> guard(_lock);
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

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) { 
	// std::unique_lock<std::mutex> guard(_lock);
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

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
	// std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));
	if(_backend.find(key) != _backend.end()) {
		value = _backend.at(key);
		return true;
	}
	return false;
}

} // namespace Backend
} // namespace Afina
