# HexaOS LittleFS Handler Manual

## Overview

The HexaOS LittleFS handler is the filesystem service layer used by the rest of the system to access the `littlefs` partition without talking to Arduino `FS` or `LittleFS` primitives directly.

It provides:

- filesystem lifecycle control
- path validation
- file and directory operations
- text and binary I/O
- atomic write helpers
- storage information queries
- directory listing through a callback API
- synchronized access through an internal mutex

The implementation lives in:

- `system/handlers/littlefs_handler.h`
- `system/handlers/littlefs_handler.cpp`

It is compiled only when `HX_ENABLE_HANDLER_LITTLEFS` is enabled.

---

## Design goals

The handler is intentionally a **service API**, not a direct filesystem wrapper.

This means:

- other HexaOS modules should call `Files...()` functions
- only the handler knows that the backend is currently `LittleFS`
- backend details remain isolated from the rest of the system

This keeps HexaOS cleaner and makes future backend changes easier.

---

## Internal behavior

### Partition label

The handler mounts the filesystem using the partition label:

`littlefs`

### Synchronization

All public operations are serialized by an internal mutex.

This means:

- concurrent access from multiple execution contexts is protected
- public `Files...()` calls are safe to use from different parts of HexaOS

### Mounted state

The mounted flag is mirrored into the global runtime state:

- `Hx.littlefs_mounted`

This flag is protected by a critical section.

### Path rules

A path is considered valid only if:

- it is not null
- it is not empty
- it starts with `/`
- its total length is at most 255 characters

Invalid paths are rejected.

---

## Public data structures

### `HxFileInfo`

Used for file metadata and directory listing entries.

```cpp
struct HxFileInfo {
  char path[256];
  size_t size_bytes;
  bool exists;
  bool is_dir;
};
```

Fields:

- `path` - full path or listed entry path
- `size_bytes` - file size in bytes, `0` for directories
- `exists` - whether the object exists
- `is_dir` - whether the object is a directory

### `HxFilesInfo`

Used for filesystem-level information.

```cpp
struct HxFilesInfo {
  bool ready;
  bool mounted;
  const char* partition_label;
  size_t total_bytes;
  size_t used_bytes;
  size_t free_bytes;
};
```

Fields:

- `ready` - handler initialization state
- `mounted` - current mount state
- `partition_label` - backend partition label
- `total_bytes` - total filesystem capacity
- `used_bytes` - currently used bytes
- `free_bytes` - currently free bytes

### `HxFilesListCallback`

Used by `FilesList()`.

```cpp
typedef bool (*HxFilesListCallback)(const HxFileInfo* entry, void* user);
```

Callback return value:

- `true` - continue listing
- `false` - stop listing early

---

## Lifecycle API

### `bool FilesInit();`

Initializes the handler and creates the internal mutex.

Behavior:

- returns `true` if already initialized
- sets `Hx.littlefs_mounted` to `false`
- logs initialization result with tag `FIL`

Expected usage:

- call once during boot before filesystem operations

### `bool FilesMount();`

Mounts the `littlefs` partition.

Behavior:

- uses `LittleFS.begin(true, "", 10, "littlefs")`
- updates `Hx.littlefs_mounted`
- logs total and used bytes on success

Important note:

The `begin(true, ...)` call means **format-on-fail is enabled at mount time**.

### `bool FilesUnmount();`

Unmounts the filesystem.

Behavior:

- calls `LittleFS.end()` if mounted
- clears `Hx.littlefs_mounted`
- returns `true` even if the filesystem was not mounted

### `bool FilesFormat();`

Formats the filesystem and remounts it.

Behavior:

- unmounts first if needed
- calls `LittleFS.format()`
- immediately attempts remount
- returns `true` only if both format and remount succeed

---

## Basic path and object operations

### `bool FilesExists(const char* path);`

Returns whether a path exists.

### `bool FilesRemove(const char* path);`

Removes a file.

Note:

- this is intended for file removal
- directory removal should use `FilesRmdir()`

### `bool FilesRename(const char* old_path, const char* new_path);`

Renames or moves a filesystem object.

### `bool FilesMkdir(const char* path);`

Creates a directory.

### `bool FilesRmdir(const char* path);`

Removes a directory.

---

## Metadata and information API

### `bool FilesStat(const char* path, HxFileInfo* out_info);`

Queries metadata for a file or directory.

Behavior:

- fills `out_info`
- returns `false` if the path is invalid, missing, or cannot be opened
- for directories, `size_bytes` is reported as `0`

### `size_t FilesSize(const char* path);`

Returns file size.

Behavior:

- returns `0` for errors
- returns `0` for directories

### `bool FilesIsFile(const char* path);`

Returns `true` if the object exists and is a regular file.

### `bool FilesIsDir(const char* path);`

Returns `true` if the object exists and is a directory.

### `bool FilesGetInfo(HxFilesInfo* out_info);`

Returns filesystem-level information.

Behavior:

- always reports handler `ready` state and partition label
- reports capacity numbers only when mounted

---

## Directory listing API

### `bool FilesList(const char* path, HxFilesListCallback callback, void* user);`

Lists directory contents and calls the provided callback for each entry.

Behavior:

- `path` must point to a directory
- each entry is returned as `HxFileInfo`
- the callback may stop iteration early by returning `false`

Important note:

`FilesList()` keeps the internal filesystem lock while iterating and while calling the callback.

This means the callback should **not** call other `Files...()` functions recursively.

Recommended usage:

- copy needed metadata inside the callback
- return quickly
- do not perform nested filesystem operations from inside the callback

Example:

