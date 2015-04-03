#include "hh.hpp"
#include "hh2.hpp"
#include "test.hpp"

///===================================================================
/// Tests
///===================================================================

bool test_store() {
  std::cout << "=================================================\n";
  auto vals = test::generate<std::vector<int>>();
  HH<int, int> hh;

  for(int i = 0; i < hh.capacity; ++i) {
    assert(hh.entries[i].valid == false);
  }

  for(auto i: vals) {
    hh.insert2(i, i * 10);
  }

  auto v1(vals);
  auto v2 = hh.to_vector();

  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());

  std::cout << "v1: " << v1.size() << "\n";
  std::cout << "v2: " << v2.size() << "\n";

  for(auto x : v1) {
    std::cout << x << " ";
  }
  std::cout << "\n";

  for(auto x : v2) {
    std::cout << x.first << "/" << x.second << " ";
  }
  std::cout << "\n";

  test::check(v1.size() == v2.size());

  for(int i = 0, end = v1.size(); i != end; ++i) {
    test::check(v2[i].first  == v1[i]);
    test::check(v2[i].second == v1[i] * 10);
  }

  return true;
}

bool test_read() {
  auto vals = test::generate<std::vector<int>>();
  HH<int, int> hh;

  for(auto i: vals) {
    hh.insert2(i, i * 10);
  }

  for(auto i: vals) {
    int val;
    test::check(hh.read(i, val));
    test::check(val == (i * 10));
  }

  std::sort(vals.begin(), vals.end());
  for(auto i: vals) {
    int val;
    test::check(hh.read(i, val));
    test::check(val == (i * 10));
  }

  return true;
}

bool test_erase() {
  auto vals = test::generate<std::vector<int>>();
  HH<int, int> hh;

  for(auto i: vals) {
    hh.insert2(i, i * 10);
  }

  int first = 0;
  int val;

  if(!vals.empty()) {
    first = vals[0];
    test::check(hh.read(first, val));
    test::check(val == first * 10);
    test::check(hh.erase(first));
    test::check(hh.read(first, val) == false);
  }

  for(auto i: vals) {
    if(i == first) continue;
    test::check(hh.read(i, val));
    test::check(val == i * 10);
  }

  return true;
}

void run_tests() {
  test::run(test_store);
  test::run(test_read);
  test::run(test_erase);
}

///===================================================================

int main(int argc, char **argv) {
  HH2<int,int> hh2;
  hh2.insert(1, 100);
  hh2.insert(2, 200);
  // test_dump(); return 0;
  run_tests(); return 0;
  // test::rerun(test_store); return 0;
  // testing(); return 0;
  HH<int,int> hh;
#if 0
  for(int i = 1; i <= 30; ++i)
    hh.insert((i*16), (i*16)*10);
  hh.insert(1, 10);
  hh.insert(2, 20); 
  hh.insert(1, 10);
#endif
  hh.insert(18, 180);
  for(int i = 1; i <= 2; ++i)
    hh.insert((i*16+1), (i*16+1)*10);
  for(int i = 1; i <= 4; ++i)
    hh.insert2((i*16), (i*16)*10);
  // hh.insert(3, 30);
  // hh.insert(1234, 30);
 return 0;
}
