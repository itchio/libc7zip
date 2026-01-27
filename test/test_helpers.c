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
