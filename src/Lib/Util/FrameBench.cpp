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
#include "Util/FrameBench.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SDL.h"

bool FrameBench::enabled = false;
bool FrameBench::keep_sleep = false;
bool FrameBench::reported = false;
const char* FrameBench::csv_path = 0;

size_t FrameBench::tick_budget = 0;
size_t FrameBench::ticks_done = 0;

unsigned long long FrameBench::run_start_us = 0;
unsigned long long FrameBench::tick_start_us = 0;
unsigned long long FrameBench::phase_start_us = 0;
unsigned long long FrameBench::tick_work_us = 0;

unsigned long long FrameBench::phase_total_us[FrameBench::PHASE_COUNT] = {0, 0,
                                                                          0, 0};

unsigned int* FrameBench::samples = 0;
size_t FrameBench::samples_capacity = 0;

namespace {

/// SDL_GetTicks() only has millisecond resolution, which is too coarse when a
/// tick is the thing being measured.
unsigned long long nowMicroseconds() {
  const Uint64 counter = SDL_GetPerformanceCounter();
  const Uint64 freq = SDL_GetPerformanceFrequency();
  if (freq == 0) return 0;
  return (unsigned long long)((counter * 1000000ULL) / freq);
}

const char* PHASE_NAMES[FrameBench::PHASE_COUNT] = {"input", "graphics", "sim",
                                                    "sleep"};

}  // namespace

void FrameBench::initialize() {
  static bool done = false;
  if (done) return;
  done = true;

  const char* ticks_env = getenv("NETPANZER_BENCH_TICKS");
  if (ticks_env == 0 || *ticks_env == '\0') return;

  const long requested = strtol(ticks_env, 0, 10);
  if (requested <= 0) return;

  enabled = true;
  tick_budget = (size_t)requested;
  keep_sleep = getenv("NETPANZER_BENCH_SLEEP") != 0;
  csv_path = getenv("NETPANZER_BENCH_CSV");

  samples_capacity = tick_budget;
  samples = new unsigned int[samples_capacity];

  // run_start_us is stamped on the first tick, not here: map loading and
  // resource init happen in between, and folding them into ticks/sec makes a
  // fast loop look slow.

  printf("[bench] measuring %zu ticks (frame limiter %s)\n", tick_budget,
         keep_sleep ? "on" : "bypassed");
  fflush(stdout);
}

void FrameBench::beginTick() {
  if (!enabled) return;
  tick_start_us = nowMicroseconds();
  if (run_start_us == 0) run_start_us = tick_start_us;
  phase_start_us = tick_start_us;
  tick_work_us = 0;
}

void FrameBench::endPhase(Phase phase) {
  if (!enabled) return;
  const unsigned long long now = nowMicroseconds();
  const unsigned long long spent = now - phase_start_us;
  phase_total_us[phase] += spent;
  // Sleep is the limiter doing its job, not work the tick had to do. Keeping
  // it out of the sample is what makes a throttled run comparable with an
  // unthrottled one.
  if (phase != PHASE_SLEEP) tick_work_us += spent;
  phase_start_us = now;
}

bool FrameBench::endTick() {
  if (!enabled) return true;

  if (ticks_done < samples_capacity) {
    samples[ticks_done] = (unsigned int)std::min(tick_work_us, 0xFFFFFFFFULL);
  }
  ticks_done++;

  if (ticks_done >= tick_budget) {
    report();
    return false;
  }
  return true;
}

void FrameBench::report() {
  if (!enabled || reported) return;
  reported = true;

  const unsigned long long wall_us = nowMicroseconds() - run_start_us;
  const size_t n = std::min(ticks_done, samples_capacity);
  if (n == 0) {
    printf("[bench] no ticks recorded\n");
    fflush(stdout);
    return;
  }

  if (csv_path != 0) {
    FILE* csv = fopen(csv_path, "w");
    if (csv != 0) {
      fprintf(csv, "tick_us\n");
      for (size_t i = 0; i < n; i++) fprintf(csv, "%u\n", samples[i]);
      fclose(csv);
      printf("[bench] per-tick samples written to %s\n", csv_path);
    } else {
      printf("[bench] could not open %s for writing\n", csv_path);
    }
  }

  // Percentiles want sorted data, but the CSV above wants the original order,
  // so sort only after it has been written.
  unsigned int* sorted = new unsigned int[n];
  memcpy(sorted, samples, n * sizeof(unsigned int));
  std::sort(sorted, sorted + n);

  unsigned long long sum = 0;
  for (size_t i = 0; i < n; i++) sum += sorted[i];

  const double mean_ms = (double)sum / (double)n / 1000.0;
  const double p50_ms = sorted[n / 2] / 1000.0;
  const double p90_ms = sorted[(size_t)(n * 0.90)] / 1000.0;
  const double p99_ms = sorted[std::min(n - 1, (size_t)(n * 0.99))] / 1000.0;
  const double max_ms = sorted[n - 1] / 1000.0;
  const double wall_s = wall_us / 1000000.0;

  printf("\n[bench] ==== %zu ticks in %.2fs ====\n", n, wall_s);
  printf("[bench] work per tick, excluding the frame limiter:\n");
  printf("[bench] mean       %8.3f ms   (%.1f%% of the %d ms budget)\n",
         mean_ms, mean_ms * 100.0 / 20.0, 20);
  printf("[bench] p50        %8.3f ms\n", p50_ms);
  printf("[bench] p90        %8.3f ms\n", p90_ms);
  printf("[bench] p99        %8.3f ms\n", p99_ms);
  printf("[bench] max        %8.3f ms\n", max_ms);
  printf("[bench] ticks/sec  %8.1f  (meaningful only when unthrottled)\n",
         wall_s > 0 ? n / wall_s : 0.0);
  printf("[bench] ---- mean per phase ----\n");
  for (int p = 0; p < PHASE_COUNT; p++) {
    printf("[bench] %-9s  %8.3f ms\n", PHASE_NAMES[p],
           (double)phase_total_us[p] / (double)n / 1000.0);
  }
  printf("\n");
  fflush(stdout);

  delete[] sorted;
}
