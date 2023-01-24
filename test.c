#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define RETURN_ON_ERROR(expr)           {int res; res = expr; if (res < 0) return res;}
#define RETURN_ON_ERROR_THREAD(expr)    {int res; res = expr; if (res < 0) pthread_exit((void*)(long)res);}

#define DEVICE_BUF_SIZE             1000

#define TEST_STRING                 "123"
#define TEST_SIZE                   (sizeof TEST_STRING)
#define SHARED_FILES_COUNT          3
#define SHARED_BUF_SIZE             TEST_SIZE
#define MULTI_FILES_MINIMUM_COUNT   2
#define MULTI_FILES_COUNT           MULTI_FILES_MINIMUM_COUNT + 1
#define MULTI_BUF_SIZE              DEVICE_BUF_SIZE
#define MULTI_THREADS_COUNT         500
#define LARGE_FILE_NAME             "litechrdrv.ko"
#define LARGE_FILE_THREAD_DELAY_US  500

struct stat large_file_st; 

int open_shared(void)
{
    int fd;
    printf("Opening shared mode file: ");
    fd = open("/dev/litechr", O_RDWR);
    if (fd < 0) 
        printf("error: %d\n", -fd);
    else
        printf("OK\n");
    return fd;
}

int open_exclusive(void)
{
    int fd;
    printf("Opening exclusive mode file: ");
    fd = open("/dev/litechr", O_EXCL | O_RDWR);
    if (fd < 0)
        printf("error %d\n", -fd);
    else
        printf("OK\n");
    return fd;
}

int open_multi(void)
{
    int fd;
    printf("Opening multi mode file: ");
    fd = open("/dev/litechr", O_CREAT | O_RDWR);
    if (fd < 0)
        printf("error %d\n", -fd);
    else
        printf("OK\n");
    return fd;
}

int test_write(int fd, char *buf, size_t size)
{
    int res;
    res = write(fd, buf, size);
    if (res == 1)
        return res;
    if (res < 0)
        printf("Writing 0x%02hhX... res=%d, error %d\n", buf[0], res, errno);
    else
        printf("Writing 0x%02hhX... res=%d, OK\n", buf[0], res);
    return res;
}

int test_read(int fd, char *buf, size_t size)
{
    int res;
    res = read(fd, buf, size);
    if (res == 1 || res == 0)
        return res;
    if (res < 0) 
        printf("Reading: res=%d, error %d\n", res, errno);
    else {
        printf("Reading: 0x%02hhX... res=%d, OK\n", buf[0], res);
    }
    return res;
}

int compare_buffers(char *buf1, char *buf2, size_t size)
{
    //if (size == 1)
    //    printf("Testing 0x%02hhX == 0x%02hhX - ", buf1[0], buf2[0]);
    if (memcmp(buf1, buf2, size) == 0) {
        if (size > 1)
            printf("Consistency: OK\n");
        return 0;
    }
    printf("Consistency: failed\n");
    return -1;
}

void fill_test_buf(char *buf, size_t length, size_t addendum)
{
    size_t i;
    for (i = 0; i < length; i++)
        buf[i] = (char)(i + addendum - 128);
}

int clear_device_buffer(void)
{
    char rbuf[DEVICE_BUF_SIZE] = {0};
    int fd;
 
    printf("\nClearing device buffer\n\n"); 

    RETURN_ON_ERROR(fd = open_exclusive());
    RETURN_ON_ERROR(test_read(fd, rbuf, DEVICE_BUF_SIZE));
    close(fd);

    return 0;
}

int test_shared(void)
{
    char wbuf[SHARED_BUF_SIZE] = TEST_STRING;
    char rbuf[SHARED_BUF_SIZE] = {0};
    int fd[SHARED_FILES_COUNT];
    int i;
 
    printf("\nShared mode test\n\n"); 
    
    for (i = 0; i < SHARED_FILES_COUNT; i++)
        RETURN_ON_ERROR(fd[i] = open_shared());
    for (i = 0; i < SHARED_FILES_COUNT; i++)
        RETURN_ON_ERROR(test_write(fd[i], wbuf, TEST_SIZE));

    // Exclusive should not open
    if (open_exclusive() >= 0) {
        printf("Error: should not open!\n");
        return -1;
    }

    for (i = 0; i < SHARED_FILES_COUNT; i++) {
        memset(rbuf, 0, SHARED_BUF_SIZE);
        RETURN_ON_ERROR(test_read(fd[i], rbuf, TEST_SIZE));
        RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));
    }
    if (test_read(fd[SHARED_FILES_COUNT-1], rbuf, TEST_SIZE) < 0) {
        printf("Error: should not read! \n");
        return -1;
    }   
    for (i = 0; i < SHARED_FILES_COUNT; i++)
        close(fd[i]);
        
    printf("\nTest passed\n");
    
    return 0;
}

