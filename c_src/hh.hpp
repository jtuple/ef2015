// Simple non-concurrent/thread-safe hopscotch hash table
// See: http://people.csail.mit.edu/shanir/publications/disc2008_submission_98.pdf

#ifndef HH_H
#define HH_H

#include <cstdint>
#include <memory>
#include <iostream>
#include <cassert>
#include <utility>
#include <vector>

template <class Key, class Val>
struct HH {
  struct Entry {
    uint32_t info;
    bool valid;
    Key key;
    Val val;

    Entry()
      : info(0)
      , valid(false)
    {}
  };

  static const int width = sizeof(uint32_t) * 8;

  size_t capacity;
  std::unique_ptr<Entry[]> entries;

  HH(size_t capacity = 8)
    : capacity(capacity)
    , entries(new Entry[capacity]())
  {
  }

  bool read(Key key, Val &out) {
    auto hash = std::hash<Key>()(key);
    size_t pos = hash % capacity;

    for(int i = pos, end = pos + width; i != end; ++i) {
      if(entries[i].valid && (entries[i].key == key)) {
        out = entries[i].val;
        return true;
      }
    }
    return false;
  }

  template <class Fun>
  bool apply(Key key, Fun fun) {
    auto hash = std::hash<Key>()(key);
    size_t pos = hash % capacity;

    for(int i = pos, end = pos + width; i != end; ++i) {
      if(entries[i].valid && (entries[i].key == key)) {
        fun(entries[i].val);
        return true;
      }
    }
    return false;
  }

  bool erase(Key key) {
    auto hash = std::hash<Key>()(key);
    size_t pos = hash % capacity;

    for(int i = pos, end = pos + width; i != end; ++i) {
      if(entries[i].valid && (entries[i].key == key)) {
        entries[i].valid = false;
        return true;
      }
    }
    return false;
  }

  bool find_empty(size_t starting, size_t &out) {
    size_t probes = capacity;
    size_t idx = starting;
    while(probes--) {
      if(!entries[idx].valid) {
        // empty slot
        // std::cout << "empty slot: " << idx << "\n";
        out = idx;
        return true;
      }
      idx = (idx + 1) % capacity;
    }
    return false;
  }

  size_t distance(size_t a, size_t b) {
    if(a <= b) {
      return b - a;
    }
    else {
      return (capacity - a) + b;
    }
  }

  size_t header(size_t pos) {
    size_t offset = width - 1;
    if(pos < offset) {
      return capacity - offset;
    }
    return pos - offset;
  }

  void set_info(size_t bucket, size_t pos) {
    size_t offset = distance(bucket, pos);
    entries[bucket].info |= (1 << offset);
  }

  void clear_info(size_t bucket, size_t pos) {
    size_t offset = distance(bucket, pos);
    entries[bucket].info &= ~(1 << offset);
  }

  bool shift(size_t desired, size_t current) {
    if(distance(desired, current) < width) {
      // std::cout << "GOOD " << desired << " :: " << current << "\n";
      return true;
    }
    size_t hpos = header(current);
    Entry &h = entries[hpos];
    uint32_t info = h.info;
    bool found = false;
    int i;
    for(i = 0; i < width; ++i) {
      // std::cout << "INFO: " << info << "\n";
      if(info & 1) {
        found = true;
        break;
      }
      info >>= 1;
    }
    if(found) {
      size_t candidate = (hpos + i) % capacity;
      // std::cout << "SHIFT " << current << " to " << (hpos+i) << "\n";
      entries[current].valid = true;
      entries[current].key = entries[candidate].key;
      entries[current].val = entries[candidate].val;
      entries[candidate].valid = false;
      clear_info(hpos, (hpos + i) % capacity);
      set_info(hpos, current);
      // h.info &= ~(1 << i);
      // h.info |= (1 << (current - hpos));
      return shift(desired, candidate);
    }
    else {
      return false;
    }
  }

  bool insert(Key key, Val val) {
    auto hash = std::hash<Key>()(key);
    size_t pos = hash % capacity;

    // std::cout << "pos: " << key << " :: " << hash << " :: " << pos << "\n";
    // check for existing key entry
    for(size_t probes = width, idx = pos; probes; probes--) {
      if(entries[idx].valid && (entries[idx].key == key)) {
        // std::cout << "found existing: " << idx << "\n";
        entries[idx].val = val;
        return true;
      }
      idx = (idx + 1) % capacity;
    }

    size_t empty_pos;
    if(!find_empty(pos, empty_pos)) {
      // std::cout << "FULL\n";
      return false;
    }

    // if((empty_pos >= pos) && (empty_pos < (pos + width))) {
    if(distance(pos, empty_pos) < width) {
      // within our virtual bucket
      // std::cout << key << ": within our bucket!\n";
      // std::cout << "entries[" << empty_pos << "].key = " << key << "\n";
      entries[empty_pos].valid = true;
      entries[empty_pos].key = key;
      entries[empty_pos].val = val;
      set_info(pos, empty_pos);
      // entries[pos].info |= (1 << (empty_pos - pos));
      return true;
    }
    else {
      // std::cout << key << ": outside our bucket!\n";
      if(shift(pos, empty_pos)) {
        return insert(key, val);
      }
      else {
        // std::cout << "FULL2\n";
        return false;
      }
    }
  }

  void resize() {
    size_t new_capacity = capacity * 2;
    while(true) {
      HH tmp(new_capacity);
      bool success = true;

      for(int i = 0; i < capacity; ++i) {
        Entry &entry = entries[i];
        if(!entry.valid) continue;
        if(!tmp.insert(entry.key, entry.val)) {
          success = false;
          break;
        }
      }

      if(success) {
        std::swap(*this, tmp);
        return;
      }
      else {
        // std::cout << "FAILED\n";
        new_capacity *= 2;
      }
    }
  }

  void insert2(Key key, Val val) {
    if(!insert(key, val)) {
      // std::cout << "RESIZE\n";
      resize();
      insert2(key, val);
    }
  }

  std::vector<std::pair<Key, Val>> to_vector() {
    std::vector<std::pair<Key, Val>> v;
    for(int i = 0; i < capacity; ++i) {
      Entry &entry = entries[i];
      if(entry.valid) {
        v.push_back(std::make_pair(entry.key, entry.val));
      }
    }
    return v;
  }
};

#endif
