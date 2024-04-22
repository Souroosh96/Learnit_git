/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 *  QNXcode.c
 *
 *  Created on: April 18, 2024
 *  Author: Souroosh Memarian
 *////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>


#define LOG_FILE "system.log"

#define M 10
#define N 20


/****Structures****/
// Data packet structure
typedef struct {
    char *data;                                                         // Pointer to the data buffer
    int size;                                                           // Size of the data buffer in bytes
    unsigned long eventId;      
    unsigned long eventCorrelationId; 
} DataPacket;

// Queue node structure
typedef struct node {
    DataPacket packet;                                                  // Data packet stored in the node
    struct node *next;                                                  // Pointer to the next node in the queue
} Node;

// Queue structure
typedef struct {
    Node *head, *tail;                                                  // Pointers to the head and tail of the queue
    int count;                                                          // Number of elements in the queue
    sem_t full, empty;                                                  // Semaphores to manage full and empty states of the queue
    pthread_mutex_t lock;                                               // Mutex for ensuring thread-safe access to the queue
} Queue;

Queue dataQueue; // Shared queue

/****************

/****Prototypes of Functions for QNXcode****/

void log_message(const char *message);
void initializeQueue(Queue *q, int size);
void enqueue(Queue *q, DataPacket data);
DataPacket dequeue(Queue *q);
int get_external_data(char *buffer, int bufferSizeInBytes);
void process_data(char *buffer, int bufferSizeInBytes);
void *writer_thread(void *arg);
void *reader_thread(void *arg);

/*****************/

/****Implementations of Functions for QNXcode****/


/********************************************************
 * @fn                        -log_message
 *
 * @brief                     -Basic logging function
 *
 * @param[in]                 message     Pointer to a C-string
 *
 * @return                    -none
 * @note                      -none
 ********************************************************/
void log_message(const char *message) {
    FILE *file = fopen(LOG_FILE, "a");
    if (file != NULL) {
        fprintf(file, "%s\n", message);
        fclose(file);
    }
}

/********************************************************
 * @fn                        -initializeQueue
 *
 * @brief                     -Function to initialize queue
 *
 * @param[in]                 q     Pointer to the Queue structure
 * @param[in]                 size  Size of the queue (maximum capacity)
 *
 * @return                    -none
 * @note                      -none
 ********************************************************/
void initializeQueue(Queue *q, int size) {
    q->head = q->tail = NULL;                                           // Initialize queue pointers to NULL
    q->count = 0;                                                       // Initialize queue count to zero
    sem_init(&q->full, 0, 0);                                           // Initialize semaphore 'full' with initial value 0
    sem_init(&q->empty, 0, size);                                       // Initialize semaphore 'empty' with max size
    pthread_mutex_init(&q->lock, NULL);                                 // Initialize the queue lock
    log_message("Queue initialized.");
}

/********************************************************
 * @fn                        -enqueue
 *
 * @brief                     -Function to push data into the queue
 *
 * @param[in]                 q     Pointer to the Queue structure
 * @param[in]                 data  DataPacket to be enqueued
 *
 * @return                    -none
 * @note                      -none
 ********************************************************/
void enqueue(Queue *q, DataPacket data) {
    Node *newNode = (Node*) malloc(sizeof(Node));                       // Allocate memory for a new node
    if (newNode == NULL)
    {
    	log_message("Error: Memory allocation failed for new node.");
    	return;                                                         // Handle memory allocation failures
    }
    
    newNode->packet = data;                                             // Store data packet in the new node
    newNode->next = NULL;                                               // Set the next pointer of the new node to NULL

    sem_wait(&q->empty);                                                // Decrement 'empty' semaphore (wait if queue is full)
    pthread_mutex_lock(&q->lock);                                       // Acquire lock before modifying the queue

    if (q->tail == NULL) {                                              // If queue is empty
        q->head = q->tail = newNode;                                    // Set head and tail to the new node
    } else {
        q->tail->next = newNode;                                        // Append the new node to the end of the queue
        q->tail = newNode;                                              // Update the tail pointer
    }
    q->count++;                                                         // Increment queue element count

    pthread_mutex_unlock(&q->lock);                                     // Release the lock
    sem_post(&q->full);                                                 // Increment 'full' semaphore (signal queue is not empty)
    log_message("Data enqueued.");
}
 
/********************************************************
 * @fn                        -dequeue
 *
 * @brief                     -Function to pop data from the queue
 *
 * @param[in]                 q     Pointer to the Queue structure from which data will be dequeued
 *
 * @return                    DataPacket containing the dequeued data, or a zero-initialized DataPacket if the queue is empty
 * @note                      This function blocks if the queue is empty, waiting for data to become available.
 ********************************************************/
DataPacket dequeue(Queue *q) {
    DataPacket data = {0};                                              // Initialize data packet to zero

    sem_wait(&q->full);                                                 // Decrement 'full' semaphore (wait if queue is empty)
    pthread_mutex_lock(&q->lock);                                       // Acquire lock before modifying the queue

    if (q->head == NULL) {                                              // If queue is unexpectedly empty
        pthread_mutex_unlock(&q->lock);                                 // Release the lock
        log_message("Error: Tried to dequeue from an empty queue.");
        sem_post(&q->empty);                                            // Correct semaphore state, should not wait if there's an error
        return data;                                                    // Return empty data if queue is unexpectedly empty
    }

    Node *temp = q->head;                                               // Temporary pointer to the head of the queue
    data = temp->packet;                                                // Retrieve data packet from the head node
    q->head = q->head->next;                                            // Move head pointer to the next node

    if (q->head == NULL) {
        q->tail = NULL;                                                 // If queue becomes empty, update tail pointer
    }
    q->count--;                                                         // Decrement queue element count

    pthread_mutex_unlock(&q->lock);                                     // Release the lock
    sem_post(&q->empty);                                                // Increment 'empty' semaphore (signal queue is not full)

    free(temp);                                                        // Free memory of the dequeued node
    log_message("Data dequeued.");
    return data;                                                        // Return the dequeued data packet
}
 
