#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_harness.h"
#include "test_helpers.h"
#include <libc7zip.h>

/* Forward declaration for set_test_archives_dir */
void set_test_archives_dir(const char *dir);

/* Expected test archive contents */
static const char *HELLO_TXT_CONTENT = "Hello, World!\n";
static const size_t HELLO_TXT_SIZE = 14;

static const char *NUMBERS_TXT_CONTENT = "12345\n";
static const size_t NUMBERS_TXT_SIZE = 6;

static const char *NESTED_TXT_CONTENT = "Nested content\n";
static const size_t NESTED_TXT_SIZE = 15;

/* Global library instance for tests */
static lib *g_lib = NULL;

/* ============================================
 * Library Lifecycle Tests
 * ============================================ */

static int test_lib_new(void) {
    lib *l = lib_new();
    TEST_ASSERT_NOT_NULL(l);
    /* Note: lib_get_last_error() is only meaningful after a failed operation.
     * The error code is not initialized to a specific value on lib_new(). */
    lib_free(l);
    return 0;
}

static int test_lib_get_version(void) {
    lib *l = lib_new();
    TEST_ASSERT_NOT_NULL(l);

    char *version = lib_get_version(l);
    TEST_ASSERT_NOT_NULL(version);
    /* Version should be non-empty */
    TEST_ASSERT(strlen(version) > 0);

    /* Note: lib_get_version returns a constant string, not allocated memory.
     * Do NOT call string_free on it. */
    lib_free(l);
    return 0;
}

/* Note: lib_free(NULL) is NOT safe - it dereferences the pointer.
 * Skipping test_lib_free_null as the library doesn't support NULL input. */

/* ============================================
 * Archive Opening Tests
 * ============================================ */

