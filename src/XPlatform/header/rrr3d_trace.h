/*
 * A trace facility for the graphics stack, off unless asked for.
 *
 * This exists because the Metal backend has already produced one class of bug
 * that is invisible from outside the process: state that is accepted by Metal,
 * passes its validation layer, and silently renders nothing. Finding the
 * stencil-compare-Never bug meant dumping the exact command sequence handed to
 * each render encoder and comparing an encoder that worked against one that did
 * not. That instrumentation is kept rather than deleted, because the next such
 * bug will need the same view.
 *
 * Enable by setting RRR3D_TRACE to a non-empty value; output goes to
 * rrr3d-trace.log in the working directory. When unset every call is a single
 * predictable branch on a cached flag, so it can stay in the hot path.
 *
 * This is deliberately not d9mt's own logging: d9mt::logf is the backend
 * talking about itself, and D9MT_NO_FETRACE silences its per-call trace because
 * that one writes gigabytes. This is our view of the ABI boundary.
 */

#ifndef RRR3D_TRACE_H
#define RRR3D_TRACE_H

#ifdef __cplusplus

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace rrr3d {

inline bool TraceEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("RRR3D_TRACE");
		return value && value[0];
	}();
	return enabled;
}

inline void Trace(const char* fmt, ...)
{
	if (!TraceEnabled())
		return;

	static std::mutex mutex;
	static std::FILE* file = nullptr;

	std::lock_guard<std::mutex> lock(mutex);
	if (!file)
	{
		file = std::fopen("rrr3d-trace.log", "w");
		if (!file)
			return;
	}

	va_list args;
	va_start(args, fmt);
	std::vfprintf(file, fmt, args);
	va_end(args);

	std::fputc('\n', file);
	std::fflush(file);
}

/*
 * Most of these traces are only wanted for the first few occurrences -- the
 * interesting thing is the shape of a frame, not the millionth repeat of it.
 * Each call site gets its own counter.
 */
#define RRR3D_TRACE_FIRST(count, ...)                     \
	do {                                                  \
		if (::rrr3d::TraceEnabled()) {                    \
			static int rrr3dTraceSeen = 0;                \
			if (rrr3dTraceSeen < (count)) {               \
				++rrr3dTraceSeen;                         \
				::rrr3d::Trace(__VA_ARGS__);              \
			}                                             \
		}                                                 \
	} while (0)

#define RRR3D_TRACE(...) ::rrr3d::Trace(__VA_ARGS__)

}

#endif /* __cplusplus */

#endif /* RRR3D_TRACE_H */
