#include "test_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global for test archives directory - set by main */
static char test_archives_dir[1024] = "./archives";

void set_test_archives_dir(const char *dir) {
    strncpy(test_archives_dir, dir, sizeof(test_archives_dir) - 1);
    test_archives_dir[sizeof(test_archives_dir) - 1] = '\0';
}

const char *get_test_archives_dir(void) {
    return test_archives_dir;
}

char *get_test_archive_path(const char *filename) {
    size_t len = strlen(test_archives_dir) + strlen(filename) + 2;
    char *path = (char *)malloc(len);
    if (path) {
        snprintf(path, len, "%s/%s", test_archives_dir, filename);
    }
    return path;
}

void free_path(char *path) {
    free(path);
}

/* File input stream callbacks */
static int file_read_cb(int64_t id, void *data, int64_t size, int64_t *processed_size) {
    file_in_stream *fis = (file_in_stream *)(intptr_t)id;
    size_t read = fread(data, 1, (size_t)size, fis->fp);
    *processed_size = (int64_t)read;
    return 0;
}

static int file_seek_cb(int64_t id, int64_t offset, int32_t whence, int64_t *new_position) {
    file_in_stream *fis = (file_in_stream *)(intptr_t)id;
    int origin;
    switch (whence) {
        case 0: origin = SEEK_SET; break;
        case 1: origin = SEEK_CUR; break;
        case 2: origin = SEEK_END; break;
        default: return 1;
    }
    if (fseek(fis->fp, (long)offset, origin) != 0) {
        return 1;
    }
    *new_position = (int64_t)ftell(fis->fp);
    return 0;
}

/* Extract file extension from path */
static char *get_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return strdup("");
    }
    return strdup(dot + 1);
}

