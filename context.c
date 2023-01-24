#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#include "context.h"

// Initialize file context
void file_context_init(struct file_context *pfile_ctx)
{
    INIT_LIST_HEAD(&pfile_ctx->data_queue.head);
    pfile_ctx->data_queue.ptail = &pfile_ctx->data_queue.head;
    pfile_ctx->data_queue.size = 0;
    mutex_init(&pfile_ctx->data_queue.mtx);
    INIT_LIST_HEAD(&pfile_ctx->ctx_head);
}

// Add new file context to the list and return its pointer
struct file_context* file_context_add(struct file_context *pmain_file_ctx)
{
    struct file_context *pnew_file_ctx;

    pnew_file_ctx = kzalloc(sizeof(struct file_context), GFP_KERNEL);
    if (pnew_file_ctx == NULL)
        return ERR_PTR(-ENOMEM);

    file_context_init(pnew_file_ctx);

    list_add_tail(&pnew_file_ctx->ctx_head, &pmain_file_ctx->ctx_head);
    
    return pnew_file_ctx;
}

// Empty data queue of specific file context
void file_context_data_queue_clear(struct file_context *pfile_ctx)
{
    struct data_queue_entry *pentry, *ptentry;
    size_t count = 0;
    // Clear data queue
    list_for_each_entry_safe(pentry, ptentry, &pfile_ctx->data_queue.head, entry_head) {
        list_del(&pentry->entry_head);
        kfree(pentry);
	    count++;
    }
    pfile_ctx->data_queue.size = 0;
    //pr_info("Cleared %ld entries\n", count);
}

// Remove file context from the list and free it's memory (if not static)
void file_context_remove(struct file_context *pfile_ctx)
{
    file_context_data_queue_clear(pfile_ctx);
    // If this is the static list head, leave it alone
    if (list_empty(&pfile_ctx->ctx_head))
        return;
    list_del(&pfile_ctx->ctx_head);
    kfree(pfile_ctx);
}

// Read bytes from file context's data queue to kernel buffer
// Returns number of bytes actually read
size_t file_context_data_queue_read_to_buffer(struct file_context *pfile_ctx, char *kbuf, size_t length)
{
    size_t buf_len;
    struct data_queue_entry *pentry;

    //pr_info("Sending data: ");
    for (buf_len = 0; buf_len < length && !list_empty(pfile_ctx->data_queue.ptail); ++buf_len) {
        pentry = list_entry(pfile_ctx->data_queue.ptail, struct data_queue_entry, entry_head);
        kbuf[buf_len] = pentry->data_byte;
        pfile_ctx->data_queue.ptail = pfile_ctx->data_queue.ptail->next;
        list_del(&pentry->entry_head);
	    pfile_ctx->data_queue.size--;
        kfree(pentry);
        //pr_cont("%c (0x%02X) ", kbuf[buf_len], kbuf[buf_len]);
    }
    //pr_info("");
    return buf_len;
}

// Write bytes as new queue entries
// Returns number of written bytes or negative error
ssize_t file_context_data_queue_write_from_buffer(struct file_context *pfile_ctx, char *kbuf, size_t length)
{
    size_t buf_len;
    struct data_queue_entry *pnew_entry;

    //pr_info("Incoming data: ");
    for (buf_len = 0; buf_len < length; ++buf_len) {
        pnew_entry = kzalloc(sizeof(struct data_queue_entry), GFP_KERNEL);
        if (pnew_entry == NULL) {
            kfree(kbuf);
            return -ENOMEM;
        }
        pnew_entry->data_byte = kbuf[buf_len];
	    INIT_LIST_HEAD(&pnew_entry->entry_head);
	    if (list_empty(&pfile_ctx->data_queue.head))
            pfile_ctx->data_queue.ptail = &pnew_entry->entry_head;
	    list_add_tail(&pnew_entry->entry_head, &pfile_ctx->data_queue.head);
	    pfile_ctx->data_queue.size++;
	    //pr_cont("%c (0x%02X) ", kbuf[buf_len], kbuf[buf_len]);
    }
    //pr_info("");

    return buf_len;
}

