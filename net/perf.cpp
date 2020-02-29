#include "perf.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>

static inline uint64_t getCycles()
{
  uint64_t t;
  __asm volatile("rdtsc" : "=A"(t));
  return t;
}

struct PerfData
{
  PerfData(const char *func, uint64_t t) : func(func), t(t) {}
  const char *func;
  uint64_t t;
};

struct D
{
  ~D()
  {
    std::unordered_map<const char *, std::unordered_map<uint64_t, int>> hist;
    for (const auto d : data)
      ++hist[d.func][d.t / 1000000];
    for (auto &h : hist)
    {
      std::cout << h.first << std::endl;
      for (int i = 0; i < 200; ++i)
        std::cout << h.second[i] << std::endl;
    }
  }
  std::vector<PerfData> data;
};

D &inst()
{
  static D d;
  return d;
}

Perf::Perf(const char *func) : func(func), t0(getCycles()) {}

Perf::~Perf()
{
  inst().data.emplace_back(func, getCycles() - t0);
}
