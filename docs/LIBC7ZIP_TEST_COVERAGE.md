# libc7zip Test Coverage Specification

This document specifies the required test coverage for the `libc7zip` shared library to ensure compatibility with the `sevenzip-go` Go bindings.

## Overview

The Go project dynamically loads the following shared library at runtime:
- Linux: `libc7zip.so`
- macOS: `libc7zip.dylib`
- Windows: `c7zip.dll`

All 26 exported functions must be present and behave according to this specification.

---

## Table of Contents

1. [Exported Symbols Verification](#1-exported-symbols-verification)
2. [Library Management Tests](#2-library-management-tests)
3. [Input Stream Tests](#3-input-stream-tests)
4. [Output Stream Tests](#4-output-stream-tests)
5. [Archive Operations Tests](#5-archive-operations-tests)
6. [Item Property Tests](#6-item-property-tests)
7. [Single Item Extraction Tests](#7-single-item-extraction-tests)
8. [Batch Extraction Tests](#8-batch-extraction-tests)
9. [Callback Behavior Tests](#9-callback-behavior-tests)
10. [Memory Management Tests](#10-memory-management-tests)
11. [Error Handling Tests](#11-error-handling-tests)
12. [Thread Safety Tests](#12-thread-safety-tests)
13. [Integration Test Scenarios](#13-integration-test-scenarios)

---

## 1. Exported Symbols Verification

### TEST-SYM-001: All Required Symbols Exported

**Purpose**: Verify all 26 required symbols are exported from the shared library.

**Required Symbols**:
```
lib_new
lib_get_last_error
lib_get_version
lib_free
in_stream_new
in_stream_get_def
in_stream_commit_def
in_stream_free
out_stream_new
out_stream_get_def
out_stream_free
archive_open
archive_close
archive_free
archive_get_archive_format
archive_get_item_count
archive_get_item
archive_extract_item
archive_extract_several
item_get_archive_index
item_get_string_property
item_get_uint64_property
item_get_bool_property
item_free
string_free
extract_callback_new
extract_callback_get_def
extract_callback_free
```

**Test Method**: Use `dlsym()` (Unix) or `GetProcAddress()` (Windows) to verify each symbol resolves.

**Expected Result**: All 26 symbols resolve to non-NULL function pointers.

---

## 2. Library Management Tests

### TEST-LIB-001: Library Creation

**Purpose**: Verify `lib_new()` creates a valid library instance.

**Test Steps**:
1. Call `lib_new()`
2. Verify return value is non-NULL
3. Call `lib_free()` to clean up

**Expected Result**: Non-NULL pointer returned.

---

### TEST-LIB-002: Library Version String

**Purpose**: Verify `lib_get_version()` returns a valid version string.

**Test Steps**:
1. Call `lib_new()`
2. Call `lib_get_version(lib)`
3. Verify return value is non-NULL
4. Verify string is non-empty
5. Call `lib_free()`

**Expected Result**: Non-NULL, non-empty string returned.

**Note**: The Go code does NOT call `string_free()` on this string. Verify whether this string is static or needs special handling.

---

### TEST-LIB-003: Library Error State After Creation

**Purpose**: Verify initial error state is `LIB7ZIP_NO_ERROR`.

**Test Steps**:
1. Call `lib_new()`
2. Call `lib_get_last_error(lib)`
3. Verify return value equals `LIB7ZIP_NO_ERROR` (0)

**Expected Result**: Returns 0.

---

### TEST-LIB-004: Library Free with NULL

**Purpose**: Verify `lib_free(NULL)` does not crash.

**Test Steps**:
1. Call `lib_free(NULL)`

**Expected Result**: No crash, no-op behavior.

---

### TEST-LIB-005: Double Free Protection

**Purpose**: Verify double-free does not cause undefined behavior.

**Test Steps**:
1. Call `lib_new()`
2. Call `lib_free(lib)`
3. Call `lib_free(lib)` again

**Expected Result**: No crash (ideally no-op on second call, or documented behavior).

---

## 3. Input Stream Tests

### TEST-IN-001: Input Stream Creation

**Purpose**: Verify `in_stream_new()` allocates a valid input stream.

**Test Steps**:
1. Call `in_stream_new()`
2. Verify return value is non-NULL
3. Call `in_stream_free()` to clean up

**Expected Result**: Non-NULL pointer returned.

---

### TEST-IN-002: Input Stream Definition Access

**Purpose**: Verify `in_stream_get_def()` returns writable definition struct.

**Test Steps**:
1. Call `in_stream_new()`
2. Call `in_stream_get_def(is)`
3. Verify return value is non-NULL
4. Verify all fields are initially zeroed or have safe default values

**Expected Result**: Non-NULL pointer to `in_stream_def` struct.

---

### TEST-IN-003: Input Stream Definition Population

**Purpose**: Verify the definition struct can be populated with callbacks.

**Test Steps**:
1. Create input stream
2. Get definition via `in_stream_get_def()`
3. Set all fields:
   - `id` = test value (e.g., 12345)
   - `seek_cb` = test seek callback function
   - `read_cb` = test read callback function
   - `ext` = "zip"
   - `size` = 1024
4. Call `in_stream_commit_def(is)`
5. Verify no crash

**Expected Result**: All fields accepted, commit succeeds.

---

### TEST-IN-004: Input Stream Extension Handling

**Purpose**: Verify the library properly handles/copies the `ext` field.

**Test Steps**:
1. Create input stream and get definition
2. Allocate string `ext = strdup("7z")`
3. Set `def->ext = ext`
4. Call `in_stream_commit_def()`
5. Free original `ext` string
6. Use input stream in archive open operation

**Expected Result**: The library should have copied the extension string internally. Using the stream after freeing the original `ext` should work correctly.

---

### TEST-IN-005: Input Stream Free with NULL

**Purpose**: Verify `in_stream_free(NULL)` is safe.

**Test Steps**:
1. Call `in_stream_free(NULL)`

**Expected Result**: No crash.

---

### TEST-IN-006: Input Stream Various Extensions

**Purpose**: Verify common archive extensions are handled.

**Extensions to test**:
- `"zip"`
- `"7z"`
- `"rar"`
- `"tar"`
- `"gz"`
- `"bz2"`
- `"xz"`
- `""` (empty string)
- `NULL`

**Expected Result**: All extensions accepted without error.

---

## 4. Output Stream Tests

### TEST-OUT-001: Output Stream Creation

**Purpose**: Verify `out_stream_new()` allocates a valid output stream.

**Test Steps**:
1. Call `out_stream_new()`
2. Verify return value is non-NULL
3. Call `out_stream_free()` to clean up

**Expected Result**: Non-NULL pointer returned.

---

### TEST-OUT-002: Output Stream Definition Access

**Purpose**: Verify `out_stream_get_def()` returns writable definition struct.

**Test Steps**:
1. Call `out_stream_new()`
2. Call `out_stream_get_def(os)`
3. Verify return value is non-NULL

**Expected Result**: Non-NULL pointer to `out_stream_def` struct.

---

### TEST-OUT-003: Output Stream Definition Population

**Purpose**: Verify the definition struct can be populated.

**Test Steps**:
1. Create output stream
2. Get definition via `out_stream_get_def()`
3. Set fields:
   - `id` = test value
   - `write_cb` = test write callback function
4. Verify no crash

**Expected Result**: Fields accepted.

**Note**: Unlike input streams, there is no `out_stream_commit_def()` function. Verify when the library reads these values (immediately vs. lazily).

---

### TEST-OUT-004: Output Stream Free with NULL

**Purpose**: Verify `out_stream_free(NULL)` is safe.

**Test Steps**:
1. Call `out_stream_free(NULL)`

**Expected Result**: No crash.

---

## 5. Archive Operations Tests

### TEST-ARCH-001: Archive Open with Valid ZIP

**Purpose**: Verify archive can be opened from a valid ZIP file.

**Test Steps**:
1. Create `lib` instance
2. Create `in_stream` with callbacks reading from a valid ZIP file
3. Call `archive_open(lib, in_stream, 0)` (by extension)
4. Verify return value is non-NULL
5. Clean up

**Expected Result**: Non-NULL archive pointer.

---

### TEST-ARCH-002: Archive Open with Valid 7z

**Purpose**: Verify archive can be opened from a valid 7z file.

**Test Steps**:
1. Same as TEST-ARCH-001 but with 7z file and `ext = "7z"`

**Expected Result**: Non-NULL archive pointer.

---

### TEST-ARCH-003: Archive Open by Signature

**Purpose**: Verify `by_signature=1` detects format from magic bytes.

**Test Steps**:
1. Create input stream with `ext = ""` or `ext = NULL`
2. Provide callbacks that read a valid ZIP file
3. Call `archive_open(lib, in_stream, 1)`
4. Verify archive opens successfully

**Expected Result**: Archive detected and opened correctly.

---

### TEST-ARCH-004: Archive Open with Invalid Data

**Purpose**: Verify proper error handling for invalid archive data.

**Test Steps**:
1. Create input stream reading random/garbage data
2. Call `archive_open()`
3. Verify return is NULL or error is set

**Expected Result**: NULL returned or `lib_get_last_error()` returns appropriate error code.

---

### TEST-ARCH-005: Archive Open with Empty Stream

**Purpose**: Verify handling of zero-length input.

**Test Steps**:
1. Create input stream with `size = 0`
2. Read callback returns 0 bytes immediately
3. Call `archive_open()`

**Expected Result**: Graceful failure, no crash.

---

### TEST-ARCH-006: Archive Get Format

**Purpose**: Verify `archive_get_archive_format()` returns correct format.

**Test Steps**:
1. Open a ZIP archive
2. Call `archive_get_archive_format(archive)`
3. Verify returned string indicates ZIP format
4. Call `string_free()` on returned string

**Expected Result**: Non-NULL string identifying the format. String must be freed with `string_free()`.

---

### TEST-ARCH-007: Archive Item Count

**Purpose**: Verify `archive_get_item_count()` returns correct count.

**Test Steps**:
1. Create a test archive with known number of items (e.g., 5 files)
2. Open archive
3. Call `archive_get_item_count(archive)`
4. Verify count matches expected value

**Expected Result**: Returns exact number of items in archive.

---

### TEST-ARCH-008: Archive Item Count Empty Archive

**Purpose**: Verify item count for empty archive.

**Test Steps**:
1. Create/open an empty archive (0 files)
2. Call `archive_get_item_count()`

**Expected Result**: Returns 0.

---

### TEST-ARCH-009: Archive Close and Free Sequence

**Purpose**: Verify proper close/free sequence.

**Test Steps**:
1. Open archive
2. Call `archive_close(archive)`
3. Call `archive_free(archive)`

**Expected Result**: No crash, proper cleanup.

---

### TEST-ARCH-010: Archive Free Without Close

**Purpose**: Verify `archive_free()` handles unclosed archive.

**Test Steps**:
1. Open archive
2. Call `archive_free(archive)` without calling `archive_close()`

**Expected Result**: Should either work (implicit close) or be documented behavior.

---

### TEST-ARCH-011: Archive Free with NULL

**Purpose**: Verify `archive_free(NULL)` is safe.

**Test Steps**:
1. Call `archive_free(NULL)`

**Expected Result**: No crash.

---

### TEST-ARCH-012: Archive Close with NULL

**Purpose**: Verify `archive_close(NULL)` is safe.

**Test Steps**:
1. Call `archive_close(NULL)`

**Expected Result**: No crash.

---

## 6. Item Property Tests

### TEST-ITEM-001: Get Item by Index

**Purpose**: Verify `archive_get_item()` returns valid items.

**Test Steps**:
1. Open archive with known items
2. For each index 0 to `item_count - 1`:
   - Call `archive_get_item(archive, index)`
   - Verify non-NULL return
   - Call `item_free()`

**Expected Result**: All valid indices return non-NULL items.

---

### TEST-ITEM-002: Get Item Invalid Index

**Purpose**: Verify handling of out-of-bounds index.

**Test Steps**:
1. Open archive with N items
2. Call `archive_get_item(archive, N)` (one past end)
3. Call `archive_get_item(archive, -1)`
4. Call `archive_get_item(archive, INT64_MAX)`

**Expected Result**: NULL returned for invalid indices.

---

### TEST-ITEM-003: Item Archive Index

**Purpose**: Verify `item_get_archive_index()` returns correct index.

**Test Steps**:
1. Open archive
2. Get item at index 3
3. Call `item_get_archive_index(item)`
4. Verify return value equals 3

**Expected Result**: Returns the index used to retrieve the item.

---

### TEST-ITEM-004: Item String Property - Path

**Purpose**: Verify `kpidPath` property retrieval.

**Test Steps**:
1. Create archive with file named "test/file.txt"
2. Open archive, get item
3. Call `item_get_string_property(item, kpidPath, &success)`
4. Verify `success != 0`
5. Verify returned string equals "test/file.txt"
6. Call `string_free()` on returned string

**Expected Result**: Correct path returned, success flag set.

---

### TEST-ITEM-005: Item String Property - All String Properties

**Purpose**: Verify all string properties can be queried.

**Properties to test**:
| Property | Index | Description |
|----------|-------|-------------|
| `kpidUser` | 7 | User name |
| `kpidGroup` | 8 | Group name |
| `kpidComment` | 9 | Comment |
| `kpidChecksum` | 12 | Checksum |
| `kpidCharacts` | 13 | Characteristics |
| `kpidCreatorApp` | 14 | Creator application |
| `kpidVolumeName` | 18 | Volume label |
| `kpidPath` | 19 | Full path |
| `kpidSymLink` | 22 | Symlink target |

**Test Steps**:
1. For each property, call `item_get_string_property()`
2. If `success != 0`, verify string is non-NULL and call `string_free()`
3. If `success == 0`, verify graceful handling (NULL return or empty string)

**Expected Result**: All properties queryable without crash.

---

### TEST-ITEM-006: Item Uint64 Property - Size

**Purpose**: Verify `kpidSize` property retrieval.

**Test Steps**:
1. Create archive with file of known size (e.g., 12345 bytes)
2. Open archive, get item
3. Call `item_get_uint64_property(item, kpidSize, &success)`
4. Verify `success != 0`
5. Verify returned value equals 12345

**Expected Result**: Correct size returned.

---

### TEST-ITEM-007: Item Uint64 Property - All Uint64 Properties

**Purpose**: Verify all uint64 properties can be queried.

**Properties to test**:
| Property | Index | Description |
|----------|-------|-------------|
| `kpidPackSize` | 0 | Compressed size |
| `kpidAttrib` | 1 | File attributes |
| `kpidCTime` | 2 | Creation time |
| `kpidATime` | 3 | Access time |
| `kpidMTime` | 4 | Modification time |
| `kpidPhySize` | 10 | Physical size |
| `kpidHeadersSize` | 11 | Headers size |
| `kpidTotalSize` | 15 | Total size |
| `kpidFreeSpace` | 16 | Free space |
| `kpidClusterSize` | 17 | Cluster size |
| `kpidSize` | 21 | Uncompressed size |
| `kpidPosixAttrib` | 23 | POSIX mode |

**Expected Result**: All properties queryable without crash.

---

### TEST-ITEM-008: Item Bool Property - IsDir

**Purpose**: Verify `kpidIsDir` property for directories.

**Test Steps**:
1. Create archive with a directory entry
2. Open archive, get directory item
3. Call `item_get_bool_property(item, kpidIsDir, &success)`
4. Verify `success != 0`
5. Verify returned value is non-zero (true)

**Expected Result**: Returns true for directories.

---

### TEST-ITEM-009: Item Bool Property - IsDir for File

**Purpose**: Verify `kpidIsDir` property for regular files.

**Test Steps**:
1. Get item for a regular file
2. Call `item_get_bool_property(item, kpidIsDir, &success)`
3. Verify returned value is 0 (false)

**Expected Result**: Returns false for files.

---

### TEST-ITEM-010: Item Bool Property - Encrypted

**Purpose**: Verify `kpidEncrypted` property.

**Test Steps**:
1. Create encrypted archive
2. Query `kpidEncrypted` on items

**Expected Result**: Returns true for encrypted items.

---

### TEST-ITEM-011: Item Bool Property - Solid

**Purpose**: Verify `kpidSolid` property.

**Test Steps**:
1. Create solid 7z archive
2. Query `kpidSolid` on items

**Expected Result**: Returns true for solid archives.

---

### TEST-ITEM-012: Property Not Found

**Purpose**: Verify behavior when property doesn't exist.

**Test Steps**:
1. Query a property that doesn't exist for the item
2. Verify `success` is set to 0
3. Verify no crash

**Expected Result**: `success = 0`, graceful handling.

---

### TEST-ITEM-013: Item Free with NULL

**Purpose**: Verify `item_free(NULL)` is safe.

**Test Steps**:
1. Call `item_free(NULL)`

**Expected Result**: No crash.

---

### TEST-ITEM-014: String Free with NULL

**Purpose**: Verify `string_free(NULL)` is safe.

**Test Steps**:
1. Call `string_free(NULL)`

**Expected Result**: No crash.

---

## 7. Single Item Extraction Tests

### TEST-EXT-001: Extract Single Item

**Purpose**: Verify `archive_extract_item()` extracts file correctly.

**Test Steps**:
1. Create archive with file containing known content "Hello, World!"
2. Open archive, get item
3. Create output stream with write callback that captures data
4. Call `archive_extract_item(archive, item, out_stream)`
5. Verify return value indicates success (0)
6. Verify captured data equals "Hello, World!"

**Expected Result**: Correct data extracted, returns 0.

---

### TEST-EXT-002: Extract Large File

**Purpose**: Verify extraction of files larger than typical buffer sizes.

**Test Steps**:
1. Create archive with 10MB file
2. Extract and verify integrity (e.g., via checksum)

**Expected Result**: Complete file extracted correctly.

---

### TEST-EXT-003: Extract Multiple Items Sequentially

**Purpose**: Verify multiple sequential extractions work.

**Test Steps**:
1. Open archive with multiple files
2. Extract each file one at a time
3. Verify each extraction

**Expected Result**: All files extracted correctly.

---

### TEST-EXT-004: Extract Directory Item

**Purpose**: Verify behavior when extracting a directory entry.

**Test Steps**:
1. Get item that is a directory (`kpidIsDir = true`)
2. Attempt extraction

**Expected Result**: Documented behavior (may write nothing, may return special code).

---

### TEST-EXT-005: Extract with Write Callback Error

**Purpose**: Verify error propagation from write callback.

**Test Steps**:
1. Create output stream with write callback that returns error (non-zero)
2. Attempt extraction

**Expected Result**: `archive_extract_item()` returns non-zero error code.

---

### TEST-EXT-006: Extract with NULL Output Stream

**Purpose**: Verify handling of NULL output stream.

**Test Steps**:
1. Call `archive_extract_item(archive, item, NULL)`

**Expected Result**: Graceful error, no crash.

---

## 8. Batch Extraction Tests

### TEST-BATCH-001: Extract Several Items

**Purpose**: Verify `archive_extract_several()` works correctly.

**Test Steps**:
1. Create archive with 5 files
2. Create extract callback with all callbacks set
3. Create indices array `{0, 2, 4}` (extract 3 of 5 files)
4. Call `archive_extract_several(archive, indices, 3, extract_callback)`
5. Verify `get_stream_cb` called 3 times with indices 0, 2, 4
6. Verify `set_operation_result_cb` called 3 times
7. Verify extracted content is correct

**Expected Result**: Only specified items extracted.

---

### TEST-BATCH-002: Extract All Items

**Purpose**: Verify extracting all items in batch.

**Test Steps**:
1. Create indices array with all indices
2. Extract all items
3. Verify all extractions successful

**Expected Result**: All items extracted.

---

### TEST-BATCH-003: Extract Empty Indices Array

**Purpose**: Verify handling of empty extraction request.

**Test Steps**:
1. Call `archive_extract_several(archive, indices, 0, extract_callback)`

**Expected Result**: Returns success, no callbacks invoked.

---

### TEST-BATCH-004: Extract with NULL Indices

**Purpose**: Verify handling of NULL indices array.

**Test Steps**:
1. Call `archive_extract_several(archive, NULL, 0, extract_callback)`

**Expected Result**: Graceful handling (success or documented error).

---

### TEST-BATCH-005: Extract with Invalid Index in Array

**Purpose**: Verify handling of out-of-bounds index.

**Test Steps**:
1. Create indices array with invalid index (e.g., 999 in 5-item archive)
2. Attempt batch extraction

**Expected Result**: Error for invalid index, valid items may still extract.

---

## 9. Callback Behavior Tests

### TEST-CB-001: Read Callback Invocation

**Purpose**: Verify read callback is called correctly during archive operations.

**Test Steps**:
1. Create input stream with read callback that logs calls
2. Open archive
3. Verify read callback was invoked
4. Verify parameters:
   - `id` matches set value
   - `data` is non-NULL writable buffer
   - `size` is reasonable (> 0)
   - `processed_size` is non-NULL

**Expected Result**: Read callback invoked with correct parameters.

---

### TEST-CB-002: Read Callback Return Values

**Purpose**: Verify library respects read callback return values.

**Test Steps**:
1. Create read callback that returns 0 (success) and sets `*processed_size`
2. Verify library continues reading
3. Create read callback that returns non-zero (error)
4. Verify library stops and reports error

**Expected Result**: Return values respected.

---

### TEST-CB-003: Read Callback Partial Read

**Purpose**: Verify library handles partial reads correctly.

**Test Steps**:
1. Create read callback that returns fewer bytes than requested
2. Verify library continues calling until sufficient data read

**Expected Result**: Library handles partial reads.

---

### TEST-CB-004: Seek Callback Whence Values

**Purpose**: Verify seek callback receives correct whence values.

**Test Steps**:
1. Create seek callback that logs whence parameter
2. Open archive and perform operations
3. Verify whence values are:
   - 0 = SEEK_SET (absolute position)
   - 1 = SEEK_CUR (relative to current)
   - 2 = SEEK_END (relative to end)

**Expected Result**: Standard whence values used.

---

### TEST-CB-005: Seek Callback New Position

**Purpose**: Verify seek callback must set new_position correctly.

**Test Steps**:
1. Create seek callback that sets `*new_position` to actual position after seek
2. Verify library uses this value correctly

**Expected Result**: Library uses reported position.

---

### TEST-CB-006: Write Callback Invocation

**Purpose**: Verify write callback during extraction.

**Test Steps**:
1. Create output stream with write callback
2. Extract item
3. Verify write callback invoked with:
   - `id` matches set value
   - `data` contains decompressed content
   - `size` indicates data size
   - `processed_size` is non-NULL output parameter

**Expected Result**: Write callback receives correct data.

---

### TEST-CB-007: Write Callback Partial Write

**Purpose**: Verify library handles partial writes.

**Test Steps**:
1. Create write callback that writes fewer bytes than provided
2. Set `*processed_size` to actual bytes written
3. Verify library continues calling with remaining data

**Expected Result**: Library handles partial writes.

---

### TEST-CB-008: Extract Callback set_total_cb

**Purpose**: Verify `set_total_cb` is called during batch extraction.

**Test Steps**:
1. Create extract callback with `set_total_cb` that logs calls
2. Perform batch extraction
3. Verify `set_total_cb` called with total bytes to extract

**Expected Result**: Called once with total size.

---

### TEST-CB-009: Extract Callback set_completed_cb

**Purpose**: Verify `set_completed_cb` is called for progress.

**Test Steps**:
1. Create extract callback with `set_completed_cb` that logs calls
2. Extract large file
3. Verify `set_completed_cb` called periodically with increasing values

**Expected Result**: Progress reported, values increase monotonically.

---

### TEST-CB-010: Extract Callback get_stream_cb

**Purpose**: Verify `get_stream_cb` is called to get output stream for each item.

**Test Steps**:
1. Create extract callback with `get_stream_cb` that returns new output stream
2. Batch extract 3 items
3. Verify `get_stream_cb` called 3 times with correct indices

**Expected Result**: Called once per item with item's archive index.

---

### TEST-CB-011: Extract Callback get_stream_cb Returns NULL

**Purpose**: Verify behavior when `get_stream_cb` returns NULL (skip item).

**Test Steps**:
1. Create `get_stream_cb` that returns NULL for certain indices
2. Batch extract
3. Verify those items are skipped

**Expected Result**: Items with NULL stream are skipped.

---

### TEST-CB-012: Extract Callback set_operation_result_cb

**Purpose**: Verify `set_operation_result_cb` is called after each item.

**Test Steps**:
1. Create extract callback with `set_operation_result_cb` that logs calls
2. Batch extract 3 items
3. Verify callback called 3 times with result codes

**Expected Result**: Called once per item after extraction completes.

---

### TEST-CB-013: Extract Callback Operation Result Values

**Purpose**: Verify operation result values are meaningful.

**Test Steps**:
1. Extract items successfully and with errors
2. Log result values
3. Document what values indicate success vs. various errors

**Expected Result**: Result values documented and consistent.

---

## 10. Memory Management Tests

### TEST-MEM-001: No Memory Leaks - Basic Lifecycle

**Purpose**: Verify no memory leaks in basic usage.

**Test Steps** (run under Valgrind/ASan):
1. Create lib, in_stream, out_stream, archive
2. Get items, query properties
3. Extract items
4. Free everything in reverse order

**Expected Result**: No memory leaks reported.

---

### TEST-MEM-002: No Memory Leaks - String Properties

**Purpose**: Verify string property memory management.

**Test Steps**:
1. Query all string properties
2. Call `string_free()` on each returned string
3. Verify no leaks

**Expected Result**: No memory leaks.

---

### TEST-MEM-003: String Free Requirement

**Purpose**: Verify that NOT calling `string_free()` causes leaks.

**Test Steps**:
1. Query string properties without freeing
2. Verify memory leak is detected

**Expected Result**: Leak detected (confirms `string_free()` is required).

---

### TEST-MEM-004: Archive Format String Memory

**Purpose**: Verify `archive_get_archive_format()` string must be freed.

**Test Steps**:
1. Call `archive_get_archive_format()`
2. Call `string_free()` on result
3. Verify no leak

**Expected Result**: String must be freed with `string_free()`.

---

### TEST-MEM-005: Repeated Operations

**Purpose**: Verify no memory growth with repeated operations.

**Test Steps**:
1. In a loop (1000 iterations):
   - Open archive
   - Get all items and properties
   - Extract all items
   - Close and free archive
2. Monitor memory usage

**Expected Result**: Memory usage stable, no growth.

---

## 11. Error Handling Tests

### TEST-ERR-001: Error Code - No Error

**Purpose**: Verify `LIB7ZIP_NO_ERROR` (0) after successful operations.

**Test Steps**:
1. Perform successful archive open
2. Check `lib_get_last_error()`

**Expected Result**: Returns 0.

---

### TEST-ERR-002: Error Code - Not Supported Archive

**Purpose**: Verify `LIB7ZIP_NOT_SUPPORTED_ARCHIVE` for unknown formats.

**Test Steps**:
1. Attempt to open file with unknown/unsupported format
2. Check `lib_get_last_error()`

**Expected Result**: Returns `LIB7ZIP_NOT_SUPPORTED_ARCHIVE` (4).

---

### TEST-ERR-003: Error Code - Need Password

**Purpose**: Verify `LIB7ZIP_NEED_PASSWORD` for encrypted archives.

**Test Steps**:
1. Attempt to extract from password-protected archive without password
2. Check error code

**Expected Result**: Returns `LIB7ZIP_NEED_PASSWORD` (3).

---

### TEST-ERR-004: Error Code Enum Values

**Purpose**: Verify error code enum values match expected.

**Expected Values**:
```c
LIB7ZIP_NO_ERROR = 0
LIB7ZIP_UNKNOWN_ERROR = 1
LIB7ZIP_NOT_INITIALIZE = 2
LIB7ZIP_NEED_PASSWORD = 3
LIB7ZIP_NOT_SUPPORTED_ARCHIVE = 4
```

---

### TEST-ERR-005: Error State Persistence

**Purpose**: Verify error state persists until next operation.

**Test Steps**:
1. Cause an error
2. Check error code
3. Check error code again without new operation
4. Verify same error code returned

**Expected Result**: Error code persists.

---

### TEST-ERR-006: Error State Reset

**Purpose**: Verify error state resets after successful operation.

**Test Steps**:
1. Cause an error
2. Perform successful operation
3. Check error code

**Expected Result**: Error code is `LIB7ZIP_NO_ERROR`.

---

## 12. Thread Safety Tests

### TEST-THREAD-001: Concurrent Library Instances

**Purpose**: Verify multiple library instances can coexist.

**Test Steps**:
1. Create multiple `lib` instances in parallel threads
2. Each thread opens different archives
3. Verify no interference

**Expected Result**: Independent operation.

---

### TEST-THREAD-002: Concurrent Archive Access

**Purpose**: Verify concurrent access to different archives.

**Test Steps**:
1. Open multiple archives (one per thread)
2. Extract items concurrently
3. Verify correct extraction

**Expected Result**: No data corruption.

---

### TEST-THREAD-003: Shared Library Instance

**Purpose**: Document whether single `lib` instance is thread-safe.

**Test Steps**:
1. Create single `lib` instance
2. Open different archives from different threads using same lib
3. Document behavior

**Expected Result**: Documented thread-safety guarantees.

---

## 13. Integration Test Scenarios

### TEST-INT-001: Full Workflow - ZIP

**Purpose**: End-to-end test with ZIP archive.

**Test Steps**:
1. Create `lib` instance
2. Create `in_stream` reading from ZIP file
3. Open archive
4. Get item count
5. For each item:
   - Get item properties (path, size, isDir)
   - If file, extract to memory
   - Verify content
6. Close and free everything

**Expected Result**: Complete successful extraction.

---

### TEST-INT-002: Full Workflow - 7z

**Purpose**: End-to-end test with 7z archive.

**Test Steps**: Same as TEST-INT-001 with 7z file.

---

### TEST-INT-003: Full Workflow - Batch Extraction

**Purpose**: End-to-end test with batch extraction.

**Test Steps**:
1. Open archive
2. Set up extract callback
3. Extract all items via `archive_extract_several()`
4. Verify all items extracted correctly

**Expected Result**: Complete successful batch extraction.

---

### TEST-INT-004: Real-World Archives

**Purpose**: Test with various real-world archive formats.

**Archives to test**:
- ZIP with directories and files
- 7z with LZMA2 compression
- 7z solid archive
- TAR archive
- GZip compressed file
- Large archive (>1GB)
- Archive with many files (>10,000)
- Archive with Unicode filenames
- Archive with symlinks

---

### TEST-INT-005: Go Binding Simulation

**Purpose**: Simulate exact Go binding usage patterns.

**Test Steps**:
1. Implement C test that mirrors Go's `glue.go` patterns exactly
2. Use same lifecycle: `lazyInit`, stream creation, callback setup
3. Verify identical behavior

**Expected Result**: C behavior matches Go expectations.

---

## Appendix A: Property Index Reference

```c
enum property_index {
    kpidPackSize = 0,      // uint64 - Compressed size
    kpidAttrib = 1,        // uint64 - File attributes
    kpidCTime = 2,         // uint64 - Creation time (FILETIME)
    kpidATime = 3,         // uint64 - Access time
    kpidMTime = 4,         // uint64 - Modification time
    kpidSolid = 5,         // bool   - Solid archive flag
    kpidEncrypted = 6,     // bool   - Encrypted flag
    kpidUser = 7,          // string - User name
    kpidGroup = 8,         // string - Group name
    kpidComment = 9,       // string - Comment
    kpidPhySize = 10,      // uint64 - Physical size
    kpidHeadersSize = 11,  // uint64 - Headers size
    kpidChecksum = 12,     // string - Checksum
    kpidCharacts = 13,     // string - Characteristics
    kpidCreatorApp = 14,   // string - Creator application
    kpidTotalSize = 15,    // uint64 - Total size
    kpidFreeSpace = 16,    // uint64 - Free space
    kpidClusterSize = 17,  // uint64 - Cluster size
    kpidVolumeName = 18,   // string - Volume label
    kpidPath = 19,         // string - Full path in archive
    kpidIsDir = 20,        // bool   - Is directory
    kpidSize = 21,         // uint64 - Uncompressed size
    kpidSymLink = 22,      // string - Symlink target
    kpidPosixAttrib = 23,  // uint64 - POSIX mode (mode_t)
};
```

## Appendix B: Error Code Reference

```c
enum error_code {
    LIB7ZIP_NO_ERROR = 0,
    LIB7ZIP_UNKNOWN_ERROR = 1,
    LIB7ZIP_NOT_INITIALIZE = 2,
    LIB7ZIP_NEED_PASSWORD = 3,
    LIB7ZIP_NOT_SUPPORTED_ARCHIVE = 4,
};
```

## Appendix C: Callback Signatures

```c
// Input stream read: return 0 on success, non-zero on error
typedef int (*read_cb_t)(
    int64_t id,              // Stream identifier
    void *data,              // Buffer to fill
    int64_t size,            // Requested bytes
    int64_t *processed_size  // Output: actual bytes read
);

// Input stream seek: return 0 on success, non-zero on error
typedef int (*seek_cb_t)(
    int64_t id,              // Stream identifier
    int64_t offset,          // Seek offset
    int32_t whence,          // 0=SET, 1=CUR, 2=END
    int64_t *new_position    // Output: position after seek
);

// Output stream write: return 0 on success, non-zero on error
typedef int (*write_cb_t)(
    int64_t id,              // Stream identifier
    const void *data,        // Data to write
    int64_t size,            // Bytes to write
    int64_t *processed_size  // Output: actual bytes written
);

// Extract callback: set total size
typedef void (*set_total_cb_t)(
    int64_t id,              // Callback identifier
    int64_t size             // Total bytes to extract
);

// Extract callback: report progress
typedef void (*set_completed_cb_t)(
    int64_t id,              // Callback identifier
    int64_t complete_value   // Bytes completed so far
);

// Extract callback: get output stream for item
typedef out_stream *(*get_stream_cb_t)(
    int64_t id,              // Callback identifier
    int64_t index            // Item index in archive
);

// Extract callback: report extraction result
typedef void (*set_operation_result_cb_t)(
    int64_t id,              // Callback identifier
    int32_t operation_result // Result code
);
```

## Appendix D: Test Archive Requirements

Create the following test archives for comprehensive testing:

1. **test_basic.zip** - Simple ZIP with 3 text files
2. **test_dirs.zip** - ZIP with nested directory structure
3. **test_large.zip** - ZIP with 10MB+ file
4. **test_many.zip** - ZIP with 100+ files
5. **test_basic.7z** - Simple 7z with LZMA compression
6. **test_solid.7z** - Solid 7z archive
7. **test_encrypted.zip** - Password-protected ZIP
8. **test_encrypted.7z** - Password-protected 7z
9. **test_unicode.zip** - Archive with Unicode filenames
10. **test_symlinks.tar** - TAR with symbolic links
11. **test_empty.zip** - Empty archive
12. **test_corrupt.zip** - Intentionally corrupted archive

---

## Summary

Total test cases: **~100**

| Category | Count |
|----------|-------|
| Symbol Verification | 1 |
| Library Management | 5 |
| Input Stream | 6 |
| Output Stream | 4 |
| Archive Operations | 12 |
| Item Properties | 14 |
| Single Extraction | 6 |
| Batch Extraction | 5 |
| Callback Behavior | 13 |
| Memory Management | 5 |
| Error Handling | 6 |
| Thread Safety | 3 |
| Integration Tests | 5 |

**Priority Order**:
1. Symbol verification (TEST-SYM-001)
2. Basic lifecycle (TEST-LIB-*, TEST-IN-001/002, TEST-OUT-001/002)
3. Archive open/close (TEST-ARCH-001 through 005)
4. Item property access (TEST-ITEM-001 through 006)
5. Single item extraction (TEST-EXT-001)
6. Callback behavior (TEST-CB-001 through 007)
7. Batch extraction (TEST-BATCH-001)
8. Memory management (TEST-MEM-001)
9. Error handling (TEST-ERR-*)
10. Edge cases and remaining tests