/********************************************************
 * @fn                        -get_external_data
 *
 * @brief                     -Function to simulate data retrieval from an external source
 *
 * @param[in]                 bufferSizeInBytes   Size of the buffer in bytes
 *
 * @return                    Number of bytes copied to the buffer, or -1 if the buffer size is insufficient
 * @note                      This function generates random data and copies it to the provided buffer. 
 *                            It returns the number of bytes copied, which can be less than or equal to bufferSizeInBytes.
 ********************************************************/
int get_external_data(char *buffer, int bufferSizeInBytes) {
    int val;
    char srcString[] = "0123456789abcdefghijklmnopqrstuvwxyxABCDEFGHIJKLMNOPQRSTUVWXYZ";

    val = (int)(rand() % bufferSizeInBytes);                            // Generate random value within buffer size

    if (bufferSizeInBytes < val)
        return (-1);                                                    // Return error if buffer size is less than required

    strncpy(buffer, srcString, val);                                    // Copy random data to buffer
    return val;                                                         // Return number of bytes copied
}

/********************************************************
 * @fn                        -process_data
 *
 * @brief                     -Function to process data
 *
 * @param[in]                 buffer              Pointer to the data buffer to be processed
 * @param[in]                 bufferSizeInBytes   Size of the data buffer in bytes
 *
 * @return                    -none
 * @note                      This function prints the contents of the data buffer character by character
 *                            and clears the buffer after printing. It also handles the case where the buffer
 *                            pointer is NULL by printing an error message.
 ********************************************************/
void process_data(char *buffer, int bufferSizeInBytes) {
    int i;

    if (buffer) {
        printf("thread %llu - ", pthread_self());                       // Print thread ID
        for (i = 0; i < bufferSizeInBytes; i++) {
            printf("%c", buffer[i]);                                    // Print each character in the buffer
        }
        printf("\n");
        memset(buffer, 0, bufferSizeInBytes);                           // Clear the buffer
    } else {
        printf("error in process data - %llu\n", pthread_self());       // Print error if buffer is NULL
    }
}
 
/********************************************************
 * @fn                        -writer_thread
 *
 * @brief                     -Writer thread
 *
 * @param[in]                 arg       Pointer to optional thread argument (not used in this function)
 *
 * @return                    -none
 * @note                      This function represents a writer thread that continuously retrieves
 *                            external data, encapsulates it into a `DataPacket` structure, and enqueues
 *                            the packet into a shared queue (`dataQueue`) for processing by reader threads.
 *                            If data retrieval fails or returns an empty packet, memory allocated for the
 *                            data buffer (`packet.data`) is freed to avoid memory leaks.
 ********************************************************/
void *writer_thread(void *arg) {
    while (1) {
        DataPacket packet;
        packet.data = (char*) malloc(1024);                             // Allocate memory for the buffer
        if (packet.data == NULL) {
            log_message("Error: Memory allocation failed for data packet.");
            continue;                                                   // Handle memory allocation failure
        }
        packet.size = get_external_data(packet.data, 1024);             // Retrieve external data
        packet.eventId = 0; 
        packet.eventCorrelationId = 0; 

        if (packet.size > 0) {
            enqueue(&dataQueue, packet);                                // Enqueue the data packet
        } else {
            free(packet.data);                                          // Clean up if data retrieval failed
        }
    }
    return NULL;
}

/********************************************************
 * @fn                        -reader_thread
 *
 * @brief                     -Reader thread

 *
 * @param[in]                 arg       Pointer to optional thread argument (not used in this function)
 *
 * @return                    -none
 * @note                      This function represents a reader thread that continuously dequeues
 *                            data packets from a shared queue (`dataQueue`). If a valid data packet
 *                            is dequeued, it processes the data using the `process_data` function and
 *                            then frees the associated memory of the data buffer (`packet.data`).
 ********************************************************/
void *reader_thread(void *arg) {
    while (1) {
         DataPacket packet = dequeue(&dataQueue);                       // Dequeue a data packet from the queue
        if (packet.size > 0) {
            process_data(packet.data, packet.size);                     // Process the data packet
            free(packet.data);                                          // Free the data buffer after processing
        }
    }
    return NULL;
}

/*****************/

int main(int argc, char **argv) {
    pthread_t writers[N], readers[M];

    initializeQueue(&dataQueue, 100);                                   // Initialize the shared queue with a size limit

    // Create writer threads
    for (int i = 0; i < N; i++) {
        pthread_create(&writers[i], NULL, writer_thread, NULL);
    }

    // Create reader threads
    for (int i = 0; i < M; i++) {
        pthread_create(&readers[i], NULL, reader_thread, NULL);
    }

    // Wait for all writer threads to complete
    for (int i = 0; i < N; i++) {
        pthread_join(writers[i], NULL);
    }

    // Wait for all reader threads to complete
    for (int i = 0; i < M; i++) {
        pthread_join(readers[i], NULL);
    }

    return 0;
}
