#pragma once

// Data queue entry type
struct data_queue_entry {
    // Double linked list handle
    struct list_head entry_head;
    // Stored byte of data
    char data_byte;
};

// File context list entry
struct file_context {
    // Fields related to data queue	
    struct {	
        // Head of the queue (with entries of type data_queue_entry)
        struct list_head head;
        // Pointer to the tail of the queue (points to the head if the queue is empty)	
        struct list_head *ptail;
        // Size of the queue	
        size_t size;
        // Mutex to be used for blocking simultaneous queue access	
        struct mutex mtx;
    } data_queue;	
    // Double linked list handle
    struct list_head ctx_head;
};

// Initialize file context
void file_context_init(struct file_context *pfile_ctx);
// Add new file context to the list and return its pointer
struct file_context* file_context_add(struct file_context *pmain_file_ctx);
// Empty data queue of specific file context
void file_context_data_queue_clear(struct file_context *pfile_ctx);
// Remove file context from the list and free it's memory (if not static)
void file_context_remove(struct file_context *pfile_ctx);
// Read bytes from file context's data queue to kernel buffer
// Returns number of bytes actually read
size_t file_context_data_queue_read_to_buffer(struct file_context *pfile_ctx, char *kbuf, size_t length);
// Write bytes as new queue entries
// Returns number of writted bytes or negative error
ssize_t file_context_data_queue_write_from_buffer(struct file_context *pfile_ctx, char *kbuf, size_t length);