```cpp
static bool PrintEntry(const HxFileInfo* entry, void* user) {
  (void)user;
  Serial.printf("%s  dir=%d  size=%u\n",
                entry->path,
                entry->is_dir ? 1 : 0,
                (unsigned)entry->size_bytes);
  return true;
}

void ExampleList() {
  FilesList("/", PrintEntry, nullptr);
}
```

---

## Text I/O API

### `String FilesReadText(const char* path);`

Reads the full file into an Arduino `String`.

Behavior:

- returns empty `String` on failure
- returns empty `String` for directories
- reserves capacity based on file size when possible

Use this for:

- JSON files
- small scripts
- configuration exports
- text-based templates

### `bool FilesWriteText(const char* path, const char* text);`

Writes text to a file using overwrite mode.

Behavior:

- null `text` is treated as an empty string
- existing content is replaced

### `bool FilesAppendText(const char* path, const char* text);`

Appends text to a file.

Behavior:

- null `text` is treated as an empty string
- file is opened in append mode

---

## Binary I/O API

### `bool FilesReadBytes(const char* path, uint8_t* out_data, size_t out_size, size_t* out_len);`

Reads an entire file into a caller-provided buffer.

Behavior:

- requires a valid output buffer
- fails if the file is larger than `out_size`
- if `out_len` is provided:
  - it is set to `0` first
  - then updated with file size or actual read length

Recommended usage:

- call `FilesSize()` first if needed
- allocate a sufficiently large buffer
- pass `out_len` to confirm the actual number of bytes read

### `bool FilesWriteBytes(const char* path, const uint8_t* data, size_t len);`

Writes binary data using overwrite mode.

### `bool FilesAppendBytes(const char* path, const uint8_t* data, size_t len);`

Appends binary data.

Validation rules for binary writes:

- `len == 0` is allowed
- if `len > 0`, `data` must not be null

---

## Atomic write API

### `bool FilesWriteTextAtomic(const char* path, const char* text);`

Atomically replaces a text file.

### `bool FilesWriteBytesAtomic(const char* path, const uint8_t* data, size_t len);`

Atomically replaces a binary file.

### Atomic write strategy

The handler uses this sequence:

1. build a temporary path: `original_path + ".tmp"`
2. remove any stale temporary file
3. write the new payload to the temporary file
4. remove the original target if it exists
5. rename the temporary file to the final path

This is the preferred write path for important files.

Use atomic writes for:

- persistent JSON documents
- exported settings
- user data files
- files that must not be left partially written

---

## Logging

The handler uses the HexaOS tagged logging system with tag:

`FIL`

Typical log categories:

- initialization
- mount and unmount
- format results
- write failures
- atomic rename failures

---

## Recommended usage patterns

### Boot sequence

Typical boot flow:

1. `FilesInit()`
2. `FilesMount()`
3. use normal file APIs

### Safe persistent document write

Use atomic writes for important files:

```cpp
const char* json = "{\"ok\":true}";
FilesWriteTextAtomic("/config.json", json);
```

### Simple text log append

```cpp
FilesAppendText("/events.log", "boot complete\n");
```

### Binary read

```cpp
size_t len = 0;
uint8_t buffer[256];
if (FilesReadBytes("/blob.bin", buffer, sizeof(buffer), &len)) {
  // use buffer[0..len-1]
}
```

### Query filesystem capacity

```cpp
HxFilesInfo info = {};
if (FilesGetInfo(&info) && info.mounted) {
  Serial.printf("used=%u free=%u total=%u\n",
                (unsigned)info.used_bytes,
                (unsigned)info.free_bytes,
                (unsigned)info.total_bytes);
}
```

---

## Current limitations

The current handler is intentionally simple and does not yet provide:

- recursive directory walking
- file copy helper
- stream-based file API
- checksums or integrity metadata
- per-file locking
- asynchronous I/O
- shell/console commands for file management

Also note:

- `FilesList()` holds the internal lock during callback execution
- `FilesReadText()` returns an empty `String` both for failure and for an empty file
- `FilesSize()` returns `0` both for failure and for zero-length files

If the caller must distinguish these cases, use `FilesStat()` first.

---

## API reference summary

```cpp
bool FilesInit();
bool FilesMount();
bool FilesUnmount();
bool FilesFormat();

bool FilesExists(const char* path);
bool FilesRemove(const char* path);
bool FilesRename(const char* old_path, const char* new_path);
bool FilesMkdir(const char* path);
bool FilesRmdir(const char* path);

bool FilesStat(const char* path, HxFileInfo* out_info);
size_t FilesSize(const char* path);
bool FilesIsFile(const char* path);
bool FilesIsDir(const char* path);
bool FilesGetInfo(HxFilesInfo* out_info);
bool FilesList(const char* path, HxFilesListCallback callback, void* user);

String FilesReadText(const char* path);
bool FilesReadBytes(const char* path, uint8_t* out_data, size_t out_size, size_t* out_len);

bool FilesWriteText(const char* path, const char* text);
bool FilesAppendText(const char* path, const char* text);
bool FilesWriteBytes(const char* path, const uint8_t* data, size_t len);
bool FilesAppendBytes(const char* path, const uint8_t* data, size_t len);

bool FilesWriteTextAtomic(const char* path, const char* text);
bool FilesWriteBytesAtomic(const char* path, const uint8_t* data, size_t len);
```

---

## Recommendation for HexaOS modules

HexaOS modules should avoid direct use of:

- `LittleFS.open()`
- `LittleFS.exists()`
- `File`
- other Arduino filesystem calls

Instead, they should go through this handler so that filesystem behavior remains centralized, synchronized, and easier to evolve.
