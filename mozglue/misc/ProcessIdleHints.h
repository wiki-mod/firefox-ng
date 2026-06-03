/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_ProcessIdleHints_h
#define mozilla_glue_ProcessIdleHints_h

#include "mozilla/Types.h"

namespace mozilla {

/**
 * Cross-platform hints to the OS that this process is idle (and its
 * memory can be reclaimed aggressively) or about to be active again.
 *
 * Best-effort: on platforms or kernel versions without support, calls are
 * no-ops and return false. Calls are cheap, thread-safe and idempotent.
 *
 * Intended use: a content process whose tab has just become hidden calls
 * MarkIdle so the OS can compress its pages while the user is elsewhere.
 * When the tab becomes visible again, MarkActive un-cools the pages so
 * they are paged back in eagerly.
 */
class ProcessIdleHints {
 public:
  /**
   * Hint that this process is now mostly inactive.
   *
   * Linux: walks anonymous read/write private mappings and applies
   * madvise(MADV_COLD). Pages get paged out / compressed under memory
   * pressure rather than always being eagerly kept resident.
   *
   * Windows: trims the working set via EmptyWorkingSet(). The Windows
   * Memory Compression mechanism (default since Windows 10) compresses
   * pages as they are reclaimed.
   *
   * @return true if at least one underlying advise succeeded.
   */
  static MFBT_API bool MarkIdle();

  /**
   * Hint that this process is about to be active again. Inverse of
   * MarkIdle: a best-effort warmup of recently-cooled pages.
   *
   * @return true if at least one underlying advise succeeded.
   */
  static MFBT_API bool MarkActive();
};

}  // namespace mozilla

#endif  // mozilla_glue_ProcessIdleHints_h
