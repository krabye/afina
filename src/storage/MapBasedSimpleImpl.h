#ifndef AFINA_STORAGE_MAP_BASED_SIMPLE_IMPL_H
#define AFINA_STORAGE_MAP_BASED_SIMPLE_IMPL_H

#include <map>
#include <string>
#include <deque>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */
class MapBasedSimpleImpl : public Afina::Storage {
public:
    MapBasedSimpleImpl(size_t max_size = 1024) : _max_size(max_size) {}
    ~MapBasedSimpleImpl() {}

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
    size_t _max_size;

    size_t time = 0;

    size_t count = 0;

    std::map<std::string, std::string> _backend;

    std::map<size_t, std::string> _timestamps;
    std::map<std::string, size_t> _inv_timestamps;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_SIMPLE_IMPL_H
