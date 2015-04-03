extern "C" {
#include "erl_nif.h"
}

#include "rwlock.hpp"
#include "hh2.hpp"

#include <atomic>
#include <memory>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <limits>
#include <unistd.h>
#include <deque>

// reservoir size
#define SIZE 10000

extern "C" {
int erts_printf(const char *, ...);
}

///===================================================================
/// Utility
///===================================================================

template<class TA, class T>
void atomic_max(TA &atomic, T val) {
  T current = atomic.load(std::memory_order_relaxed);
  while(val > current) {
    current = val;
    val = atomic.exchange(val);
  }
}

template<class TA, class T>
void atomic_min(TA &atomic, T val) {
  T current = atomic.load(std::memory_order_relaxed);
  while(val < current) {
    current = val;
    val = atomic.exchange(val);
  }
}

///===================================================================
/// Fast metric implementation
///===================================================================

struct samples_t {
  struct data_t {
    mutable std::atomic<unsigned> pos;
    int res[SIZE];
    rwlock lock;
    mutable std::atomic<unsigned> min;
    mutable std::atomic<unsigned> max;

    data_t() {
      reset();
    }

    void reset() {
      pos.store(0);
      min.store(std::numeric_limits<unsigned>::max());
      max.store(std::numeric_limits<unsigned>::min());
    }
  };

  struct report {
    int64_t timestamp;
    int64_t events;
    int64_t min;
    int64_t max;
    int64_t p50;
    int64_t p90;
    int64_t p95;
    int64_t p99;
    int64_t p999;
  };

  std::unique_ptr<data_t[]> storage;
  mutable std::atomic<data_t*> data;
  data_t *next_data;

  std::unique_ptr<report[]> reports;
  mutable std::atomic<report*> current_report;
  report *next_report;
  rwlock report_lock;

  std::mt19937 rnd;

  samples_t()
    : storage(new data_t[2])
    , data(storage.get())
    , next_data(data + 1)
    , reports(new report[2])
    , current_report(reports.get())
    , next_report(current_report + 1)
    , rnd(rand())
  {}

  void add(unsigned val) {
    data_t *d = data.load(std::memory_order_relaxed);
    while(!d->lock.try_rlock())
      d = data.load(std::memory_order_relaxed);

    atomic_max(d->max, val);
    atomic_min(d->min, val);
    unsigned p = std::atomic_fetch_add_explicit(&d->pos, 1u, std::memory_order_relaxed);
    if(p < SIZE) {
      d->res[p] = val;
    }
    else {
      // int j = rand() % p;
      int j = rnd() % p;
      if(j < SIZE) {
        // std::cout << "Replacing res[" << j << "]\n";
        d->res[j] = val;
      }
    }
    d->lock.runlock();
  }

  report read() {
    while(!report_lock.try_rlock())
      ;

    report r(*current_report.load());
    report_lock.runlock();
    return r;
  }

  void collect() {
    data_t *d = data.exchange(next_data);
    d->lock.wlock();

    int64_t events = d->pos.load(std::memory_order_relaxed);
    unsigned size = std::min(events, (int64_t)SIZE);
    if(events == 0) {
      next_report->events = 0;
      next_report->min  = 0;
      next_report->max  = 0;
      next_report->p50  = 0;
      next_report->p90  = 0;
      next_report->p95  = 0;
      next_report->p99  = 0;
      next_report->p999 = 0;
    }
    else {
      std::sort(d->res, d->res + size);
      next_report->events = events;
      next_report->min  = d->min;
      next_report->max  = d->max;
      next_report->p50  = d->res[size*50/100];
      next_report->p90  = d->res[size*90/100];
      next_report->p95  = d->res[size*95/100];
      next_report->p99  = d->res[size*99/100];
      next_report->p999 = d->res[size*999/1000];
    }

    report_lock.wlock();
    report *nr = current_report.exchange(next_report);
    report_lock.wunlock();
    next_report = nr;

    d->lock.wunlock();
    d->reset();
    next_data = d;
  }
};

///===================================================================
/// Actual NIF code
///===================================================================

std::atomic<bool> running;
std::thread collector_thread;
ErlNifTid collector_ethread;

std::deque<samples_t> metrics;

static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_METRIC;

std::mutex mtx_metrics;
std::deque<samples_t> d_metrics;
HH2<ERL_NIF_TERM, std::vector<samples_t*>> app_metrics;

