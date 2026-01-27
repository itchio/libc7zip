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

    printf("Library Lifecycle Tests:\n");
    RUN_TEST(test_lib_new);
    RUN_TEST(test_lib_get_version);

    printf("\nArchive Opening Tests:\n");
    RUN_TEST(test_open_7z_archive);
    RUN_TEST(test_open_zip_archive);
    RUN_TEST(test_open_tar_gz_archive);
    RUN_TEST(test_open_invalid_archive);

    printf("\nArchive Metadata Tests:\n");
    RUN_TEST(test_archive_item_count_7z);
    RUN_TEST(test_archive_item_count_zip);

    printf("\nItem Property Tests:\n");
    RUN_TEST(test_item_path_property);
    RUN_TEST(test_item_size_property);
    RUN_TEST(test_item_isdir_property);

    printf("\nSingle Extraction Tests:\n");
    RUN_TEST(test_extract_single_item);
    RUN_TEST(test_extract_nested_item);
    RUN_TEST(test_extract_from_zip);

    printf("\nBatch Extraction Tests:\n");
    RUN_TEST(test_batch_extract);

    /* Cleanup */
    lib_free(g_lib);

    TEST_SUMMARY();
    return TEST_RESULT();
}
