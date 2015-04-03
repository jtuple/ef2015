#ifndef RWLOCK_H
#define RWLOCK_H

#include <atomic>
#include <thread>
#include <array>

///===================================================================
/// Simple reader-biased reader/writer spinlock
///===================================================================

struct rwlock {
  const int WRITER = 1;
  const int READER = 2;
  mutable std::atomic<unsigned> val;

  rwlock() : val(0) {}

  bool try_rlock() {
    unsigned v = val.fetch_add(READER, std::memory_order_acquire);
    if(v & WRITER) {
      val.fetch_sub(READER, std::memory_order_release);
      return false;
    }
    return true;
  }

  void rlock() {
    while(!try_rlock())
      continue;
  }

  void runlock() {
    val.fetch_sub(READER, std::memory_order_release);
  }

  void wlock() {
    unsigned expect = 0;
    while(!val.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel)) {
      expect = 0;
    }
  }

  void wunlock() {
    val.fetch_sub(WRITER, std::memory_order_release);
  }

  ///===================================================================
  struct rlockable {
    rwlock &l;
    rlockable(rwlock &l) : l(l) {}
    void lock() {
      while(!l.try_rlock()) continue;
    }
    void unlock() {
      l.runlock();
    }
  };
  rlockable get_rlock() { return rlockable(*this); }
  ///===================================================================
  struct wlockable {
    rwlock &l;
    wlockable(rwlock &l) : l(l) {}
    void lock() {
      l.wlock();
    }
    void unlock() {
      l.wunlock();
    }
  };
  wlockable get_wlock() { return wlockable(*this); }
};

///===================================================================
/// Partitioned rwlock (fast for readers, slow for writers)
///===================================================================

struct partitioned_rwlock {
  static const int num = 8;
  std::array<rwlock, num> locks;

  static int get_index() {
    auto thread_id = std::this_thread::get_id();
    return std::hash<decltype(thread_id)>()(thread_id) % num;
  }

  void rlock() {
    auto idx = get_index();
    locks[idx].rlock();
  }

  void runlock() {
    auto idx = get_index();
    locks[idx].runlock();
  }

  void wlock() {
    for(auto it = locks.begin(), end = locks.end(); it != end; ++it) {
      it->wlock();
    }
  }

  void wunlock() {
    for(auto it = locks.rbegin(), end = locks.rend(); it != end; ++it) {
      it->wunlock();
    }
  }
};

#endif
