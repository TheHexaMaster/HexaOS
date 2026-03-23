/*
  HexaOS - scheduler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Reusable domain scheduler primitive for HexaOS modules and services.
  Each caller owns its own HxScheduler instance — there is no global registry
  and no system-wide dispatch. This keeps scheduling logic local to the domain
  that needs it and avoids a central mega-scheduler.

  Typical use:
    static HxScheduler g_poll_sched;

    void MyModuleInit() {
      HxSchedulerInit(&g_poll_sched, 500, 0);  // 500 ms interval, no phase offset
    }

    void MyModuleEvery10ms() {
      if (HxSchedulerDue(&g_poll_sched)) {
        DoPoll();
      }
    }

  Notes:
    - All time values are in milliseconds, sourced from TimeMonotonicMs32().
    - HxSchedulerDue() advances next_due by the configured interval on each
      firing. If the system is overloaded and the call arrives late, the next
      deadline is set from the current call time (not from the missed deadline)
      to avoid burst catch-up.
    - budget_ms is informational: callers may use it to skip work when time is
      constrained. The scheduler itself does not enforce it.
    - A scheduler with interval_ms == 0 fires every time HxSchedulerDue() is
      called (pass-through).
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "system/core/time.h"

// ---------------------------------------------------------------------------
// Scheduler instance — embed one per domain, no heap allocation needed
// ---------------------------------------------------------------------------

struct HxScheduler {
  uint32_t interval_ms;     // Cadence between firings. 0 = fire every call.
  uint32_t next_due_ms;     // Monotonic timestamp of next allowed firing.
  uint32_t phase_offset_ms; // One-time offset applied during init.
  uint32_t budget_ms;       // Advisory execution budget. 0 = unconstrained.
  bool     enabled;         // When false HxSchedulerDue() always returns false.
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Initialise a scheduler. First firing is delayed by phase_offset_ms beyond
// the first call to HxSchedulerDue() so staggered polling is easy to set up.
static inline void HxSchedulerInit(HxScheduler* s,
                                   uint32_t interval_ms,
                                   uint32_t phase_offset_ms) {
  if (!s) { return; }
  s->interval_ms     = interval_ms;
  s->phase_offset_ms = phase_offset_ms;
  s->budget_ms       = 0;
  s->enabled         = true;
  // Arm for first firing: now + phase_offset_ms.
  s->next_due_ms = TimeMonotonicMs32() + phase_offset_ms;
}

// Initialise with an explicit advisory execution budget.
static inline void HxSchedulerInitWithBudget(HxScheduler* s,
                                             uint32_t interval_ms,
                                             uint32_t phase_offset_ms,
                                             uint32_t budget_ms) {
  HxSchedulerInit(s, interval_ms, phase_offset_ms);
  if (s) { s->budget_ms = budget_ms; }
}

// Returns true when the scheduler is due to fire and advances the deadline.
// Returns false when disabled, or when the interval has not yet elapsed.
static inline bool HxSchedulerDue(HxScheduler* s) {
  if (!s || !s->enabled) { return false; }
  if (s->interval_ms == 0) { return true; }

  uint32_t now = TimeMonotonicMs32();
  // Cast to int32_t for correct handling of uint32_t wrap-around.
  if ((int32_t)(now - s->next_due_ms) < 0) { return false; }

  // Advance deadline from now to avoid burst catch-up after a late call.
  s->next_due_ms = now + s->interval_ms;
  return true;
}

// Force the scheduler to fire on the very next HxSchedulerDue() call.
static inline void HxSchedulerFireNow(HxScheduler* s) {
  if (!s) { return; }
  s->next_due_ms = TimeMonotonicMs32();
}

// Reschedule: reset the deadline to now + interval_ms + phase_offset_ms.
static inline void HxSchedulerReset(HxScheduler* s) {
  if (!s) { return; }
  s->next_due_ms = TimeMonotonicMs32() + s->interval_ms + s->phase_offset_ms;
}

static inline void HxSchedulerEnable(HxScheduler* s)  { if (s) { s->enabled = true;  } }
static inline void HxSchedulerDisable(HxScheduler* s) { if (s) { s->enabled = false; } }

// Returns true when the remaining budget makes it worth starting work.
// Callers supply the estimated work duration; scheduler checks against budget_ms.
// Always returns true when budget_ms == 0 (unconstrained).
static inline bool HxSchedulerHasBudget(const HxScheduler* s, uint32_t estimated_ms) {
  if (!s || s->budget_ms == 0) { return true; }
  return estimated_ms <= s->budget_ms;
}
