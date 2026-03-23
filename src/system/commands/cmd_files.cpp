/*
  HexaOS - cmd_files.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Filesystem inspection and control commands for HexaOS.
  Registers: files status, files use, files ls, files cat, files info,
             files rm, files mkdir, files rmdir, files write, files append,
             files format.

  Active backend selection (flash or sd) is a session-level flag that resets
  to flash on every boot. All file operations are dispatched to the currently
  selected backend. The selection is independent from the underlying mount
  state — commands report an error when the selected backend is not mounted.

  Gated by HX_ENABLE_MODULE_STORAGE. Individual flash/SD branches are further
  gated by HX_ENABLE_FEATURE_LITTLEFS and HX_ENABLE_FEATURE_SD respectively.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "headers/hx_build.h"
#include "system/core/runtime.h"

#if HX_ENABLE_MODULE_STORAGE

#if HX_ENABLE_FEATURE_LITTLEFS
  #include "system/handlers/files_handler.h"
#endif

#if HX_ENABLE_FEATURE_SD
  #include "system/adapters/sdmmc_adapter.h"
#endif

// ---------------------------------------------------------------------------
// Active backend selection
// ---------------------------------------------------------------------------

typedef enum : uint8_t {
  HX_FILES_ACTIVE_FLASH = 0,
  HX_FILES_ACTIVE_SD    = 1
} HxFilesActive;

// Defaults to flash on every boot.
static HxFilesActive g_active = HX_FILES_ACTIVE_FLASH;

static const char* ActiveName() {
  return (g_active == HX_FILES_ACTIVE_SD) ? "sd" : "flash";
}

static bool ActiveMounted() {
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD) {
    return Hx.sd_mounted;
  }
#endif
#if HX_ENABLE_FEATURE_LITTLEFS
  return Hx.files_mounted;
#else
  return false;
#endif
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static constexpr size_t      FILES_CAT_MAX = 8192;
static constexpr const char* FILES_SD_VFS  = "/sd";

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

  CmdOutPrintfLine(out, "active = %s", ActiveName());
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
  CmdOutPrintfLine(out, "  sd     mounted=%s", Hx.sd_mounted ? "true" : "false");
  if (Hx.sd_mounted) {
    uint64_t total = 0;
    uint64_t used  = 0;
    if (SdmmcGetStorageInfo(&total, &used)) {
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
    g_active = HX_FILES_ACTIVE_FLASH;
    CmdOutPrintfLine(out, "active backend: flash (mounted=%s)",
                     Hx.files_mounted ? "true" : "false");
    return HX_CMD_OK;
#else
    CmdOutWriteLine(out, "flash not available in this build");
    return HX_CMD_ERROR;
#endif
  }

  if (strcmp(target, "sd") == 0) {
#if HX_ENABLE_FEATURE_SD
    g_active = HX_FILES_ACTIVE_SD;
    CmdOutPrintfLine(out, "active backend: sd (mounted=%s)",
                     Hx.sd_mounted ? "true" : "false");
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

#if HX_ENABLE_FEATURE_LITTLEFS
static bool FlashLsCb(const HxFileInfo* entry, void* user) {
  auto* ctx = static_cast<FilesLsCtx*>(user);
  if (entry->is_dir) {
    CmdOutPrintfLine(ctx->out, "  [DIR]  %s", entry->path);
  } else {
    CmdOutPrintfLine(ctx->out, "  [FILE] %-32s  %u B", entry->path, (unsigned)entry->size_bytes);
  }
  ctx->count++;
  return true;
}
#endif

#if HX_ENABLE_FEATURE_SD
static bool SdLsCb(const char* name, bool is_dir, size_t size_bytes, void* user) {
  auto* ctx = static_cast<FilesLsCtx*>(user);
  if (is_dir) {
    CmdOutPrintfLine(ctx->out, "  [DIR]  %s", name);
  } else {
    CmdOutPrintfLine(ctx->out, "  [FILE] %-32s  %u B", name, (unsigned)size_bytes);
  }
  ctx->count++;
  return true;
}
#endif

static HxCmdStatus CmdFilesLs(const char* args, HxCmdOutput* out) {
  char path[256];

  if (CmdSkipWs(args)[0] == '\0') {
    snprintf(path, sizeof(path), "/");
  } else if (!CmdExtractSingleKey(args, path, sizeof(path))) {
    CmdOutWriteLine(out, "usage: files ls [path]");
    return HX_CMD_USAGE;
  }

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  FilesLsCtx ctx = { out, 0 };
  bool ok = false;

#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) {
    ok = FilesList(path, FlashLsCb, &ctx);
  }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD) {
    ok = SdmmcList(path, SdLsCb, &ctx);
  }
#endif

  if (!ok) {
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) {
    if (!FilesIsFile(path)) {
      CmdOutWriteLine(out, "not a file");
      return HX_CMD_ERROR;
    }
    size_t fsize   = FilesSize(path);
    size_t to_read = (fsize < FILES_CAT_MAX) ? fsize : FILES_CAT_MAX;

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
    bool   ok       = FilesReadBytes(path, buf, to_read, &read_len);
    if (ok && read_len > 0) {
      buf[read_len] = '\0';
      CmdOutWriteRaw(out, reinterpret_cast<const char*>(buf));
      CmdOutWriteRaw(out, "\n");
      if (fsize > FILES_CAT_MAX) {
        CmdOutPrintfLine(out, "(truncated — %u of %u B shown)",
                         (unsigned)FILES_CAT_MAX, (unsigned)fsize);
      }
    }
    free(buf);
    if (!ok) {
      CmdOutWriteLine(out, "read failed");
      return HX_CMD_ERROR;
    }
    return HX_CMD_OK;
  }
#endif

#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD) {
    // SdmmcReadBytes fails when file > buffer, so use POSIX fopen on the VFS
    // path directly to support partial reads on large files.
    char full[300];
    snprintf(full, sizeof(full), "%s%s", FILES_SD_VFS, path);

    FILE* f = fopen(full, "rb");
    if (!f) {
      CmdOutWriteLine(out, "not a file or open failed");
      return HX_CMD_ERROR;
    }

    uint8_t* buf = static_cast<uint8_t*>(malloc(FILES_CAT_MAX + 1));
    if (!buf) {
      fclose(f);
      CmdOutWriteLine(out, "out of memory");
      return HX_CMD_ERROR;
    }

    size_t read_len = fread(buf, 1, FILES_CAT_MAX, f);
    bool   truncated = !feof(f);
    fclose(f);

    if (read_len > 0) {
      buf[read_len] = '\0';
      CmdOutWriteRaw(out, reinterpret_cast<const char*>(buf));
      CmdOutWriteRaw(out, "\n");
      if (truncated) {
        CmdOutPrintfLine(out, "(truncated — first %u B shown)", (unsigned)FILES_CAT_MAX);
      }
    } else {
      CmdOutWriteLine(out, "(empty)");
    }
    free(buf);
    return HX_CMD_OK;
  }
#endif

  return HX_CMD_ERROR;
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) {
    HxFileInfo info{};
    if (!FilesStat(path, &info) || !info.exists) {
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
#endif

#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD) {
    bool   is_dir = false;
    size_t size   = 0;
    if (!SdmmcStat(path, &is_dir, &size)) {
      CmdOutWriteLine(out, "not found");
      return HX_CMD_ERROR;
    }
    CmdOutPrintfLine(out, "  path         = %s", path);
    CmdOutPrintfLine(out, "  type         = %s", is_dir ? "dir" : "file");
    if (!is_dir) {
      PrintStorageSize(out, "size", size);
    }
    return HX_CMD_OK;
  }
#endif

  return HX_CMD_ERROR;
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  bool ok = false;
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) { ok = FilesRemove(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD)    { ok = SdmmcRemove(path); }
#endif

  if (!ok) {
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  bool ok = false;
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) { ok = FilesMkdir(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD)    { ok = SdmmcMkdir(path); }
#endif

  if (!ok) {
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  bool ok = false;
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) { ok = FilesRmdir(path); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD)    { ok = SdmmcRmdir(path); }
#endif

  if (!ok) {
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

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  bool ok = false;
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) {
    ok = append ? FilesAppendText(path, text) : FilesWriteText(path, text);
  }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD) {
    ok = SdmmcWriteBytes(path, reinterpret_cast<const uint8_t*>(text), strlen(text), append);
  }
#endif

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
    CmdOutPrintfLine(out, "usage: files format confirm  (erases all data on %s)", ActiveName());
    return HX_CMD_USAGE;
  }

  if (!ActiveMounted()) {
    CmdOutPrintfLine(out, "%s not mounted", ActiveName());
    return HX_CMD_ERROR;
  }

  bool ok = false;
#if HX_ENABLE_FEATURE_LITTLEFS
  if (g_active == HX_FILES_ACTIVE_FLASH) { ok = FilesFormat(); }
#endif
#if HX_ENABLE_FEATURE_SD
  if (g_active == HX_FILES_ACTIVE_SD)    { ok = SdmmcFormat(); }
#endif

  if (!ok) {
    CmdOutPrintfLine(out, "format failed on %s (not supported or error)", ActiveName());
    return HX_CMD_ERROR;
  }
  CmdOutPrintfLine(out, "format OK: %s", ActiveName());
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