int test_exclusive(void)
{
    char wbuf[SHARED_BUF_SIZE] = TEST_STRING;
    char rbuf[SHARED_BUF_SIZE] = {0};
    int fd;
 
    printf("\nExclusive mode test\n\n"); 

    // Simple open/write/read/close test
    RETURN_ON_ERROR(fd = open_exclusive());
    RETURN_ON_ERROR(test_write(fd, wbuf, TEST_SIZE));
    RETURN_ON_ERROR(test_read(fd, rbuf, TEST_SIZE));
    RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));
    
    // Simple open restricted test
    if (open_shared() >= 0) {
        printf("Error: should not open shared!\n");
        return -1;
    }
    if (open_exclusive() >= 0) {
        printf("Error: should not open exclusive!\n");
        return -1;
    }
    if (open_multi() >= 0) {
        printf("Error: should not open!\n");
        return -1;
    }

    close(fd);

    printf("\nTest passed\n");

    return 0;
}


int test_multi(void)
{
    char wbuf[MULTI_BUF_SIZE] = TEST_STRING;
    char rbuf[MULTI_BUF_SIZE] = {0};
    int fd[MULTI_FILES_COUNT];
    unsigned char i;
    
    printf("\nMulti mode test\n\n"); 
    
    // Simple open/write/read/close test
    RETURN_ON_ERROR(fd[0] = open_multi());
    RETURN_ON_ERROR(test_write(fd[0], wbuf, TEST_SIZE));
    RETURN_ON_ERROR(test_read(fd[0], rbuf, TEST_SIZE));
    RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));

    // Exclusive should not open
    if (open_exclusive() >= 0) {
        printf("Error: should not open!\n");
        return -1;
    }
    
    // Shared should open/write/read/close
    RETURN_ON_ERROR(fd[1] = open_shared());
    RETURN_ON_ERROR(test_write(fd[1], wbuf, TEST_SIZE));
    memset(rbuf, 0, MULTI_BUF_SIZE);
    RETURN_ON_ERROR(test_read(fd[1], rbuf, TEST_SIZE));
    RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));

    // Test multi write/read one more time
    RETURN_ON_ERROR(test_write(fd[0], wbuf, TEST_SIZE));
    memset(rbuf, 0, MULTI_BUF_SIZE);
    RETURN_ON_ERROR(test_read(fd[0], rbuf, TEST_SIZE));
    RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));

    close(fd[0]);

    close(fd[1]);

    // Test multi mode independency
    for (i = 0; i < MULTI_FILES_COUNT; i++)
        RETURN_ON_ERROR(fd[i] = open_multi());
    for (i = 0; i < MULTI_FILES_COUNT; i++) {
        fill_test_buf(wbuf, MULTI_BUF_SIZE, i);
        RETURN_ON_ERROR(test_write(fd[i], wbuf, MULTI_BUF_SIZE));
    }
    for (i = 0; i < MULTI_FILES_COUNT; i++) {
        fill_test_buf(wbuf, MULTI_BUF_SIZE, i);
        RETURN_ON_ERROR(test_read(fd[i], rbuf, MULTI_BUF_SIZE));
        RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, MULTI_BUF_SIZE));
    }
    for (i = 0; i < MULTI_FILES_COUNT; i++)
        close(fd[i]);
    
    printf("\nTest passed\n");

    return 0;
}

void *mul_test_thread_fn(void *arg)
{
    char wbuf[MULTI_BUF_SIZE] = {0};
    char rbuf[MULTI_BUF_SIZE] = {0};
    int fd;

    fill_test_buf(wbuf, MULTI_BUF_SIZE, 0);

    // Simple open/write/read/close test
    RETURN_ON_ERROR_THREAD(fd = open_multi());
    RETURN_ON_ERROR_THREAD(test_write(fd, wbuf, MULTI_BUF_SIZE));
    RETURN_ON_ERROR_THREAD(test_read(fd, rbuf, MULTI_BUF_SIZE));
    RETURN_ON_ERROR_THREAD(compare_buffers(wbuf, rbuf, MULTI_BUF_SIZE));
    close(fd);
    
    pthread_exit(NULL);
}
           
