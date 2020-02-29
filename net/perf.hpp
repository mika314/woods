#pragma once
#include <cstdint>

class Perf
{
public:
  Perf(const char *func);
  ~Perf();

private:
  const char *func;
  uint64_t t0;
};