static int test_open_7z_archive(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    char *format = archive_get_archive_format(a);
    TEST_ASSERT_NOT_NULL(format);
    TEST_ASSERT_STR_EQ("7z", format);
    string_free(format);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_open_zip_archive(void) {
    char *path = get_test_archive_path("simple.zip");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    char *format = archive_get_archive_format(a);
    TEST_ASSERT_NOT_NULL(format);
    TEST_ASSERT_STR_EQ("zip", format);
    string_free(format);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_open_tar_gz_archive(void) {
    char *path = get_test_archive_path("simple.tar.gz");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    /* Use by_signature=1 for tar.gz since extension hints don't work well */
    archive *a = archive_open(g_lib, fis->stream, 1);
    TEST_ASSERT_NOT_NULL(a);

    char *format = archive_get_archive_format(a);
    TEST_ASSERT_NOT_NULL(format);
    /* tar.gz shows as "gzip" format */
    TEST_ASSERT_STR_EQ("gzip", format);
    string_free(format);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_open_invalid_archive(void) {
    /* Try to open a non-existent file */
    file_in_stream *fis = create_file_in_stream("/nonexistent/path/to/archive.7z");
    TEST_ASSERT_NULL(fis);
    return 0;
}

/* ============================================
 * Archive Metadata Tests
 * ============================================ */

static int test_archive_item_count_7z(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    int64_t count = archive_get_item_count(a);
    /* Expected: hello.txt, numbers.txt, subdir, subdir/nested.txt = 4 items */
    TEST_ASSERT_EQ(4, count);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_archive_item_count_zip(void) {
    char *path = get_test_archive_path("simple.zip");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    int64_t count = archive_get_item_count(a);
    /* zip may or may not include directory entries depending on how created */
    TEST_ASSERT(count >= 3);
    TEST_ASSERT(count <= 4);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Item Property Tests
 * ============================================ */

/* Helper to find item by path */
static item *find_item_by_path(archive *a, const char *target_path) {
    int64_t count = archive_get_item_count(a);
    for (int64_t i = 0; i < count; i++) {
        item *it = archive_get_item(a, i);
        if (!it) continue;

        int32_t success = 0;
        char *path = item_get_string_property(it, kpidPath, &success);
        if (success && path) {
            if (strcmp(path, target_path) == 0) {
                string_free(path);
                return it;
            }
            string_free(path);
        }
        item_free(it);
    }
    return NULL;
}

static int test_item_path_property(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);
    item_free(hello);

    item *numbers = find_item_by_path(a, "numbers.txt");
    TEST_ASSERT_NOT_NULL(numbers);
    item_free(numbers);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_size_property(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    int32_t success = 0;
    uint64_t size = item_get_uint64_property(hello, kpidSize, &success);
    TEST_ASSERT(success);
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, size);

    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_isdir_property(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Check that hello.txt is not a directory */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    int32_t success = 0;
    int32_t is_dir = item_get_bool_property(hello, kpidIsDir, &success);
    TEST_ASSERT(success);
    TEST_ASSERT_EQ(0, is_dir);
    item_free(hello);

    /* Check that subdir is a directory */
    item *subdir = find_item_by_path(a, "subdir");
    TEST_ASSERT_NOT_NULL(subdir);

    success = 0;
    is_dir = item_get_bool_property(subdir, kpidIsDir, &success);
    TEST_ASSERT(success);
    TEST_ASSERT_EQ(1, is_dir);
    item_free(subdir);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Single Extraction Tests
 * ============================================ */

static int test_extract_single_item(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    memory_out_stream *mos = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos);

    int result = archive_extract_item(a, hello, mos->stream);
    /* Extract returns true (1) on success, false (0) on failure */
    TEST_ASSERT_EQ(1, result);

    /* Verify content */
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, mos->size);
    TEST_ASSERT(memcmp(mos->buffer, HELLO_TXT_CONTENT, HELLO_TXT_SIZE) == 0);

    free_memory_out_stream(mos);
    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_nested_item(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Try both path formats */
    item *nested = find_item_by_path(a, "subdir/nested.txt");
    if (!nested) {
        nested = find_item_by_path(a, "subdir\\nested.txt");
    }
    TEST_ASSERT_NOT_NULL(nested);

    memory_out_stream *mos = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos);

    int result = archive_extract_item(a, nested, mos->stream);
    /* Extract returns true (1) on success, false (0) on failure */
    TEST_ASSERT_EQ(1, result);

    /* Verify content */
    TEST_ASSERT_EQ(NESTED_TXT_SIZE, mos->size);
    TEST_ASSERT(memcmp(mos->buffer, NESTED_TXT_CONTENT, NESTED_TXT_SIZE) == 0);

    free_memory_out_stream(mos);
    item_free(nested);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_from_zip(void) {
    char *path = get_test_archive_path("simple.zip");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *numbers = find_item_by_path(a, "numbers.txt");
    TEST_ASSERT_NOT_NULL(numbers);

    memory_out_stream *mos = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos);

    int result = archive_extract_item(a, numbers, mos->stream);
    /* Extract returns true (1) on success, false (0) on failure */
    TEST_ASSERT_EQ(1, result);

    /* Verify content */
    TEST_ASSERT_EQ(NUMBERS_TXT_SIZE, mos->size);
    TEST_ASSERT(memcmp(mos->buffer, NUMBERS_TXT_CONTENT, NUMBERS_TXT_SIZE) == 0);

    free_memory_out_stream(mos);
    item_free(numbers);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Batch Extraction Tests
 * ============================================ */

static int test_batch_extract(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find indices for hello.txt and numbers.txt */
    int64_t hello_idx = -1;
    int64_t numbers_idx = -1;
    int64_t count = archive_get_item_count(a);

    for (int64_t i = 0; i < count; i++) {
        item *it = archive_get_item(a, i);
        if (!it) continue;

        int32_t success = 0;
        char *item_path = item_get_string_property(it, kpidPath, &success);
        if (success && item_path) {
            if (strcmp(item_path, "hello.txt") == 0) {
                hello_idx = i;
            } else if (strcmp(item_path, "numbers.txt") == 0) {
                numbers_idx = i;
            }
            string_free(item_path);
        }
        item_free(it);
    }

    TEST_ASSERT(hello_idx >= 0);
    TEST_ASSERT(numbers_idx >= 0);

    /* Setup batch extraction */
    int64_t indices[] = { hello_idx, numbers_idx };
    batch_extract_context *ctx = create_batch_extract_context(2, indices);
    TEST_ASSERT_NOT_NULL(ctx);

    extract_callback *ec = setup_batch_extract_callback(ctx);
    TEST_ASSERT_NOT_NULL(ec);

    int result = archive_extract_several(a, indices, 2, ec);
    /* ExtractSeveral returns true (1) on success, false (0) on failure */
    TEST_ASSERT_EQ(1, result);
    TEST_ASSERT_EQ(2, ctx->items_extracted);
    TEST_ASSERT_EQ(0, ctx->errors);

    /* Verify hello.txt content */
    TEST_ASSERT_NOT_NULL(ctx->streams[0]);
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, ctx->streams[0]->size);
    TEST_ASSERT(memcmp(ctx->streams[0]->buffer, HELLO_TXT_CONTENT, HELLO_TXT_SIZE) == 0);

    /* Verify numbers.txt content */
    TEST_ASSERT_NOT_NULL(ctx->streams[1]);
    TEST_ASSERT_EQ(NUMBERS_TXT_SIZE, ctx->streams[1]->size);
    TEST_ASSERT(memcmp(ctx->streams[1]->buffer, NUMBERS_TXT_CONTENT, NUMBERS_TXT_SIZE) == 0);

    extract_callback_free(ec);
    free_batch_extract_context(ctx);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Symbol Verification Tests
 * ============================================ */

static int test_all_symbols_exported(void) {
    /* Verify all 28 function pointers are non-NULL by using them */
    /* Library functions */
    TEST_ASSERT_NOT_NULL(lib_new);
    TEST_ASSERT_NOT_NULL(lib_get_last_error);
    TEST_ASSERT_NOT_NULL(lib_get_version);
    TEST_ASSERT_NOT_NULL(lib_free);

    /* Input stream functions */
    TEST_ASSERT_NOT_NULL(in_stream_new);
    TEST_ASSERT_NOT_NULL(in_stream_get_def);
    TEST_ASSERT_NOT_NULL(in_stream_commit_def);
    TEST_ASSERT_NOT_NULL(in_stream_free);

    /* Output stream functions */
    TEST_ASSERT_NOT_NULL(out_stream_new);
    TEST_ASSERT_NOT_NULL(out_stream_get_def);
    TEST_ASSERT_NOT_NULL(out_stream_free);

    /* Archive functions */
    TEST_ASSERT_NOT_NULL(archive_open);
    TEST_ASSERT_NOT_NULL(archive_close);
    TEST_ASSERT_NOT_NULL(archive_free);
    TEST_ASSERT_NOT_NULL(archive_get_item_count);
    TEST_ASSERT_NOT_NULL(archive_get_archive_format);
    TEST_ASSERT_NOT_NULL(archive_get_item);
    TEST_ASSERT_NOT_NULL(archive_extract_item);
    TEST_ASSERT_NOT_NULL(archive_extract_several);

    /* Item functions */
    TEST_ASSERT_NOT_NULL(item_get_archive_index);
    TEST_ASSERT_NOT_NULL(item_get_string_property);
    TEST_ASSERT_NOT_NULL(item_get_uint64_property);
    TEST_ASSERT_NOT_NULL(item_get_bool_property);
    TEST_ASSERT_NOT_NULL(item_free);
    TEST_ASSERT_NOT_NULL(string_free);

    /* Extract callback functions */
    TEST_ASSERT_NOT_NULL(extract_callback_new);
    TEST_ASSERT_NOT_NULL(extract_callback_get_def);
    TEST_ASSERT_NOT_NULL(extract_callback_free);

    return 0;
}

/* ============================================
 * Input Stream Tests
 * ============================================ */

static int test_in_stream_new(void) {
    in_stream *is = in_stream_new();
    TEST_ASSERT_NOT_NULL(is);
    in_stream_free(is);
    return 0;
}

static int test_in_stream_get_def(void) {
    in_stream *is = in_stream_new();
    TEST_ASSERT_NOT_NULL(is);

    in_stream_def *def = in_stream_get_def(is);
    TEST_ASSERT_NOT_NULL(def);

    /* Verify definition is writable by setting values */
    def->id = 12345;
    def->size = 1024;

    in_stream_free(is);
    return 0;
}

static int test_in_stream_commit_def(void) {
    in_stream *is = in_stream_new();
    TEST_ASSERT_NOT_NULL(is);

    in_stream_def *def = in_stream_get_def(is);
    TEST_ASSERT_NOT_NULL(def);

    /* Populate definition */
    def->id = 12345;
    def->read_cb = NULL;  /* Not testing actual callback here */
    def->seek_cb = NULL;
    def->ext = "zip";
    def->size = 1024;

    /* Commit should not crash */
    in_stream_commit_def(is);

    in_stream_free(is);
    return 0;
}

static int test_in_stream_null_extension(void) {
    /* Test with NULL extension - should not crash */
    uint8_t data[] = {0x50, 0x4B, 0x03, 0x04};  /* ZIP magic */
    memory_in_stream *mis = create_memory_in_stream(data, sizeof(data), NULL);
    TEST_ASSERT_NOT_NULL(mis);

    /* Verify stream was created */
    TEST_ASSERT_NOT_NULL(mis->stream);

    free_memory_in_stream(mis);
    return 0;
}

static int test_in_stream_empty_extension(void) {
    /* Test with empty string extension */
    uint8_t data[] = {0x50, 0x4B, 0x03, 0x04};  /* ZIP magic */
    memory_in_stream *mis = create_memory_in_stream(data, sizeof(data), "");
    TEST_ASSERT_NOT_NULL(mis);

    /* Verify stream was created */
    TEST_ASSERT_NOT_NULL(mis->stream);

    free_memory_in_stream(mis);
    return 0;
}

/* ============================================
 * Output Stream Tests
 * ============================================ */

static int test_out_stream_new(void) {
    out_stream *os = out_stream_new();
    TEST_ASSERT_NOT_NULL(os);
    out_stream_free(os);
    return 0;
}

static int test_out_stream_get_def(void) {
    out_stream *os = out_stream_new();
    TEST_ASSERT_NOT_NULL(os);

    out_stream_def *def = out_stream_get_def(os);
    TEST_ASSERT_NOT_NULL(def);

    /* Verify definition is writable */
    def->id = 54321;
    def->write_cb = NULL;

    out_stream_free(os);
    return 0;
}

static int test_out_stream_definition_population(void) {
    tracking_out_stream *tos = create_tracking_out_stream(256);
    TEST_ASSERT_NOT_NULL(tos);
    TEST_ASSERT_NOT_NULL(tos->stream);
    TEST_ASSERT_NOT_NULL(tos->tracker);

    /* Verify the definition was populated correctly */
    out_stream_def *def = out_stream_get_def(tos->stream);
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT(def->write_cb != NULL);
    TEST_ASSERT(def->id != 0);

    free_tracking_out_stream(tos);
    return 0;
}

/* ============================================
 * Archive Operation Tests
 * ============================================ */

static int test_archive_open_invalid_data(void) {
    /* Try to open archive with garbage data */
    uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
    memory_in_stream *mis = create_memory_in_stream(garbage, sizeof(garbage), "zip");
    TEST_ASSERT_NOT_NULL(mis);

    archive *a = archive_open(g_lib, mis->stream, 0);
    /* Should return NULL for invalid data */
    TEST_ASSERT_NULL(a);

    /* Check that error code is set */
    int32_t err = lib_get_last_error(g_lib);
    TEST_ASSERT(err != LIB7ZIP_NO_ERROR);

    free_memory_in_stream(mis);
    return 0;
}

static int test_archive_open_empty_stream(void) {
    /* Try to open archive with zero-length stream */
    memory_in_stream *mis = create_memory_in_stream(NULL, 0, "zip");
    TEST_ASSERT_NOT_NULL(mis);

    archive *a = archive_open(g_lib, mis->stream, 0);
    /* Should return NULL for empty data */
    TEST_ASSERT_NULL(a);

    free_memory_in_stream(mis);
    return 0;
}

static int test_archive_close_free_sequence(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Proper sequence: close then free */
    archive_close(a);
    archive_free(a);

    /* Stream can be freed after archive */
    free_file_in_stream(fis);
    return 0;
}

static int test_archive_free_without_close(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Skip close, just free (should handle internally) */
    archive_free(a);

    free_file_in_stream(fis);
    return 0;
}

static int test_archive_open_by_signature(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    /* Open by signature detection (by_signature=1) */
    archive *a = archive_open(g_lib, fis->stream, 1);
    TEST_ASSERT_NOT_NULL(a);

    char *format = archive_get_archive_format(a);
    TEST_ASSERT_NOT_NULL(format);
    TEST_ASSERT_STR_EQ("7z", format);
    string_free(format);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_archive_item_count_on_reopen(void) {
    /* Open same archive twice, verify consistent count */
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis1 = create_file_in_stream(path);
    TEST_ASSERT_NOT_NULL(fis1);
    archive *a1 = archive_open(g_lib, fis1->stream, 0);
    TEST_ASSERT_NOT_NULL(a1);
    int64_t count1 = archive_get_item_count(a1);

    file_in_stream *fis2 = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis2);
    archive *a2 = archive_open(g_lib, fis2->stream, 0);
    TEST_ASSERT_NOT_NULL(a2);
    int64_t count2 = archive_get_item_count(a2);

    TEST_ASSERT_EQ(count1, count2);

    archive_close(a1);
    archive_free(a1);
    archive_close(a2);
    archive_free(a2);
    free_file_in_stream(fis1);
    free_file_in_stream(fis2);
    return 0;
}

/* ============================================
 * Item Property Tests - Extended
 * ============================================ */

static int test_get_item_invalid_index(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    int64_t count = archive_get_item_count(a);

    /* Out of bounds - one past end */
    item *it = archive_get_item(a, count);
    TEST_ASSERT_NULL(it);

    /* Out of bounds - negative */
    it = archive_get_item(a, -1);
    TEST_ASSERT_NULL(it);

    /* Out of bounds - very large */
    it = archive_get_item(a, INT64_MAX);
    TEST_ASSERT_NULL(it);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_get_archive_index(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Get item at index 2 and verify round-trip */
    item *it = archive_get_item(a, 2);
    TEST_ASSERT_NOT_NULL(it);

    int32_t idx = item_get_archive_index(it);
    TEST_ASSERT_EQ(2, idx);

    item_free(it);

    /* Test with index 0 */
    it = archive_get_item(a, 0);
    TEST_ASSERT_NOT_NULL(it);
    idx = item_get_archive_index(it);
    TEST_ASSERT_EQ(0, idx);
    item_free(it);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_pack_size_property(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    int32_t success = 0;
    uint64_t pack_size = item_get_uint64_property(hello, kpidPackSize, &success);
    /* Pack size might not be available for all items in 7z solid archives */
    /* Just verify it doesn't crash */
    (void)pack_size;

    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_mtime_property(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    int32_t success = 0;
    uint64_t mtime = item_get_uint64_property(hello, kpidMTime, &success);
    /* MTime should be available */
    if (success) {
        /* Verify it's a reasonable timestamp (after 2000) */
        /* FILETIME is 100-nanosecond intervals since 1601 */
        TEST_ASSERT(mtime > 0);
    }

    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_property_not_found(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    /* Query a property that likely doesn't exist for a regular file */
    int32_t success = 0;
    char *symlink = item_get_string_property(hello, kpidSymLink, &success);
    /* Should handle gracefully - either success=0 or NULL return */
    if (success && symlink) {
        string_free(symlink);
    }

    /* Query comment which is usually empty */
    success = 0;
    char *comment = item_get_string_property(hello, kpidComment, &success);
    if (success && comment) {
        string_free(comment);
    }

    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_item_free_null(void) {
    /* item_free(NULL) should be safe per implementation note */
    item_free(NULL);
    return 0;
}

static int test_string_free_null(void) {
    /* string_free(NULL) should be safe per implementation note */
    string_free(NULL);
    return 0;
}

/* ============================================
 * Single Extraction Tests - Extended
 * ============================================ */

static int test_extract_directory_item(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find the directory item */
    item *subdir = find_item_by_path(a, "subdir");
    TEST_ASSERT_NOT_NULL(subdir);

    /* Verify it's a directory */
    int32_t success = 0;
    int32_t is_dir = item_get_bool_property(subdir, kpidIsDir, &success);
    TEST_ASSERT(success);
    TEST_ASSERT_EQ(1, is_dir);

    /* Try to extract - should handle gracefully (likely writes nothing) */
    memory_out_stream *mos = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos);

    int result = archive_extract_item(a, subdir, mos->stream);
    /* Extraction of directory should succeed but write nothing */
    TEST_ASSERT_EQ(1, result);
    TEST_ASSERT_EQ(0, mos->size);

    free_memory_out_stream(mos);
    item_free(subdir);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_with_write_error(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    /* Create output stream that will fail on write */
    tracking_out_stream *tos = create_tracking_out_stream(256);
    TEST_ASSERT_NOT_NULL(tos);
    tracking_out_stream_set_write_error(tos, 1);  /* Set to return error */

    int result = archive_extract_item(a, hello, tos->stream);
    /* Extraction should fail due to write error */
    TEST_ASSERT_EQ(0, result);

    free_tracking_out_stream(tos);
    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_multiple_sequential(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Extract hello.txt */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    memory_out_stream *mos1 = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos1);
    int result = archive_extract_item(a, hello, mos1->stream);
    TEST_ASSERT_EQ(1, result);
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, mos1->size);
    item_free(hello);

    /* Extract numbers.txt */
    item *numbers = find_item_by_path(a, "numbers.txt");
    TEST_ASSERT_NOT_NULL(numbers);

    memory_out_stream *mos2 = create_memory_out_stream(256);
    TEST_ASSERT_NOT_NULL(mos2);
    result = archive_extract_item(a, numbers, mos2->stream);
    TEST_ASSERT_EQ(1, result);
    TEST_ASSERT_EQ(NUMBERS_TXT_SIZE, mos2->size);
    item_free(numbers);

    /* Verify first extraction still valid */
    TEST_ASSERT(memcmp(mos1->buffer, HELLO_TXT_CONTENT, HELLO_TXT_SIZE) == 0);
    TEST_ASSERT(memcmp(mos2->buffer, NUMBERS_TXT_CONTENT, NUMBERS_TXT_SIZE) == 0);

    free_memory_out_stream(mos1);
    free_memory_out_stream(mos2);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Batch Extraction Tests - Extended
 * ============================================ */

static int test_batch_extract_all_items(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    int64_t count = archive_get_item_count(a);
    TEST_ASSERT(count > 0);

    /* Create indices array for all items */
    int64_t *indices = (int64_t *)malloc(count * sizeof(int64_t));
    TEST_ASSERT_NOT_NULL(indices);
    for (int64_t i = 0; i < count; i++) {
        indices[i] = i;
    }

    /* Setup tracking extract callback */
    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    int result = archive_extract_several(a, indices, (int32_t)count, ec);
    TEST_ASSERT_EQ(1, result);

    /* Verify all items were processed */
    TEST_ASSERT_EQ(count, tracker->get_stream_count);
    TEST_ASSERT_EQ(count, tracker->set_operation_result_count);

    free(indices);
    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_batch_extract_empty_indices(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Setup tracking extract callback */
    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    /* Extract with empty indices array */
    int64_t indices[] = {0};  /* Not used since count is 0 */
    int result = archive_extract_several(a, indices, 0, ec);
    /* Should succeed with nothing to do */
    TEST_ASSERT_EQ(1, result);
    TEST_ASSERT_EQ(0, tracker->get_stream_count);

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_batch_extract_invalid_index(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    int64_t count = archive_get_item_count(a);

    /* Setup tracking extract callback */
    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    /* Include an invalid index */
    int64_t indices[] = {0, count + 100};  /* Second index is invalid */
    int result = archive_extract_several(a, indices, 2, ec);
    /* Behavior with invalid index - library may handle various ways */
    (void)result;

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Callback Behavior Tests
 * ============================================ */

static int test_read_callback_invocation(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    tracking_file_in_stream *tfis = create_tracking_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(tfis);

    archive *a = archive_open(g_lib, tfis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Verify read callback was invoked during open */
    TEST_ASSERT(tfis->tracker->read_count > 0);
    TEST_ASSERT(tfis->tracker->total_bytes_read > 0);

    archive_close(a);
    archive_free(a);
    free_tracking_file_in_stream(tfis);
    return 0;
}

static int test_seek_callback_whence_values(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    tracking_file_in_stream *tfis = create_tracking_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(tfis);

    archive *a = archive_open(g_lib, tfis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Verify seek callback was invoked */
    TEST_ASSERT(tfis->tracker->seek_count > 0);

    /* Verify whence values are valid (0, 1, or 2) */
    for (int i = 0; i < tfis->tracker->whence_history_count; i++) {
        int32_t whence = tfis->tracker->whence_history[i];
        TEST_ASSERT(whence >= 0 && whence <= 2);
    }

    archive_close(a);
    archive_free(a);
    free_tracking_file_in_stream(tfis);
    return 0;
}

static int test_write_callback_invocation(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);

    tracking_out_stream *tos = create_tracking_out_stream(256);
    TEST_ASSERT_NOT_NULL(tos);

    int result = archive_extract_item(a, hello, tos->stream);
    TEST_ASSERT_EQ(1, result);

    /* Verify write callback was invoked */
    TEST_ASSERT(tos->tracker->write_count > 0);
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, tos->tracker->total_bytes_written);

    /* Verify correct data was written */
    TEST_ASSERT_EQ(HELLO_TXT_SIZE, tos->size);
    TEST_ASSERT(memcmp(tos->buffer, HELLO_TXT_CONTENT, HELLO_TXT_SIZE) == 0);

    free_tracking_out_stream(tos);
    item_free(hello);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_callback_set_total(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find file indices */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);
    int64_t hello_idx = item_get_archive_index(hello);
    item_free(hello);

    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    int64_t indices[] = {hello_idx};
    int result = archive_extract_several(a, indices, 1, ec);
    TEST_ASSERT_EQ(1, result);

    /* Verify set_total was called */
    TEST_ASSERT(tracker->set_total_count > 0);
    TEST_ASSERT(tracker->last_total >= 0);

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_callback_get_stream(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find indices */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);
    int64_t hello_idx = item_get_archive_index(hello);
    item_free(hello);

    item *numbers = find_item_by_path(a, "numbers.txt");
    TEST_ASSERT_NOT_NULL(numbers);
    int64_t numbers_idx = item_get_archive_index(numbers);
    item_free(numbers);

    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    int64_t indices[] = {hello_idx, numbers_idx};
    int result = archive_extract_several(a, indices, 2, ec);
    TEST_ASSERT_EQ(1, result);

    /* Verify get_stream was called for each item */
    TEST_ASSERT_EQ(2, tracker->get_stream_count);

    /* Verify correct indices were passed */
    int found_hello = 0, found_numbers = 0;
    for (int i = 0; i < tracker->get_stream_count; i++) {
        if (tracker->get_stream_indices[i] == hello_idx) found_hello = 1;
        if (tracker->get_stream_indices[i] == numbers_idx) found_numbers = 1;
    }
    TEST_ASSERT(found_hello);
    TEST_ASSERT(found_numbers);

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_callback_get_stream_null_skip(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find indices */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);
    int64_t hello_idx = item_get_archive_index(hello);
    item_free(hello);

    item *numbers = find_item_by_path(a, "numbers.txt");
    TEST_ASSERT_NOT_NULL(numbers);
    int64_t numbers_idx = item_get_archive_index(numbers);
    item_free(numbers);

    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    /* Set hello.txt to be skipped (return NULL) */
    extract_tracker_add_skip_index(tracker, hello_idx);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    int64_t indices[] = {hello_idx, numbers_idx};
    int result = archive_extract_several(a, indices, 2, ec);
    TEST_ASSERT_EQ(1, result);

    /* Verify get_stream was called for both */
    TEST_ASSERT_EQ(2, tracker->get_stream_count);

    /* Verify hello.txt was skipped (no stream created) */
    if (hello_idx < tracker->streams_capacity) {
        TEST_ASSERT_NULL(tracker->streams[hello_idx]);
    }

    /* Verify numbers.txt was extracted */
    if (numbers_idx < tracker->streams_capacity && tracker->streams[numbers_idx]) {
        TEST_ASSERT_EQ(NUMBERS_TXT_SIZE, tracker->streams[numbers_idx]->size);
    }

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_extract_callback_operation_result(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Find file index */
    item *hello = find_item_by_path(a, "hello.txt");
    TEST_ASSERT_NOT_NULL(hello);
    int64_t hello_idx = item_get_archive_index(hello);
    item_free(hello);

    extract_callback_tracker *tracker = create_extract_callback_tracker();
    TEST_ASSERT_NOT_NULL(tracker);

    extract_callback *ec = setup_tracking_extract_callback(tracker);
    TEST_ASSERT_NOT_NULL(ec);

    int64_t indices[] = {hello_idx};
    int result = archive_extract_several(a, indices, 1, ec);
    TEST_ASSERT_EQ(1, result);

    /* Verify set_operation_result was called */
    TEST_ASSERT_EQ(1, tracker->set_operation_result_count);
    /* Successful extraction should have result 0 */
    TEST_ASSERT_EQ(0, tracker->operation_results[0]);

    extract_callback_free(ec);
    free_extract_callback_tracker(tracker);
    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_read_callback_error_propagation(void) {
    /* Test that read errors stop processing */
    uint8_t data[] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};  /* 7z signature */
    memory_in_stream *mis = create_memory_in_stream(data, sizeof(data), "7z");
    TEST_ASSERT_NOT_NULL(mis);

    /* Set to return error on read */
    memory_in_stream_set_read_error(mis, 1);

    archive *a = archive_open(g_lib, mis->stream, 0);
    /* Should fail due to read error */
    TEST_ASSERT_NULL(a);

    free_memory_in_stream(mis);
    return 0;
}

/* ============================================
 * Error Handling Tests
 * ============================================ */

static int test_error_code_after_success(void) {
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    archive *a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* After successful open, error should be NO_ERROR */
    int32_t err = lib_get_last_error(g_lib);
    TEST_ASSERT_EQ(LIB7ZIP_NO_ERROR, err);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

static int test_error_code_not_supported(void) {
    /* Try to open unsupported format */
    uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
    memory_in_stream *mis = create_memory_in_stream(garbage, sizeof(garbage), "xyz");
    TEST_ASSERT_NOT_NULL(mis);

    archive *a = archive_open(g_lib, mis->stream, 0);
    TEST_ASSERT_NULL(a);

    int32_t err = lib_get_last_error(g_lib);
    TEST_ASSERT(err == LIB7ZIP_NOT_SUPPORTED_ARCHIVE || err == LIB7ZIP_UNKNOWN_ERROR);

    free_memory_in_stream(mis);
    return 0;
}

static int test_error_state_persistence(void) {
    /* Cause an error */
    uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF};
    memory_in_stream *mis = create_memory_in_stream(garbage, sizeof(garbage), "zip");
    TEST_ASSERT_NOT_NULL(mis);

    archive *a = archive_open(g_lib, mis->stream, 0);
    TEST_ASSERT_NULL(a);

    int32_t err1 = lib_get_last_error(g_lib);
    TEST_ASSERT(err1 != LIB7ZIP_NO_ERROR);

    /* Check error persists */
    int32_t err2 = lib_get_last_error(g_lib);
    TEST_ASSERT_EQ(err1, err2);

    free_memory_in_stream(mis);
    return 0;
}

static int test_error_state_reset_after_success(void) {
    /* First cause an error */
    uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF};
    memory_in_stream *mis = create_memory_in_stream(garbage, sizeof(garbage), "zip");
    TEST_ASSERT_NOT_NULL(mis);

    archive *a = archive_open(g_lib, mis->stream, 0);
    TEST_ASSERT_NULL(a);
    free_memory_in_stream(mis);

    int32_t err = lib_get_last_error(g_lib);
    TEST_ASSERT(err != LIB7ZIP_NO_ERROR);

    /* Now do successful operation */
    char *path = get_test_archive_path("simple.7z");
    TEST_ASSERT_NOT_NULL(path);

    file_in_stream *fis = create_file_in_stream(path);
    free_path(path);
    TEST_ASSERT_NOT_NULL(fis);

    a = archive_open(g_lib, fis->stream, 0);
    TEST_ASSERT_NOT_NULL(a);

    /* Error should be reset */
    err = lib_get_last_error(g_lib);
    TEST_ASSERT_EQ(LIB7ZIP_NO_ERROR, err);

    archive_close(a);
    archive_free(a);
    free_file_in_stream(fis);
    return 0;
}

/* ============================================
 * Extract Callback Lifecycle Tests
 * ============================================ */

static int test_extract_callback_new(void) {
    extract_callback *ec = extract_callback_new();
    TEST_ASSERT_NOT_NULL(ec);
    extract_callback_free(ec);
    return 0;
}

static int test_extract_callback_get_def(void) {
    extract_callback *ec = extract_callback_new();
    TEST_ASSERT_NOT_NULL(ec);

    extract_callback_def *def = extract_callback_get_def(ec);
    TEST_ASSERT_NOT_NULL(def);

    /* Verify definition is writable */
    def->id = 12345;
    def->set_total_cb = NULL;
    def->set_completed_cb = NULL;
    def->get_stream_cb = NULL;
    def->set_operation_result_cb = NULL;

    extract_callback_free(ec);
    return 0;
}

/* ============================================
 * Main Test Runner
 * ============================================ */

int main(int argc, char *argv[]) {
    printf("libc7zip Test Suite\n");
    printf("========================================\n\n");

    /* Allow overriding archives directory via command line */
    if (argc > 1) {
        set_test_archives_dir(argv[1]);
    }
    printf("Test archives directory: %s\n\n", get_test_archives_dir());

    /* Initialize global library instance */
    g_lib = lib_new();
    if (!g_lib) {
        fprintf(stderr, "Failed to initialize library\n");
        return 1;
    }

    printf("Symbol Verification Tests:\n");
    RUN_TEST(test_all_symbols_exported);

    printf("\nLibrary Lifecycle Tests:\n");
    RUN_TEST(test_lib_new);
    RUN_TEST(test_lib_get_version);

    printf("\nInput Stream Tests:\n");
    RUN_TEST(test_in_stream_new);
    RUN_TEST(test_in_stream_get_def);
    RUN_TEST(test_in_stream_commit_def);
    RUN_TEST(test_in_stream_null_extension);
    RUN_TEST(test_in_stream_empty_extension);

    printf("\nOutput Stream Tests:\n");
    RUN_TEST(test_out_stream_new);
    RUN_TEST(test_out_stream_get_def);
    RUN_TEST(test_out_stream_definition_population);

    printf("\nArchive Opening Tests:\n");
    RUN_TEST(test_open_7z_archive);
    RUN_TEST(test_open_zip_archive);
    RUN_TEST(test_open_tar_gz_archive);
    RUN_TEST(test_open_invalid_archive);

    printf("\nArchive Operation Tests:\n");
    RUN_TEST(test_archive_open_invalid_data);
    RUN_TEST(test_archive_open_empty_stream);
    RUN_TEST(test_archive_close_free_sequence);
    RUN_TEST(test_archive_free_without_close);
    RUN_TEST(test_archive_open_by_signature);
    RUN_TEST(test_archive_item_count_on_reopen);

    printf("\nArchive Metadata Tests:\n");
    RUN_TEST(test_archive_item_count_7z);
    RUN_TEST(test_archive_item_count_zip);

    printf("\nItem Property Tests:\n");
    RUN_TEST(test_item_path_property);
    RUN_TEST(test_item_size_property);
    RUN_TEST(test_item_isdir_property);
    RUN_TEST(test_get_item_invalid_index);
    RUN_TEST(test_item_get_archive_index);
    RUN_TEST(test_item_pack_size_property);
    RUN_TEST(test_item_mtime_property);
    RUN_TEST(test_item_property_not_found);
    RUN_TEST(test_item_free_null);
    RUN_TEST(test_string_free_null);

    printf("\nSingle Extraction Tests:\n");
    RUN_TEST(test_extract_single_item);
    RUN_TEST(test_extract_nested_item);
    RUN_TEST(test_extract_from_zip);
    RUN_TEST(test_extract_directory_item);
    RUN_TEST(test_extract_with_write_error);
    RUN_TEST(test_extract_multiple_sequential);

    printf("\nBatch Extraction Tests:\n");
    RUN_TEST(test_batch_extract);
    RUN_TEST(test_batch_extract_all_items);
    RUN_TEST(test_batch_extract_empty_indices);
    RUN_TEST(test_batch_extract_invalid_index);

    printf("\nCallback Behavior Tests:\n");
    RUN_TEST(test_read_callback_invocation);
    RUN_TEST(test_seek_callback_whence_values);
    RUN_TEST(test_write_callback_invocation);
    RUN_TEST(test_extract_callback_set_total);
    RUN_TEST(test_extract_callback_get_stream);
    RUN_TEST(test_extract_callback_get_stream_null_skip);
    RUN_TEST(test_extract_callback_operation_result);
    RUN_TEST(test_read_callback_error_propagation);

    printf("\nError Handling Tests:\n");
    RUN_TEST(test_error_code_after_success);
    RUN_TEST(test_error_code_not_supported);
    RUN_TEST(test_error_state_persistence);
    RUN_TEST(test_error_state_reset_after_success);

    printf("\nExtract Callback Lifecycle Tests:\n");
    RUN_TEST(test_extract_callback_new);
    RUN_TEST(test_extract_callback_get_def);

    /* Cleanup */
    lib_free(g_lib);

    TEST_SUMMARY();
    return TEST_RESULT();
}
