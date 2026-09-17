#ifndef TOUCH_TRACE_H__
#define TOUCH_TRACE_H__

// Temporary diagnostic hooks for touch and mouse input.  They intentionally
// compile to no-ops in normal builds: the game must not create a runtime touch
// log unless a focused diagnostic build explicitly restores an implementation.
#define TOUCH_TRACE(...) ((void)0)

#endif
