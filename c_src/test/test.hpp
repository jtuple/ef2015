#include "dump.hpp"

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace test {
///===================================================================

static size_t test_size = 5;
static bool test_rerun = false;
static bool test_history = false;
static int saved = 1;

template <class T>
struct generator {
  T operator()() {
    assert(0);
  }
};

template <class X>
struct generator<std::vector<X>> {
  std::vector<X> operator()() {
    std::vector<X> v(test_size);
    for(size_t i = 0; i < test_size; ++i)
      v[i] = X(i);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(v.begin(), v.end(), std::default_random_engine(seed));

    return v;
  }
};

std::string gen_name() {
  std::ostringstream oss;
  oss << ".test/gen" << saved;
  if(test_history)
    saved++;
  return oss.str();
}

template <class T>
T generate() {
  if(test_rerun) {
    T tmp;
    Dump d(gen_name().c_str(), false);
    d.read(tmp);
    return tmp;
  }
  else {    
    generator<T> g;
    auto ret = g();
    Dump d(gen_name().c_str(), true);
    d.write(ret);
    return ret;
  }
}

template <class Test>
void run(Test test, int runs = 10) {
  test_history = true;
  for(int run = 0; run < runs; ++run) {
    switch(run) {
      case 0:
        test_size = 0; break;
      case 1:
        test_size = 1; break;
      case 2:
        test_size = 2; break;
      default:
        test_size = test_size + 10;
    }
    try {
      test();
    }
    catch (...) {
      std::cout << "Test failed\n";
      exit(1);
    }
  }
}

template <class Test>
void rerun(Test test, int runs = 10) {
  test_history = true;
  test_rerun = true;
  for(int run = 0; run < runs; ++run) {
    try {
      test();
    }
    catch (...) {
      std::cout << "Test failed\n";
      exit(1);
    }
  }
}

void check(bool success) {
  if(!success)
    throw std::runtime_error{"test check failed"};
}

///===================================================================
}

bool simple() {
  auto v = test::generate<std::vector<int>>();
  for(auto i : v) {
    std::cout << i << " ";
  }
  std::cout << "\n";
  return true;
}

void testing() {
  test::run(simple);
}
