#ifndef MCPE_EXIT_TRACE_H__
#define MCPE_EXIT_TRACE_H__

// Startup tracing is disabled in normal builds.  Keep the macro so the
// existing call sites remain harmless and can be re-enabled locally when
// diagnosing a platform-specific failure.
#define MCPE_EXIT_TRACE(...) ((void)0)

#endif
