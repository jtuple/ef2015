// Wraps non-concurrent hopscotch hash table (hh.hpp) with simple
// partitioned reader/writer lock to make thread-safe.

#include "hh.hpp"
#include "rwlock.hpp"

template <class Key, class Val>
struct HH2 {
  HH<Key, Val> hh;
  partitioned_rwlock lock;

  bool read(Key key, Val &out) {
    lock.rlock();
    bool result = hh.read(key, out);
    lock.runlock();
    return result;
  }

  template <class Fun>
  bool apply(Key key, Fun fun) {
    lock.rlock();
    bool result = hh.apply(key, fun);
    lock.runlock();
    return result;
  }

  void insert(Key key, Val val) {
    lock.wlock();
    hh.insert2(key, val);
    lock.wunlock();
  }
};
