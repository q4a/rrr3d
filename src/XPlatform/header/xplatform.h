#pragma once
#ifdef _WIN32

#define PATH_SEP '\\'

#else // NOT _WIN32

#define PATH_SEP '/'

#define CP_ACP        0 // Windows ANSI code page
#define CP_THREAD_ACP 3 // ANSI code page of the current thread

template <size_t size> int sprintf_s(char (&buffer)[size], const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, size, format, args);
	va_end(args);
	return result;
}

inline int sprintf_s(char *buffer, size_t size, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int result = vsnprintf(buffer, size, format, ap);
	va_end(ap);
	return result;
}

#endif