file_in_stream *create_file_in_stream(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    file_in_stream *fis = (file_in_stream *)calloc(1, sizeof(file_in_stream));
    if (!fis) {
        fclose(fp);
        return NULL;
    }

    fis->fp = fp;
    fis->ext = get_extension(path);

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    fis->size = (int64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fis->stream = in_stream_new();
    if (!fis->stream) {
        fclose(fp);
        free(fis->ext);
        free(fis);
        return NULL;
    }

    in_stream_def *def = in_stream_get_def(fis->stream);
    def->id = (int64_t)(intptr_t)fis;
    def->read_cb = file_read_cb;
    def->seek_cb = file_seek_cb;
    def->size = fis->size;
    def->ext = fis->ext;
    in_stream_commit_def(fis->stream);

    return fis;
}

void free_file_in_stream(file_in_stream *fis) {
    if (fis) {
        if (fis->stream) {
            in_stream_free(fis->stream);
        }
        if (fis->fp) {
            fclose(fis->fp);
        }
        if (fis->ext) {
            free(fis->ext);
        }
        free(fis);
    }
}

/* Memory output stream callbacks */
static int memory_write_cb(int64_t id, const void *data, int64_t size, int64_t *processed_size) {
    memory_out_stream *mos = (memory_out_stream *)(intptr_t)id;

    /* Expand buffer if needed */
    size_t needed = mos->position + (size_t)size;
    if (needed > mos->capacity) {
        size_t new_capacity = mos->capacity * 2;
        if (new_capacity < needed) {
            new_capacity = needed;
        }
        uint8_t *new_buffer = (uint8_t *)realloc(mos->buffer, new_capacity);
        if (!new_buffer) {
            *processed_size = 0;
            return 1;
        }
        mos->buffer = new_buffer;
        mos->capacity = new_capacity;
    }

    memcpy(mos->buffer + mos->position, data, (size_t)size);
    mos->position += (size_t)size;
    if (mos->position > mos->size) {
        mos->size = mos->position;
    }
    *processed_size = size;
    return 0;
}

memory_out_stream *create_memory_out_stream(size_t initial_capacity) {
    memory_out_stream *mos = (memory_out_stream *)calloc(1, sizeof(memory_out_stream));
    if (!mos) {
        return NULL;
    }

    mos->capacity = initial_capacity > 0 ? initial_capacity : 1024;
    mos->buffer = (uint8_t *)malloc(mos->capacity);
    if (!mos->buffer) {
        free(mos);
        return NULL;
    }

    mos->stream = out_stream_new();
    if (!mos->stream) {
        free(mos->buffer);
        free(mos);
        return NULL;
    }

    out_stream_def *def = out_stream_get_def(mos->stream);
    def->id = (int64_t)(intptr_t)mos;
    def->write_cb = memory_write_cb;

    return mos;
}

void free_memory_out_stream(memory_out_stream *mos) {
    if (mos) {
        if (mos->stream) {
            out_stream_free(mos->stream);
        }
        if (mos->buffer) {
            free(mos->buffer);
        }
        free(mos);
    }
}

void reset_memory_out_stream(memory_out_stream *mos) {
    if (mos) {
        mos->size = 0;
        mos->position = 0;
    }
}

/* Batch extraction callbacks */
static void batch_set_total_cb(int64_t id, int64_t size) {
    (void)id;
    (void)size;
}

static void batch_set_completed_cb(int64_t id, int64_t complete_value) {
    (void)id;
    (void)complete_value;
}

static out_stream *batch_get_stream_cb(int64_t id, int64_t index) {
    batch_extract_context *ctx = (batch_extract_context *)(intptr_t)id;

    /* Find which stream corresponds to this index */
    for (int32_t i = 0; i < ctx->num_items; i++) {
        if (ctx->indices[i] == index) {
            if (!ctx->streams[i]) {
                ctx->streams[i] = create_memory_out_stream(1024);
            }
            return ctx->streams[i] ? ctx->streams[i]->stream : NULL;
        }
    }
    return NULL;
}

static void batch_set_operation_result_cb(int64_t id, int32_t operation_result) {
    batch_extract_context *ctx = (batch_extract_context *)(intptr_t)id;
    if (operation_result == 0) {
        ctx->items_extracted++;
    } else {
        ctx->errors++;
    }
}

batch_extract_context *create_batch_extract_context(int32_t num_items, int64_t *indices) {
    batch_extract_context *ctx = (batch_extract_context *)calloc(1, sizeof(batch_extract_context));
    if (!ctx) {
        return NULL;
    }

    ctx->num_items = num_items;
    ctx->indices = (int64_t *)malloc(num_items * sizeof(int64_t));
    ctx->streams = (memory_out_stream **)calloc(num_items, sizeof(memory_out_stream *));

    if (!ctx->indices || !ctx->streams) {
        free(ctx->indices);
        free(ctx->streams);
        free(ctx);
        return NULL;
    }

    memcpy(ctx->indices, indices, num_items * sizeof(int64_t));
    return ctx;
}

void free_batch_extract_context(batch_extract_context *ctx) {
    if (ctx) {
        for (int32_t i = 0; i < ctx->num_items; i++) {
            if (ctx->streams[i]) {
                free_memory_out_stream(ctx->streams[i]);
            }
        }
        free(ctx->streams);
        free(ctx->indices);
        free(ctx);
    }
}

extract_callback *setup_batch_extract_callback(batch_extract_context *ctx) {
    extract_callback *ec = extract_callback_new();
    if (!ec) {
        return NULL;
    }

    extract_callback_def *def = extract_callback_get_def(ec);
    def->id = (int64_t)(intptr_t)ctx;
    def->set_total_cb = batch_set_total_cb;
    def->set_completed_cb = batch_set_completed_cb;
    def->get_stream_cb = batch_get_stream_cb;
    def->set_operation_result_cb = batch_set_operation_result_cb;

    return ec;
}

/* ============================================
 * Memory-backed input stream implementation
 * ============================================ */

static int memory_in_read_cb(int64_t id, void *data, int64_t size, int64_t *processed_size) {
    memory_in_stream *mis = (memory_in_stream *)(intptr_t)id;
    if (mis->read_error) {
        *processed_size = 0;
        return mis->read_error;
    }
    size_t available = mis->size - mis->position;
    size_t to_read = (size_t)size;
    if (to_read > available) {
        to_read = available;
    }
    if (to_read > 0) {
        memcpy(data, mis->data + mis->position, to_read);
        mis->position += to_read;
    }
    *processed_size = (int64_t)to_read;
    return 0;
}

static int memory_in_seek_cb(int64_t id, int64_t offset, int32_t whence, int64_t *new_position) {
    memory_in_stream *mis = (memory_in_stream *)(intptr_t)id;
    int64_t new_pos;
    switch (whence) {
        case 0: /* SEEK_SET */
            new_pos = offset;
            break;
        case 1: /* SEEK_CUR */
            new_pos = (int64_t)mis->position + offset;
            break;
        case 2: /* SEEK_END */
            new_pos = (int64_t)mis->size + offset;
            break;
        default:
            return 1;
    }
    if (new_pos < 0) {
        new_pos = 0;
    }
    if (new_pos > (int64_t)mis->size) {
        new_pos = (int64_t)mis->size;
    }
    mis->position = (size_t)new_pos;
    *new_position = new_pos;
    return 0;
}

memory_in_stream *create_memory_in_stream(const uint8_t *data, size_t size, const char *ext) {
    memory_in_stream *mis = (memory_in_stream *)calloc(1, sizeof(memory_in_stream));
    if (!mis) {
        return NULL;
    }

    if (size > 0 && data) {
        mis->data = (uint8_t *)malloc(size);
        if (!mis->data) {
            free(mis);
            return NULL;
        }
        memcpy(mis->data, data, size);
        mis->size = size;
    }

    if (ext) {
        mis->ext = strdup(ext);
    }

    mis->stream = in_stream_new();
    if (!mis->stream) {
        free(mis->data);
        free(mis->ext);
        free(mis);
        return NULL;
    }

    in_stream_def *def = in_stream_get_def(mis->stream);
    def->id = (int64_t)(intptr_t)mis;
    def->read_cb = memory_in_read_cb;
    def->seek_cb = memory_in_seek_cb;
    def->size = (int64_t)mis->size;
    def->ext = mis->ext;
    in_stream_commit_def(mis->stream);

    return mis;
}

void free_memory_in_stream(memory_in_stream *mis) {
    if (mis) {
        if (mis->stream) {
            in_stream_free(mis->stream);
        }
        free(mis->data);
        free(mis->ext);
        free(mis);
    }
}

void memory_in_stream_set_read_error(memory_in_stream *mis, int error_code) {
    if (mis) {
        mis->read_error = error_code;
    }
}

/* ============================================
 * Callback tracker implementation
 * ============================================ */

callback_tracker *create_callback_tracker(void) {
    callback_tracker *tracker = (callback_tracker *)calloc(1, sizeof(callback_tracker));
    if (!tracker) {
        return NULL;
    }
    tracker->whence_history_capacity = 16;
    tracker->whence_history = (int32_t *)malloc(tracker->whence_history_capacity * sizeof(int32_t));
    if (!tracker->whence_history) {
        free(tracker);
        return NULL;
    }
    return tracker;
}

void free_callback_tracker(callback_tracker *tracker) {
    if (tracker) {
        free(tracker->whence_history);
        free(tracker);
    }
}

void tracker_record_whence(callback_tracker *tracker, int32_t whence) {
    if (!tracker) return;
    if (tracker->whence_history_count >= tracker->whence_history_capacity) {
        int new_cap = tracker->whence_history_capacity * 2;
        int32_t *new_history = (int32_t *)realloc(tracker->whence_history, new_cap * sizeof(int32_t));
        if (!new_history) return;
        tracker->whence_history = new_history;
        tracker->whence_history_capacity = new_cap;
    }
    tracker->whence_history[tracker->whence_history_count++] = whence;
}

/* ============================================
 * Tracking file input stream implementation
 * ============================================ */

static int tracking_file_read_cb(int64_t id, void *data, int64_t size, int64_t *processed_size) {
    tracking_file_in_stream *tfis = (tracking_file_in_stream *)(intptr_t)id;
    size_t read = fread(data, 1, (size_t)size, tfis->fp);
    *processed_size = (int64_t)read;
    if (tfis->tracker) {
        tfis->tracker->read_count++;
        tfis->tracker->total_bytes_read += (int64_t)read;
    }
    return 0;
}

static int tracking_file_seek_cb(int64_t id, int64_t offset, int32_t whence, int64_t *new_position) {
    tracking_file_in_stream *tfis = (tracking_file_in_stream *)(intptr_t)id;
    if (tfis->tracker) {
        tfis->tracker->seek_count++;
        tracker_record_whence(tfis->tracker, whence);
    }
    int origin;
    switch (whence) {
        case 0: origin = SEEK_SET; break;
        case 1: origin = SEEK_CUR; break;
        case 2: origin = SEEK_END; break;
        default: return 1;
    }
    if (fseek(tfis->fp, (long)offset, origin) != 0) {
        return 1;
    }
    *new_position = (int64_t)ftell(tfis->fp);
    return 0;
}

static char *get_extension_from_path(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return strdup("");
    }
    return strdup(dot + 1);
}

