/*
Copyright (C) 2026 The netPanzer Project

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _FRAMEBENCH_HPP
#define _FRAMEBENCH_HPP

#include <cstddef>

/**
 * Frame and tick timing for benchmark runs.
 *
 * Off unless NETPANZER_BENCH_TICKS is set in the environment, and when it is
 * off the only cost on the main loop is one predictable branch per phase.
 *
 *   NETPANZER_BENCH_TICKS=<n>   run <n> ticks, print a report, then quit
 *   NETPANZER_BENCH_SLEEP=1     keep the frame limiter on (default: off, so
 *                               the run measures work rather than SDL_Delay)
 *   NETPANZER_BENCH_CSV=<path>  also write per-tick microseconds, one per line
 *
 * The report goes to stdout so it survives whatever the logger is doing.
 */
class FrameBench {
 public:
  enum Phase {
    PHASE_INPUT = 0,
    PHASE_GRAPHICS,
    PHASE_SIM,
    PHASE_SLEEP,
    PHASE_COUNT
  };

  /// True when a benchmark run was requested. Cheap enough to call per phase.
  static bool active() { return enabled; }

  /// True when the frame limiter should be bypassed for this run.
  static bool skipSleep() { return enabled && !keep_sleep; }

  /// Read the environment. Safe to call more than once; only the first counts.
  static void initialize();

  static void beginTick();
  /// Close out one phase of the current tick and attribute the time to it.
  static void endPhase(Phase phase);
  /// Returns false once the requested tick count has been reached.
  static bool endTick();

  /// Print the report. Called automatically when the tick budget runs out.
  static void report();

 private:
  static bool enabled;
  static bool keep_sleep;
  static bool reported;
  static const char* csv_path;

  static size_t tick_budget;
  static size_t ticks_done;

  static unsigned long long run_start_us;
  static unsigned long long tick_start_us;
  static unsigned long long phase_start_us;
  /// This tick's time in the non-sleep phases; this is what gets sampled.
  static unsigned long long tick_work_us;

  static unsigned long long phase_total_us[PHASE_COUNT];

  /// Per-tick durations, kept so percentiles are exact rather than estimated.
  static unsigned int* samples;
  static size_t samples_capacity;
};

#endif