int test_shared_threads(void)
{
    char wbuf[MULTI_BUF_SIZE] = TEST_STRING;
    char rbuf[MULTI_BUF_SIZE] = {0};
    int fd, res;
    pthread_t threads[MULTI_THREADS_COUNT];
    long tres, i, j;
    
    printf("\nMulti mode thread test\n\n"); 
    
    // Simple open/write/read/close shared test
    RETURN_ON_ERROR(fd = open_shared());
    RETURN_ON_ERROR(test_write(fd, wbuf, TEST_SIZE));
    RETURN_ON_ERROR(test_read(fd, rbuf, TEST_SIZE));
    RETURN_ON_ERROR(compare_buffers(wbuf, rbuf, TEST_SIZE));

    printf("\nLauching %d threads\n", MULTI_THREADS_COUNT); 
    for (i = 0; i < MULTI_THREADS_COUNT; i++) {
        res = pthread_create(&threads[i], NULL, mul_test_thread_fn, NULL);
        if (res < 0) {
            for (j = 0; j < i; j++) {
                pthread_cancel(threads[i]);
                pthread_join(threads[i], NULL);
            }
            return res;
        }
    }
    
    for (i = 0; i < MULTI_THREADS_COUNT; i++) {
        res = pthread_join(threads[i], (void*)&tres);
        if (res < 0) {
            for (j = i + 1; j < MULTI_THREADS_COUNT; j++) {
                pthread_cancel(threads[j]);
                pthread_join(threads[j], NULL);
            }
            return res;
        }
        if (tres < 0) {
            for (j = i + 1; j < MULTI_THREADS_COUNT; j++) {
                pthread_cancel(threads[j]);
                pthread_join(threads[j], NULL);
            }
            printf("Test failed, err: %ld\n", -tres);
            return tres;
        }
    }

    close(fd);

    printf("\nTest passed\n");

    return 0;
}

int test_large_shared(void)
{
    struct stat large_file_st; 
    char *wbuf, *rbuf;
    int fd, large_filed;
    int res;

    printf("\nLarge file in shared mode test\n\n"); 
    
    if (stat(LARGE_FILE_NAME, &large_file_st))
        return -1;

    large_filed = open(LARGE_FILE_NAME, O_RDONLY);
    if (large_filed < 0) {
        printf("malloc: error %d errno=%d\n", large_filed, errno);
        return large_filed;
    }

    wbuf = malloc(large_file_st.st_size);
    if (!wbuf) {
        printf("malloc: errno=%d\n", errno);
        return -1;
    }
    rbuf = malloc(large_file_st.st_size);
    if (!rbuf) {
        free(wbuf);
        printf("malloc: errno=%d\n", errno);
        return -1;
    }
 
    res = read(large_filed, wbuf, large_file_st.st_size);
    if (res < 0) {
        free(wbuf);
        free(rbuf);
        printf("malloc: error %d errno=%d\n", res, errno);
        return res;
    }

    fd = open_shared();
    if (fd < 0) {
        free(wbuf);
        free(rbuf);
        return fd;
    }
    if ((res = test_write(fd, wbuf, large_file_st.st_size)) < 0) {
        free(wbuf);
        free(rbuf);
        return res;
    }
    memset(rbuf, 0, SHARED_BUF_SIZE);
    if ((res = test_read(fd, rbuf, large_file_st.st_size)) < 0) {
        free(wbuf);
        free(rbuf);
        return res;
    }
    if ((res = compare_buffers(wbuf, rbuf, large_file_st.st_size)) < 0) {
        free(wbuf);
        free(rbuf);
        return res;
    }
    close(fd);
        
    free(wbuf);
    free(rbuf);

    printf("\nTest passed\n");
    
    return 0;
}

void *test_large_write_thread_fn(void *wbuf)
{
    int fd;
    size_t i;

    RETURN_ON_ERROR_THREAD(fd = open_shared());
    for (i = 0; i < large_file_st.st_size; i++) {
        RETURN_ON_ERROR_THREAD(test_write(fd, wbuf + i, 1));
        usleep(LARGE_FILE_THREAD_DELAY_US);
    }
    close(fd);
    
    pthread_exit(NULL);
}

