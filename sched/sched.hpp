#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <uv.h>

class Sched
{
public:
  Sched();
  ~Sched();
  auto process() -> void;
  auto processNoWait() -> void;
  auto regIdle(std::function<void()> &&) -> void;

  using TimerCanceler = std::function<void()>;
  auto regTimer(std::function<void()> &&, std::chrono::milliseconds, bool repeat = false)
    -> TimerCanceler;

  uv_loop_t loop;

private:
  uv_idle_t idle;
  std::function<void()> idleFunc;

  struct TimerData
  {
    uv_timer_t handle;
    bool repeat;
    std::function<void()> cb;
    Sched *sched;
  };
  std::unordered_map<TimerData *, std::shared_ptr<TimerData>> timers;
};
