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

/* Memory-backed input stream for testing invalid/custom data */
typedef struct memory_in_stream {
    in_stream *stream;
    uint8_t *data;
    size_t size;
    size_t position;
    char *ext;
    int read_error;  /* If non-zero, read callback returns this error */
} memory_in_stream;

memory_in_stream *create_memory_in_stream(const uint8_t *data, size_t size, const char *ext);
void free_memory_in_stream(memory_in_stream *mis);
void memory_in_stream_set_read_error(memory_in_stream *mis, int error_code);

/* Callback tracking for verification */
typedef struct callback_tracker {
    int read_count;
    int seek_count;
    int write_count;
    int64_t total_bytes_read;
    int64_t total_bytes_written;
    int32_t *whence_history;
    int whence_history_count;
    int whence_history_capacity;
} callback_tracker;

callback_tracker *create_callback_tracker(void);
void free_callback_tracker(callback_tracker *tracker);
void tracker_record_whence(callback_tracker *tracker, int32_t whence);

/* Tracking variants of streams */
typedef struct tracking_file_in_stream {
    in_stream *stream;
    FILE *fp;
    int64_t size;
    char *ext;
    callback_tracker *tracker;
} tracking_file_in_stream;

tracking_file_in_stream *create_tracking_file_in_stream(const char *path);
void free_tracking_file_in_stream(tracking_file_in_stream *tfis);

typedef struct tracking_out_stream {
    out_stream *stream;
    uint8_t *buffer;
    size_t capacity;
    size_t size;
    size_t position;
    callback_tracker *tracker;
    int write_error;  /* If non-zero, write callback returns this error */
} tracking_out_stream;

tracking_out_stream *create_tracking_out_stream(size_t initial_capacity);
void free_tracking_out_stream(tracking_out_stream *tos);
void tracking_out_stream_set_write_error(tracking_out_stream *tos, int error_code);

/* Extract callback tracker for batch extraction verification */
typedef struct extract_callback_tracker {
    int set_total_count;
    int64_t last_total;
    int get_stream_count;
    int64_t *get_stream_indices;
    int get_stream_indices_capacity;
    int set_operation_result_count;
    int32_t *operation_results;
    int operation_results_capacity;
    int64_t *skip_indices;  /* Indices to skip (return NULL from get_stream_cb) */
    int skip_count;
    memory_out_stream **streams;  /* Streams for each item */
    int streams_capacity;
} extract_callback_tracker;

extract_callback_tracker *create_extract_callback_tracker(void);
void free_extract_callback_tracker(extract_callback_tracker *tracker);
void extract_tracker_add_skip_index(extract_callback_tracker *tracker, int64_t index);
extract_callback *setup_tracking_extract_callback(extract_callback_tracker *tracker);

/* Get path to test archives directory */
const char *get_test_archives_dir(void);

/* Build full path to a test archive */
char *get_test_archive_path(const char *filename);

/* Free a path allocated by get_test_archive_path */
void free_path(char *path);

#endif /* TEST_HELPERS_H */
