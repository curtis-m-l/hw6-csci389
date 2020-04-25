#include <unordered_map>
#include <iostream>
#include <cassert>

#include "cache.hh"
#include "fifo_evictor.hh"

/*


*/

class Cache::Impl {
public:
  //Our data members:
  size_type m_current_mem;
  std::unordered_map<key_type, std::string, hash_func> m_cache_vals;
  std::unordered_map<key_type, size_type, hash_func> m_cache_sizes;
  std::string get_val_;

  // Initial data members/functions:
  size_type m_maxmem;
  Evictor* m_evictor = nullptr;

  Impl() {
    m_current_mem = 0;
  }

  void set(key_type key, val_type val, size_type size) {
    // If data is larger than cache capacity
    if (size > m_maxmem) {
      std::cout << "It don't fit.\n"; 
      return;
    }
    // If value we're emplacing already exists, calculate the size change
    auto existing_value = m_cache_vals.find(key);
    size_type actual_size = size;
    if (existing_value != m_cache_vals.end()) {
      actual_size = size - m_cache_sizes.find(key)->second;
    }
    assert (val != NULL && "String was null :/ \n");

    std::string val_string(val);
    // If it fits, add it to the cache 
    if (m_current_mem + actual_size <= m_maxmem) {

        if (existing_value == m_cache_vals.end()) {
            m_cache_vals.emplace(key, val_string);
            m_cache_sizes.emplace(key, size);
        }
        else {
            //https://stackoverflow.com/questions/16291897/in-unordered-map-of-c11-how-to-update-the-value-of-a-particular-key
            existing_value->second = val_string;
            auto existing_size = m_cache_sizes.find(key);
            existing_size->second = size;
        }
        m_current_mem += actual_size;
        // Let the eviction policy know about the new item
        if (m_evictor != nullptr) {
            m_evictor->touch_key(key);
        }
    }
    else {
      // If we have no eviction policy, reject it
      if (m_evictor == nullptr) {
      }
      // Otherwise, evict stuff until it fits
      else {
        while (m_current_mem + actual_size > m_maxmem) {
          key_type evictedKey = m_evictor->evict();
          m_cache_vals.erase(evictedKey);
          if (m_cache_sizes.find(evictedKey) != m_cache_sizes.end()) {
            size_type sizeOfEvicted = m_cache_sizes.find(evictedKey)->second;
            m_current_mem -= sizeOfEvicted;
          }
          m_cache_sizes.erase(evictedKey);
        }
        // This is identical to code in an above if statement, since we've now guaranteed
        // that the data can fit in our cache. Restructuring of this code could yield
        // more optimal performance, but this should still be correct.
        if (existing_value == m_cache_vals.end()) {
            m_cache_vals.emplace(key, val_string);
            m_cache_sizes.emplace(key, size);
        }
        else {
            //https://stackoverflow.com/questions/16291897/in-unordered-map-of-c11-how-to-update-the-value-of-a-particular-key
            existing_value->second = val_string;
            auto existing_size = m_cache_sizes.find(key);
            existing_size->second = size;
        }
        m_current_mem += actual_size;
        m_evictor->touch_key(key); 
      }
    }
  }

  val_type get(key_type key, size_type& val_size) {
    auto toRe = m_cache_vals.find(key);
    if (toRe == m_cache_vals.end()) {
        return nullptr;
    }
    if (m_evictor != nullptr) {
        m_evictor->touch_key(key);
    }
    get_val_ = toRe->second;
    val_size = m_cache_sizes.find(key)->second;
    return static_cast<val_type>(get_val_.c_str());
  }

  bool del(key_type key) {
    auto entry = m_cache_vals.find(key);
    if (entry == m_cache_vals.end()) {
      return false;
    }
    else {
      m_current_mem -= m_cache_sizes[key];
      m_cache_vals.erase(key);
      m_cache_sizes.erase(key);
      return true;
    }
  }

  size_type space_used() {
    return m_current_mem;
  }

  void reset() {
    m_current_mem = 0;
    m_cache_vals.clear();
    m_cache_sizes.clear();
  }
};

Cache::Cache(size_type maxmem,
    float max_load_factor,
    Evictor* evictor,
    hash_func hasher) :
    pImpl_(new Impl())
{
	pImpl_->m_maxmem = maxmem;
	pImpl_->m_evictor = evictor;
	pImpl_->m_current_mem = 0;
	std::unordered_map<key_type, std::string, hash_func> cache_vals(0, hasher);
    std::unordered_map<key_type, size_type, hash_func> cache_sizes(0, hasher);
    cache_vals.max_load_factor(max_load_factor);
    cache_sizes.max_load_factor(max_load_factor);
    pImpl_->m_cache_vals = cache_vals;
    pImpl_->m_cache_sizes = cache_sizes;
}

void Cache::set(key_type key, val_type val, size_type size) { pImpl_->set(key, val, size); }
Cache::val_type Cache::get(key_type key, size_type& val_size) const { return pImpl_->get(key, val_size); }
bool Cache::del(key_type key) { return pImpl_->del(key); }
Cache::size_type Cache::space_used() const { return pImpl_->space_used(); }
void Cache::reset() { pImpl_->reset(); }
Cache::~Cache() { pImpl_.reset(); }
