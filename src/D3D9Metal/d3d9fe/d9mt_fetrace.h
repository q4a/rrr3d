// d9mt DEBUG-ONLY frontend trace — writes to d9mt-fe-trace.log in the process
// cwd (game dir), append+fflush per line so the last line survives a hang.
// Standalone (only <cstdio>/<cstdarg>/<mutex>) so it can be included from the
// vendored DXVK d3d9 sources without pulling in winemetal/vulkan headers.
// REMOVE all FETRACE() calls + this header before a final build.
#pragma once
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <mutex>

namespace dxvk { namespace d9mt {
  inline void fetrace(const char* fmt, ...) {
    static std::mutex s_m;
    static FILE*      s_f = nullptr;
    std::lock_guard<std::mutex> lk(s_m);
    if (!s_f) { s_f = std::fopen("d9mt-fe-trace.log", "w"); if (!s_f) return; }
    static const auto s_t0 = std::chrono::steady_clock::now();
    std::fprintf(s_f, "[%8lld] ", (long long) std::chrono::duration_cast<
      std::chrono::milliseconds>(std::chrono::steady_clock::now() - s_t0).count());
    va_list ap; va_start(ap, fmt);
    std::vfprintf(s_f, fmt, ap);
    va_end(ap);
    std::fputc('\n', s_f);
    std::fflush(s_f);
  }
} }
// Gated off in RELEASE (D9MT_NO_LOG) so instrumentation adds zero perturbation.
//
// rrr3d: D9MT_NO_FETRACE added. D9MT_NO_LOG silences d9mt::logf too, and that
// one is low-volume and worth keeping -- it is where the backend reports
// pipeline and surface failures. This trace is per-call with an fflush each
// time; left on it wrote 2.3 GB in a minute and dominated frame time.
#if defined(D9MT_NO_LOG) || defined(D9MT_NO_FETRACE)
#define FETRACE(...) ((void)0)
#else
#define FETRACE(...) ::dxvk::d9mt::fetrace(__VA_ARGS__)
#endif