void *test_large_read_thread_fn(void *wbuf)
{
    int fd, res;
    size_t i, p, pn;
    char el;

    RETURN_ON_ERROR_THREAD(fd = open_shared());
    printf("Test progress: 0%%");
    fflush(stdout);
    for (i = 0, p = 0; i < large_file_st.st_size; i++) {
        pn = ((double)i / large_file_st.st_size) * 100;
        if (pn > p) {
            printf("\rTest progress: %ld%%", pn);
            fflush(stdout);
            p = pn;
        }
        usleep(LARGE_FILE_THREAD_DELAY_US);
        res = test_read(fd, &el, 1);
        if (res < 0)
            pthread_exit((void*)(long)res);
        if (res == 0) {
            usleep(LARGE_FILE_THREAD_DELAY_US);
            res = test_read(fd, &el, 1);
            if (res < 0)
                pthread_exit((void*)(long)res);
            if (res == 0)
                pthread_exit((void*)(long)-1);
        }
        RETURN_ON_ERROR_THREAD(compare_buffers(wbuf + i, &el, 1));
    }
    printf("\rTest progress: 100%%\n");
    close(fd);
    
    pthread_exit(NULL);
}

int test_large_shared_threads(void)
{
    pthread_t thread_writer, thread_reader;
    long tres;
    char *wbuf;
    int large_filed;
    int res;

    printf("\nLarge file in shared mode parallel thread test\n"); 
    
    if (stat(LARGE_FILE_NAME, &large_file_st))
        return -1;

    large_filed = open(LARGE_FILE_NAME, O_RDONLY);
    if (large_filed < 0) {
        printf("malloc: error %d errno=%d\n", large_filed, errno);
        return large_filed;
    }

    //usleep(1000);

    wbuf = malloc(large_file_st.st_size);
    if (!wbuf) {
        printf("malloc: errno=%d\n", errno);
        return -1;
    }
 
    res = read(large_filed, wbuf, large_file_st.st_size);
    if (res < 0) {
        free(wbuf);
        printf("malloc: error %d errno=%d\n", res, errno);
        return res;
    }

    printf("\nLauching writer thread\n"); 
    res = pthread_create(&thread_writer, NULL, test_large_write_thread_fn, (void *)wbuf);
    if (res < 0) {
        free(wbuf);
        printf("pthread_create thread_writer: error %d errno=%d\n", res, errno);
        return res;
    }

    printf("\nLauching reader thread\n\n"); 
    res = pthread_create(&thread_reader, NULL, test_large_read_thread_fn, (void *)wbuf);
    if (res < 0) {
        pthread_cancel(thread_writer);
        pthread_join(thread_writer, NULL);
        free(wbuf);
        printf("pthread_create thread_reader: error %d errno=%d\n", res, errno);
        return res;
    }
    
    printf("\n\n\n"); 
    
    res = pthread_join(thread_reader, (void*)&tres);
    if (res < 0) {
        pthread_cancel(thread_writer);
        pthread_join(thread_writer, NULL);
        free(wbuf);
        printf("pthread_join thread_reader: error %d errno=%d\n", res, errno);
        return res;
    }
    if (tres < 0) {
        pthread_cancel(thread_writer);
        pthread_join(thread_writer, NULL);
        free(wbuf);
        printf("Reader thread failed, err: %ld\n", -tres);
        return tres;
    }
    res = pthread_join(thread_writer, (void*)&tres);
    if (res < 0) {
        free(wbuf);
        printf("pthread_join thread_writer: error %d errno=%d\n", res, errno);
        return res;
    }
    if (tres < 0) {
        free(wbuf);
        printf("Writer thread failed, err: %ld\n", -tres);
        return tres;
    }

    free(wbuf);

    printf("\nTest passed\n");

    return 0;
}

int main(void)
{
    clear_device_buffer();
    
    // Tests of device shared mode open
    RETURN_ON_ERROR(test_shared());
    // Tests of device exclusive mode open
    RETURN_ON_ERROR(test_exclusive());
    // Tests of device multiple file context mode open
    RETURN_ON_ERROR(test_multi());
    // Second tests of device shared mode open after multi mode
    RETURN_ON_ERROR(test_shared());
    // Second tests of device exclusive mode open after previous modes
    RETURN_ON_ERROR(test_exclusive());
    // Test using device with multiple threads (writes then reads)
    RETURN_ON_ERROR(test_shared_threads());
    // Test writing and reading of a large file using shared mode (driver should be compiled with MAX_BUFFER_SIZE = 20000 for litechrdrv.ko)
    //RETURN_ON_ERROR(test_large_shared());
    // Test using device with multiple threads (simultaneous write and read of a large file)
    // Can fail due to device buffer overflow    
    RETURN_ON_ERROR(test_large_shared_threads());
    
    printf("\nAll tests passed\n\n");

    return 0;
}
