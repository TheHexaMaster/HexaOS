/*
  HexaOS - cmd_files.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Filesystem inspection and control commands for HexaOS.
  Registers: files status, files use, files ls, files cat, files info,
             files rm, files mkdir, files rmdir, files write, files append,
             files format.

  All operations are dispatched through files_handler — no adapter is
  accessed directly. Backend selection is owned by files_handler;
  commands read and set it via FilesGetActiveBackend / FilesSetActiveBackend.

  Gated by HX_ENABLE_MODULE_STORAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "headers/hx_build.h"
#include "system/core/runtime.h"

#if HX_ENABLE_MODULE_STORAGE

#include "system/handlers/files_handler.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static constexpr size_t FILES_CAT_MAX = HX_FILES_CAT_MAX;

static void PrintStorageSize(HxCmdOutput* out, const char* label, size_t bytes) {
  if (bytes >= 1024UL * 1024UL) {
    CmdOutPrintfLine(out, "  %-12s = %u MB (%u B)",
                     label, (unsigned)(bytes / (1024UL * 1024UL)), (unsigned)bytes);
  } else if (bytes >= 1024UL) {
    CmdOutPrintfLine(out, "  %-12s = %u KB (%u B)",
                     label, (unsigned)(bytes / 1024UL), (unsigned)bytes);
  } else {
    CmdOutPrintfLine(out, "  %-12s = %u B", label, (unsigned)bytes);
  }
}

// ---------------------------------------------------------------------------
// files / files status
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesStatus(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: files status");
    return HX_CMD_USAGE;
  }

  CmdOutPrintfLine(out, "active = %s", FilesActiveBackendName());
  CmdOutWriteLine(out, "backends:");

#if HX_ENABLE_FEATURE_LITTLEFS
  CmdOutPrintfLine(out, "  flash  mounted=%s", Hx.files_mounted ? "true" : "false");
  if (Hx.files_mounted) {
    HxFilesInfo info{};
    if (FilesGetInfo(&info)) {
      CmdOutPrintfLine(out, "         total=%u KB  used=%u KB  free=%u KB",
                       (unsigned)(info.total_bytes / 1024),
                       (unsigned)(info.used_bytes  / 1024),
                       (unsigned)(info.free_bytes  / 1024));
    }
  }
#else
  CmdOutWriteLine(out, "  flash  disabled (build)");
#endif

#if HX_ENABLE_FEATURE_SD
  CmdOutPrintfLine(out, "  sd     mounted=%s", FilesSdIsMounted() ? "true" : "false");
  if (FilesSdIsMounted()) {
    uint64_t total = 0;
    uint64_t used  = 0;
    if (FilesSdGetStorageInfo(&total, &used)) {
      uint64_t free_b = (total > used) ? total - used : 0;
      CmdOutPrintfLine(out, "         total=%lu MB  used=%lu MB  free=%lu MB",
                       (unsigned long)(total  / (1024ULL * 1024ULL)),
                       (unsigned long)(used   / (1024ULL * 1024ULL)),
                       (unsigned long)(free_b / (1024ULL * 1024ULL)));
    }
  }
#else
  CmdOutWriteLine(out, "  sd     disabled (build)");
#endif

  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files use <flash|sd>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesUse(const char* args, HxCmdOutput* out) {
  char target[16];
  if (!CmdExtractSingleKey(args, target, sizeof(target))) {
    CmdOutWriteLine(out, "usage: files use <flash|sd>");
    return HX_CMD_USAGE;
  }

  if (strcmp(target, "flash") == 0) {
#if HX_ENABLE_FEATURE_LITTLEFS
    FilesSetActiveBackend(HX_FILES_BACKEND_FLASH);
    CmdOutPrintfLine(out, "active backend: flash (mounted=%s)",
                     FilesActiveIsMounted() ? "true" : "false");
    return HX_CMD_OK;
#else
    CmdOutWriteLine(out, "flash not available in this build");
    return HX_CMD_ERROR;
#endif
  }

  if (strcmp(target, "sd") == 0) {
#if HX_ENABLE_FEATURE_SD
    FilesSetActiveBackend(HX_FILES_BACKEND_SD);
    CmdOutPrintfLine(out, "active backend: sd (mounted=%s)",
                     FilesActiveIsMounted() ? "true" : "false");
    return HX_CMD_OK;
#else
    CmdOutWriteLine(out, "sd not available in this build");
    return HX_CMD_ERROR;
#endif
  }

  CmdOutWriteLine(out, "unknown backend — use 'flash' or 'sd'");
  return HX_CMD_USAGE;
}

// ---------------------------------------------------------------------------
// files ls [path]
// ---------------------------------------------------------------------------

struct FilesLsCtx {
  HxCmdOutput* out;
  size_t       count;
};

static bool LsCb(const HxFileInfo* entry, void* user) {
  auto* ctx = static_cast<FilesLsCtx*>(user);
  if (entry->is_dir) {
    CmdOutPrintfLine(ctx->out, "  [DIR]  %s", entry->path);
  } else {
    CmdOutPrintfLine(ctx->out, "  [FILE] %-32s  %u B", entry->path, (unsigned)entry->size_bytes);
  }
  ctx->count++;
  return true;
}

static HxCmdStatus CmdFilesLs(const char* args, HxCmdOutput* out) {
  char path[256];

  if (CmdSkipWs(args)[0] == '\0') {
    snprintf(path, sizeof(path), "/");
  } else if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files ls [path]");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  FilesLsCtx ctx = { out, 0 };
  if (!FilesActiveList(path, LsCb, &ctx)) {
    CmdOutPrintfLine(out, "ls failed: %s", path);
    return HX_CMD_ERROR;
  }

  if (ctx.count == 0) {
    CmdOutWriteLine(out, "  (empty)");
  }
  CmdOutPrintfLine(out, "%u %s", (unsigned)ctx.count, ctx.count == 1 ? "entry" : "entries");
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files cat <path>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesCat(const char* args, HxCmdOutput* out) {
  char path[256];
  if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files cat <path>");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  HxFileInfo info{};
  if (!FilesActiveStat(path, &info) || !info.exists || info.is_dir) {
    CmdOutWriteLine(out, "not a file");
    return HX_CMD_ERROR;
  }

  size_t to_read = (info.size_bytes < FILES_CAT_MAX) ? info.size_bytes : FILES_CAT_MAX;

  if (to_read == 0) {
    CmdOutWriteLine(out, "(empty)");
    return HX_CMD_OK;
  }

  uint8_t* buf = static_cast<uint8_t*>(malloc(to_read + 1));
  if (!buf) {
    CmdOutWriteLine(out, "out of memory");
    return HX_CMD_ERROR;
  }

  size_t read_len = 0;
  bool   ok       = FilesActiveReadBytes(path, buf, to_read, &read_len);
  if (ok && read_len > 0) {
    buf[read_len] = '\0';
    CmdOutWriteRaw(out, reinterpret_cast<const char*>(buf));
    CmdOutWriteRaw(out, "\n");
    if (info.size_bytes > FILES_CAT_MAX) {
      CmdOutPrintfLine(out, "(truncated — %u of %u B shown)",
                       (unsigned)FILES_CAT_MAX, (unsigned)info.size_bytes);
    }
  } else if (ok) {
    CmdOutWriteLine(out, "(empty)");
  }
  free(buf);

  if (!ok) {
    CmdOutWriteLine(out, "read failed");
    return HX_CMD_ERROR;
  }
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files info <path>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesInfo(const char* args, HxCmdOutput* out) {
  char path[256];
  if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files info <path>");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  HxFileInfo info{};
  if (!FilesActiveStat(path, &info) || !info.exists) {
    CmdOutWriteLine(out, "not found");
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "  path         = %s", info.path);
  CmdOutPrintfLine(out, "  type         = %s", info.is_dir ? "dir" : "file");
  if (!info.is_dir) {
    PrintStorageSize(out, "size", info.size_bytes);
  }
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files rm <path>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesRm(const char* args, HxCmdOutput* out) {
  char path[256];
  if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files rm <path>");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  if (!FilesActiveRemove(path)) {
    CmdOutPrintfLine(out, "rm failed: %s", path);
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "removed: %s", path);
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files mkdir <path>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesMkdir(const char* args, HxCmdOutput* out) {
  char path[256];
  if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files mkdir <path>");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  if (!FilesActiveMkdir(path)) {
    CmdOutPrintfLine(out, "mkdir failed: %s", path);
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "created: %s", path);
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files rmdir <path>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesRmdir(const char* args, HxCmdOutput* out) {
  char path[256];
  if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files rmdir <path>");
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  if (!FilesActiveRmdir(path)) {
    CmdOutPrintfLine(out, "rmdir failed: %s", path);
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "removed dir: %s", path);
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// files write <path> <text>
// files append <path> <text>
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesWriteOrAppend(const char* args, HxCmdOutput* out, bool append) {
  const char* cursor = args;
  char path[256];

  if (!CmdExtractToken(&cursor, path, sizeof(path))) {
    CmdOutPrintfLine(out, "usage: files %s <path> <text>", append ? "append" : "write");
    return HX_CMD_USAGE;
  }

  // Everything after the path (may be empty string) is the text content.
  const char* text = CmdSkipWs(cursor);

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  bool ok = append ? FilesActiveAppendText(path, text) : FilesActiveWriteText(path, text);
  if (!ok) {
    CmdOutPrintfLine(out, "%s failed: %s", append ? "append" : "write", path);
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "%s OK: %s (%u B)",
                   append ? "append" : "write", path, (unsigned)strlen(text));
  return HX_CMD_OK;
}

static HxCmdStatus CmdFilesWrite(const char* args, HxCmdOutput* out) {
  return CmdFilesWriteOrAppend(args, out, false);
}

static HxCmdStatus CmdFilesAppend(const char* args, HxCmdOutput* out) {
  return CmdFilesWriteOrAppend(args, out, true);
}

// ---------------------------------------------------------------------------
// files format confirm
// ---------------------------------------------------------------------------

static HxCmdStatus CmdFilesFormat(const char* args, HxCmdOutput* out) {
  char confirm[16];
  if (!CmdExtractSingleKey(args, confirm, sizeof(confirm))
      || strcmp(confirm, "confirm") != 0) {
    CmdOutPrintfLine(out, "usage: files format confirm  (erases all data on %s)",
                     FilesActiveBackendName());
    return HX_CMD_USAGE;
  }

  if (!FilesActiveIsMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }

  if (!FilesActiveFormat()) {
    CmdOutPrintfLine(out, "format failed on %s (not supported or error)", FilesActiveBackendName());
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "format OK: %s", FilesActiveBackendName());
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

static const HxCmdDef kFilesCommands[] = {
  { "files",        CmdFilesStatus, "Show storage backends and active selection" },
  { "files status", CmdFilesStatus, nullptr },
  { "files use",    CmdFilesUse,    "Switch active backend: files use <flash|sd>" },
  { "files ls",     CmdFilesLs,     "List directory: files ls [path]" },
  { "files cat",    CmdFilesCat,    "Print file contents: files cat <path>" },
  { "files info",   CmdFilesInfo,   "Show file or directory info: files info <path>" },
  { "files rm",     CmdFilesRm,     "Remove file: files rm <path>" },
  { "files mkdir",  CmdFilesMkdir,  "Create directory: files mkdir <path>" },
  { "files rmdir",  CmdFilesRmdir,  "Remove empty directory: files rmdir <path>" },
  { "files write",  CmdFilesWrite,  "Write text to file: files write <path> <text>" },
  { "files append", CmdFilesAppend, "Append text to file: files append <path> <text>" },
  { "files format", CmdFilesFormat, "Format active backend: files format confirm" }
};

bool CommandRegisterFiles() {
  for (size_t i = 0; i < (sizeof(kFilesCommands) / sizeof(kFilesCommands[0])); i++) {
    if (!CommandRegister(&kFilesCommands[i])) {
      return false;
    }
  }
  return true;
}

#else  // !HX_ENABLE_MODULE_STORAGE

bool CommandRegisterFiles() {
  return true;
}

#endif // HX_ENABLE_MODULE_STORAGE
