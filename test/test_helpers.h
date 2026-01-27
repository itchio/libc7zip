#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <libc7zip.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* File-backed input stream */
typedef struct file_in_stream {
    in_stream *stream;
    FILE *fp;
    int64_t size;
    char *ext;
} file_in_stream;

file_in_stream *create_file_in_stream(const char *path);
void free_file_in_stream(file_in_stream *fis);

/* Memory-backed output stream for extraction verification */
typedef struct memory_out_stream {
    out_stream *stream;
    uint8_t *buffer;
    size_t capacity;
    size_t size;
    size_t position;
} memory_out_stream;

memory_out_stream *create_memory_out_stream(size_t initial_capacity);
void free_memory_out_stream(memory_out_stream *mos);
void reset_memory_out_stream(memory_out_stream *mos);

/* Batch extraction context */
typedef struct batch_extract_context {
    memory_out_stream **streams;
    int64_t *indices;
    int32_t num_items;
    int32_t items_extracted;
    int32_t errors;
} batch_extract_context;

batch_extract_context *create_batch_extract_context(int32_t num_items, int64_t *indices);
void free_batch_extract_context(batch_extract_context *ctx);

/* Setup extract callback for batch extraction */
extract_callback *setup_batch_extract_callback(batch_extract_context *ctx);

/* Get path to test archives directory */
const char *get_test_archives_dir(void);

/* Build full path to a test archive */
char *get_test_archive_path(const char *filename);

/* Free a path allocated by get_test_archive_path */
void free_path(char *path);

#endif /* TEST_HELPERS_H */