tracking_file_in_stream *create_tracking_file_in_stream(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    tracking_file_in_stream *tfis = (tracking_file_in_stream *)calloc(1, sizeof(tracking_file_in_stream));
    if (!tfis) {
        fclose(fp);
        return NULL;
    }

    tfis->fp = fp;
    tfis->ext = get_extension_from_path(path);
    tfis->tracker = create_callback_tracker();

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    tfis->size = (int64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    tfis->stream = in_stream_new();
    if (!tfis->stream) {
        fclose(fp);
        free(tfis->ext);
        free_callback_tracker(tfis->tracker);
        free(tfis);
        return NULL;
    }

    in_stream_def *def = in_stream_get_def(tfis->stream);
    def->id = (int64_t)(intptr_t)tfis;
    def->read_cb = tracking_file_read_cb;
    def->seek_cb = tracking_file_seek_cb;
    def->size = tfis->size;
    def->ext = tfis->ext;
    in_stream_commit_def(tfis->stream);

    return tfis;
}

void free_tracking_file_in_stream(tracking_file_in_stream *tfis) {
    if (tfis) {
        if (tfis->stream) {
            in_stream_free(tfis->stream);
        }
        if (tfis->fp) {
            fclose(tfis->fp);
        }
        free(tfis->ext);
        free_callback_tracker(tfis->tracker);
        free(tfis);
    }
}

/* ============================================
 * Tracking output stream implementation
 * ============================================ */

static int tracking_write_cb(int64_t id, const void *data, int64_t size, int64_t *processed_size) {
    tracking_out_stream *tos = (tracking_out_stream *)(intptr_t)id;

    if (tos->write_error) {
        *processed_size = 0;
        return tos->write_error;
    }

    /* Expand buffer if needed */
    size_t needed = tos->position + (size_t)size;
    if (needed > tos->capacity) {
        size_t new_capacity = tos->capacity * 2;
        if (new_capacity < needed) {
            new_capacity = needed;
        }
        uint8_t *new_buffer = (uint8_t *)realloc(tos->buffer, new_capacity);
        if (!new_buffer) {
            *processed_size = 0;
            return 1;
        }
        tos->buffer = new_buffer;
        tos->capacity = new_capacity;
    }

    memcpy(tos->buffer + tos->position, data, (size_t)size);
    tos->position += (size_t)size;
    if (tos->position > tos->size) {
        tos->size = tos->position;
    }
    *processed_size = size;

    if (tos->tracker) {
        tos->tracker->write_count++;
        tos->tracker->total_bytes_written += size;
    }

    return 0;
}

tracking_out_stream *create_tracking_out_stream(size_t initial_capacity) {
    tracking_out_stream *tos = (tracking_out_stream *)calloc(1, sizeof(tracking_out_stream));
    if (!tos) {
        return NULL;
    }

    tos->capacity = initial_capacity > 0 ? initial_capacity : 1024;
    tos->buffer = (uint8_t *)malloc(tos->capacity);
    if (!tos->buffer) {
        free(tos);
        return NULL;
    }

    tos->tracker = create_callback_tracker();

    tos->stream = out_stream_new();
    if (!tos->stream) {
        free(tos->buffer);
        free_callback_tracker(tos->tracker);
        free(tos);
        return NULL;
    }

    out_stream_def *def = out_stream_get_def(tos->stream);
    def->id = (int64_t)(intptr_t)tos;
    def->write_cb = tracking_write_cb;

    return tos;
}

void free_tracking_out_stream(tracking_out_stream *tos) {
    if (tos) {
        if (tos->stream) {
            out_stream_free(tos->stream);
        }
        free(tos->buffer);
        free_callback_tracker(tos->tracker);
        free(tos);
    }
}

void tracking_out_stream_set_write_error(tracking_out_stream *tos, int error_code) {
    if (tos) {
        tos->write_error = error_code;
    }
}

/* ============================================
 * Extract callback tracker implementation
 * ============================================ */

extract_callback_tracker *create_extract_callback_tracker(void) {
    extract_callback_tracker *tracker = (extract_callback_tracker *)calloc(1, sizeof(extract_callback_tracker));
    if (!tracker) {
        return NULL;
    }

    tracker->get_stream_indices_capacity = 16;
    tracker->get_stream_indices = (int64_t *)malloc(tracker->get_stream_indices_capacity * sizeof(int64_t));

    tracker->operation_results_capacity = 16;
    tracker->operation_results = (int32_t *)malloc(tracker->operation_results_capacity * sizeof(int32_t));

    tracker->streams_capacity = 16;
    tracker->streams = (memory_out_stream **)calloc(tracker->streams_capacity, sizeof(memory_out_stream *));

    if (!tracker->get_stream_indices || !tracker->operation_results || !tracker->streams) {
        free(tracker->get_stream_indices);
        free(tracker->operation_results);
        free(tracker->streams);
        free(tracker);
        return NULL;
    }

    return tracker;
}

void free_extract_callback_tracker(extract_callback_tracker *tracker) {
    if (tracker) {
        for (int i = 0; i < tracker->streams_capacity; i++) {
            if (tracker->streams[i]) {
                free_memory_out_stream(tracker->streams[i]);
            }
        }
        free(tracker->streams);
        free(tracker->get_stream_indices);
        free(tracker->operation_results);
        free(tracker->skip_indices);
        free(tracker);
    }
}

void extract_tracker_add_skip_index(extract_callback_tracker *tracker, int64_t index) {
    if (!tracker) return;
    int new_count = tracker->skip_count + 1;
    int64_t *new_skip = (int64_t *)realloc(tracker->skip_indices, new_count * sizeof(int64_t));
    if (!new_skip) return;
    tracker->skip_indices = new_skip;
    tracker->skip_indices[tracker->skip_count++] = index;
}

static void tracking_set_total_cb(int64_t id, int64_t size) {
    extract_callback_tracker *tracker = (extract_callback_tracker *)(intptr_t)id;
    tracker->set_total_count++;
    tracker->last_total = size;
}

static void tracking_set_completed_cb(int64_t id, int64_t complete_value) {
    (void)id;
    (void)complete_value;
}

static out_stream *tracking_get_stream_cb(int64_t id, int64_t index) {
    extract_callback_tracker *tracker = (extract_callback_tracker *)(intptr_t)id;

    /* Record this call */
    if (tracker->get_stream_count >= tracker->get_stream_indices_capacity) {
        int new_cap = tracker->get_stream_indices_capacity * 2;
        int64_t *new_indices = (int64_t *)realloc(tracker->get_stream_indices, new_cap * sizeof(int64_t));
        if (!new_indices) return NULL;
        tracker->get_stream_indices = new_indices;
        tracker->get_stream_indices_capacity = new_cap;
    }
    tracker->get_stream_indices[tracker->get_stream_count++] = index;

    /* Check if this index should be skipped */
    for (int i = 0; i < tracker->skip_count; i++) {
        if (tracker->skip_indices[i] == index) {
            return NULL;  /* Skip this item */
        }
    }

    /* Ensure we have space for this stream */
    if (index >= tracker->streams_capacity) {
        int new_cap = (int)(index + 16);
        memory_out_stream **new_streams = (memory_out_stream **)realloc(
            tracker->streams, new_cap * sizeof(memory_out_stream *));
        if (!new_streams) return NULL;
        /* Zero out new entries */
        for (int i = tracker->streams_capacity; i < new_cap; i++) {
            new_streams[i] = NULL;
        }
        tracker->streams = new_streams;
        tracker->streams_capacity = new_cap;
    }

    /* Create stream if needed */
    if (!tracker->streams[index]) {
        tracker->streams[index] = create_memory_out_stream(1024);
    }
    return tracker->streams[index] ? tracker->streams[index]->stream : NULL;
}

static void tracking_set_operation_result_cb(int64_t id, int32_t operation_result) {
    extract_callback_tracker *tracker = (extract_callback_tracker *)(intptr_t)id;

    if (tracker->set_operation_result_count >= tracker->operation_results_capacity) {
        int new_cap = tracker->operation_results_capacity * 2;
        int32_t *new_results = (int32_t *)realloc(tracker->operation_results, new_cap * sizeof(int32_t));
        if (!new_results) return;
        tracker->operation_results = new_results;
        tracker->operation_results_capacity = new_cap;
    }
    tracker->operation_results[tracker->set_operation_result_count++] = operation_result;
}

extract_callback *setup_tracking_extract_callback(extract_callback_tracker *tracker) {
    extract_callback *ec = extract_callback_new();
    if (!ec) {
        return NULL;
    }

    extract_callback_def *def = extract_callback_get_def(ec);
    def->id = (int64_t)(intptr_t)tracker;
    def->set_total_cb = tracking_set_total_cb;
    def->set_completed_cb = tracking_set_completed_cb;
    def->get_stream_cb = tracking_get_stream_cb;
    def->set_operation_result_cb = tracking_set_operation_result_cb;

    return ec;
}
