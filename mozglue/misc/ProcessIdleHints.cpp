/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcessIdleHints.h"

#if defined(XP_LINUX)
#  include <fcntl.h>
#  include <stdio.h>
#  include <string.h>
#  include <sys/mman.h>
#  include <unistd.h>
// MADV_COLD / MADV_PAGEOUT were added in Linux 5.4 (Nov 2019). Define
// here so builds against older system headers still compile; at runtime
// the madvise() call will simply return EINVAL on older kernels and we
// treat that as a no-op.
#  ifndef MADV_COLD
#    define MADV_COLD 20
#  endif
#elif defined(XP_WIN)
#  include <windows.h>
#  include <psapi.h>
#endif

namespace mozilla {

#if defined(XP_LINUX)

// Parse a single /proc/self/maps line and apply `aAdvice` to that
// mapping if it is anonymous (or [heap]) and read/write private. Returns
// true on a successful madvise call.
//
// Line layout (man 5 proc):
//   address           perms offset  dev   inode  pathname
//   00400000-00452000 r-xp 00000000 08:02 173521 /usr/bin/cat
//
// Intentionally skipped:
//   - any file-backed mapping (real pathname),
//   - kernel-special mappings other than [heap] (e.g. [stack], [vdso],
//     [vvar], [vsyscall], thread stacks).
static bool AdviseMappingLine(const char* aLine, int aAdvice) {
  unsigned long start = 0;
  unsigned long end = 0;
  char perms[8] = {0};
  char pathname[256] = {0};
  // %255s stops at whitespace, so "[heap]" lands fine. If the field is
  // absent the buffer stays empty (we initialised it).
  int parsed = sscanf(aLine, "%lx-%lx %7s %*x %*s %*s %255s", &start, &end,
                      perms, pathname);
  if (parsed < 3) {
    return false;
  }
  if (perms[0] != 'r' || perms[1] != 'w' || perms[3] != 'p') {
    return false;
  }
  // Allow: empty pathname (anonymous mmap) or "[heap]".
  if (pathname[0] != '\0' && strcmp(pathname, "[heap]") != 0) {
    return false;
  }
  size_t len = (size_t)(end - start);
  if (len == 0) {
    return false;
  }
  return madvise(reinterpret_cast<void*>(start), len, aAdvice) == 0;
}

static int AdviseAnonymousMappings(int aAdvice) {
  // The "e" mode flag asks glibc to set O_CLOEXEC on the underlying fd.
  FILE* fp = fopen("/proc/self/maps", "re");
  if (!fp) {
    return 0;
  }
  int succeeded = 0;
  // /proc/.../maps lines are bounded; 1024 has comfortable headroom for
  // even long pathnames.
  char line[1024];
  while (fgets(line, static_cast<int>(sizeof(line)), fp)) {
    if (AdviseMappingLine(line, aAdvice)) {
      ++succeeded;
    }
  }
  fclose(fp);
  return succeeded;
}

MFBT_API bool ProcessIdleHints::MarkIdle() {
  return AdviseAnonymousMappings(MADV_COLD) > 0;
}

MFBT_API bool ProcessIdleHints::MarkActive() {
  return AdviseAnonymousMappings(MADV_WILLNEED) > 0;
}

#elif defined(XP_WIN)

MFBT_API bool ProcessIdleHints::MarkIdle() {
  // Trims this process's working set. The OS reclaims pages under
  // memory pressure and the Windows Memory Compression mechanism
  // (default since Windows 10) compresses them.
  return ::EmptyWorkingSet(::GetCurrentProcess()) != 0;
}

MFBT_API bool ProcessIdleHints::MarkActive() {
  // Windows has no first-class "warm whole working set" API. Touching
  // pages defensively would only pollute caches; pages are demand-paged
  // back in naturally on first access. Report no-op.
  return false;
}

#else

// macOS / BSD / etc.: posix_madvise has POSIX_MADV_DONTNEED but its
// semantics differ across platforms (advisory on macOS, destructive on
// Linux). Rather than risk a correctness regression, treat unsupported
// platforms as a no-op until platform-specific implementations are added.
MFBT_API bool ProcessIdleHints::MarkIdle() { return false; }
MFBT_API bool ProcessIdleHints::MarkActive() { return false; }

#endif

}  // namespace mozilla
