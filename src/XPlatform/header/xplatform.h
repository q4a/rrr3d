#pragma once
#ifdef _WIN32

#define PATH_SEP '\\'

#else // NOT _WIN32

#define PATH_SEP '/'

#define CP_ACP        0 // Windows ANSI code page
#define CP_THREAD_ACP 3 // ANSI code page of the current thread

#endif
