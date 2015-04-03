#ifndef DUMP_H
#define DUMP_H

#include <cstdio>
#include <cerrno>
#include <system_error>
#include <iostream>
#include <vector>

struct Dump {
  FILE *fd;

  Dump(const char *fname, bool truncate = false) : fd(nullptr) {
    if(truncate) fd = fopen(fname, "w+");
    else         fd = fopen(fname, "r+");
    if(!fd) {
      throw std::system_error(errno, std::system_category());
    }
  }

  ~Dump() {
    if(fd) {
      fclose(fd);
      fd = nullptr;
    }
  }

  ///===================================================================

  void write(uint64_t i) {
    fwrite(&i, sizeof(uint64_t), 1, fd);
  }

  void write(int i) {
    uint64_t x = i;
    write(x);
  }

  template <class T>
  void write(std::vector<T> &v) {
    uint64_t size = v.size();
    write(size);
    for(auto &x : v) write(x);
  }


  ///===================================================================

  void read(uint64_t &out) {
    fread(&out, sizeof(uint64_t), 1, fd);
  }

  void read(int &out) {
    uint64_t x;
    read(x);
    out = x;
  }

  template <class T>
  void read(std::vector<T> &v) {
    uint64_t size;
    read(size);
    v.resize(size);
    for(int i = 0; i < size; ++i) {
      read(v[i]);
    }
  }

  ///===================================================================
};

#if 0
void test_dump() {
  {
    Dump d("out", true);
    d.write(9);
    std::vector<int> v{{1,2,3,4,5}};
    d.write(v);
  }
  {
    Dump d("out");
    uint64_t x;
    d.read(x);
    std::vector<int> v;
    d.read(v);
    std::cout << x << "\n";
    for(auto i : v) {
      std::cout << i << " ";
    }
    std::cout << "\n";
  }
}
#endif

#endif
