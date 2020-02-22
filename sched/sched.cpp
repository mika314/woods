#include "sched.hpp"
#include <stdexcept>

Sched::Sched()
{
  uv_loop_init(&loop);
  uv_idle_init(&loop, &idle);
  idle.data = this;
}

Sched::~Sched()
{
  if (idleFunc)
    uv_idle_stop(&idle);
  uv_loop_close(&loop);
}

auto Sched::process() -> void
{
  uv_run(&loop, UV_RUN_ONCE);
}

auto Sched::processNoWait() -> void
{
  uv_run(&loop, UV_RUN_NOWAIT);
}

auto Sched::regIdle(std::function<void()> &&func) -> void
{
  if (!func)
  {
    uv_idle_stop(&idle);
    return;
  }

  idleFunc = func;
  uv_idle_start(&idle, [](uv_idle_t *ctx) { static_cast<Sched *>(ctx->data)->idleFunc(); });
}

static void checkError(int err)
{
  if (err != 0)
    throw std::runtime_error("Error in UV lib: " + std::to_string(err));
}

auto Sched::regTimer(std::function<void()> &&cb, std::chrono::milliseconds timeout, bool repeat)
  -> TimerCanceler
{
  auto timerData = std::make_shared<TimerData>();
  auto err = uv_timer_init(&loop, &timerData->handle);
  checkError(err);
  timerData->handle.data = timerData.get();
  timerData->repeat = repeat;
  timerData->cb = std::move(cb);
  timerData->sched = this;
  err = uv_timer_start(&timerData->handle,
                       [](uv_timer_t *handle) {
                         auto timerData = static_cast<Sched::TimerData *>(handle->data);
                         timerData->cb();
                         if (!timerData->repeat)
                         {
                           timerData->sched->timers.erase(timerData);
                           uv_close((uv_handle_t *)handle, [](uv_handle_t *) {});
                         }
                       },
                       repeat ? 0 : timeout.count(),
                       repeat ? timeout.count() : 0);
  checkError(err);
  timers[timerData.get()] = timerData;
  std::weak_ptr<TimerData> timerDataWeak = timerData;
  return [timerDataWeak]() {
    if (auto timerData = timerDataWeak.lock())
    {
      auto it = timerData->sched->timers.find(timerData.get());
      if (it == std::end(timerData->sched->timers))
        return;
      auto err = uv_timer_stop(&timerData->handle);
      checkError(err);
      timerData->sched->timers.erase(timerData.get());
      uv_close((uv_handle_t *)&timerData->handle, [](uv_handle_t *) {});
    }
  };
}