static ERL_NIF_TERM setup_metrics(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int num;

  if(argc == 1) {
    // global metrics
    if(!enif_get_int(env, argv[0], &num)) {
      return ATOM_ERROR;
    }
    if(metrics.size() < num) {
      metrics.resize(num);
    }
    return ATOM_OK;
  }
  else {
    // namespaced metrics
    ERL_NIF_TERM app = argv[0];

    if(!enif_is_atom(env, app) ||
       !enif_get_int(env, argv[1], &num)) {
      return ATOM_ERROR;
    }

    std::lock_guard<std::mutex> lock(mtx_metrics);
    std::vector<samples_t*> mm;
    app_metrics.read(app, mm);
    for(int i = mm.size(); i < num; ++i) {
      d_metrics.emplace_back();
      mm.push_back(&d_metrics.back());
    }
    app_metrics.insert(app, mm);
    return ATOM_OK;
  }
}

bool get_metric(ErlNifEnv *env,
               int argc,
               const ERL_NIF_TERM argv[],
               samples_t *&metric) {
  int metric_id;

  if(argc == 1) {
    if(!enif_get_int(env, argv[0], &metric_id) ||
       metrics.size() <= metric_id) {
      return false;
    }
    metric = &metrics[metric_id];
    return true;
  }
  else if (argc == 2) {
    ERL_NIF_TERM app = argv[0];

    if(!enif_is_atom(env, app) ||
       !enif_get_int(env, argv[1], &metric_id)) {
      return false;
    }

    samples_t *mp = nullptr;
    app_metrics.apply(app,
                      [&](std::vector<samples_t*> &mm) {
                        if(mm.size() > metric_id) {
                          mp = mm[metric_id];
                        }
                      });
    if(!mp) return false;

    metric = mp;
    return true;
  }
  return false;
}

static ERL_NIF_TERM report_random(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  samples_t *mp = nullptr;
  if(!get_metric(env, argc, argv, mp)) return ATOM_ERROR;
  samples_t &metric = *mp;

  unsigned val = metric.rnd() % 1000000;
  metric.add(val);
  return ATOM_OK;
}

static ERL_NIF_TERM report(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  samples_t *mp = nullptr;
  if(!get_metric(env, argc - 1, argv, mp)) return ATOM_ERROR;
  samples_t &metric = *mp;

  int val;
  if(!((argc == 2) && (enif_get_int(env, argv[1], &val))) &&
     !((argc == 3) && (enif_get_int(env, argv[2], &val)))) {
    return ATOM_ERROR;
  }

  metric.add(val);
  return ATOM_OK;
}

static ERL_NIF_TERM query(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  samples_t *mp = nullptr;
  if(!get_metric(env, argc, argv, mp)) return ATOM_ERROR;
  samples_t &metric = *mp;

  auto r = metric.read();

  return enif_make_tuple(env,
                         8,
                         ATOM_METRIC,
                         enif_make_int64(env, r.min),
                         enif_make_int64(env, r.p50),
                         enif_make_int64(env, r.p90),
                         enif_make_int64(env, r.p95),
                         enif_make_int64(env, r.p99),
                         enif_make_int64(env, r.p999),
                         enif_make_int64(env, r.max));
}

void collector() {
  while(running.load(std::memory_order_relaxed)) {
    usleep(1000000);
    for(samples_t &metric : d_metrics) {
      metric.collect();
    }
    for(samples_t &metric : metrics) {
      metric.collect();
    }
  }
}

void *collector_nif(void *p) {
  collector();
  return NULL;
}

extern "C" {
static int on_load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
{
  ATOM_ERROR = enif_make_atom(env, "error");
  ATOM_OK = enif_make_atom(env, "ok");
  ATOM_METRIC = enif_make_atom(env, "metric");
  running.store(true);
  // collector_thread = std::thread(collector);
  enif_thread_create((char*)"jtstat_collector", &collector_ethread, &collector_nif, NULL, NULL);
  return 0;
}

static void on_unload(ErlNifEnv *env, void *priv_data)
{
  running.store(false);
  // collector_thread.join();
  enif_thread_join(collector_ethread, NULL);
}

static ErlNifFunc nif_funcs[] =
{
  // global, non-namespaced metrics
  {"setup_metrics", 1, setup_metrics},
  {"report_random", 1, report_random},
  {"report", 2, report},
  {"query", 1, query},

  // application namespaced metrics
  {"setup_metrics", 2, setup_metrics},
  {"report_random", 2, report_random},
  {"report", 3, report},
  {"query", 2, query}
};

ERL_NIF_INIT(jtstat, nif_funcs, &on_load, NULL, NULL, &on_unload)
}
