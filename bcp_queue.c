/**
 * \file
 *         The default implementation of bcp_queue (see \ref bcp_queue.h). This
 *         implementation uses linked list DS to store data packets.
 *
 */
#include "bcp_queue.h"
#include "bcp.h"


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif



void bcp_queue_init(void *c){
    //Setup BCP
    struct bcp_conn * bcp_c = (struct bcp_conn *) c;
    bcp_c->packet_queue.list = &(bcp_c->packet_queue_list);
    bcp_c->packet_queue.bcp_connection = c;
    
    list_init(bcp_c->packet_queue_list);
    PRINTF("DEBUG: Bcp Queue has been initialized \n");
    
    /**
     * As seen, the actual memory allocation for the queue has to be performed 
     * separately by another component (the default implementation is BCP_Queue_Allocater. 
     * This will provide a great flexiablity in terms of extending the queue items and provide a customized header.
     */
}

struct bcp_queue_item * bcp_queue_top(struct bcp_queue *s){
    return list_head(*s->list);
}

struct bcp_queue_item * bcp_queue_element(struct bcp_queue *s, uint16_t index){
    
    struct bcp_queue_item * i = bcp_queue_top(s);
    int j;
    
    for(j = 1 ; j < index+1; j++)
        i = list_item_next(i);
    
    return i;
    
}

void bcp_queue_remove(struct bcp_queue *s, struct bcp_queue_item *i){
   PRINTF("DEBUG: Removing an item from the packet queue\n");
   //Null is not allowed here
   if(i != NULL) {
    list_remove(*s->list, i);
    memb_free(s->memb, i);
  }else{
       PRINTF("ERROR: Passed queue item record cannot be removed from the packet queue\n");
  }
}

void bcp_queue_pop(struct bcp_queue *s){
    PRINTF("DEBUG: Removing the first item from the packet queue\n");
    struct bcp_queue_item *  i = bcp_queue_top(s);
    bcp_queue_remove(s, i);
}

int bcp_queue_length(struct bcp_queue *s){
    return list_length(*s->list);
}

struct bcp_queue_item * bcp_queue_push(struct bcp_queue *s, struct bcp_queue_item *i){
    struct bcp_queue_item * newRow;
    
    //Make sure the queue is not full
    uint16_t current_queue_length =  bcp_queue_length(s);
     if(current_queue_length + 1 > MAX_PACKET_QUEUE_SIZE){
        PRINTF("ERROR: Packet Queue is full, a new packet will be dropped \n");
        return NULL;
    }
    
    // Allocate a memory block for the new record
    newRow = memb_alloc(s->memb);
  
     if(newRow == NULL) {
         PRINTF("DEBUG: Error, memory cannot be allocated for a bcp_queue_item record \n");
         return NULL;
     }
    
    
    //Sets the fields of the new record
    newRow->next = NULL;
    newRow->hdr.bcp_backpressure = 0;
    newRow->data_length = i->data_length;
    
    memcpy(newRow->data, i->data, newRow->data_length);
    
    
    //Add the row to the queue
    list_push(*s->list, newRow);
    
    PRINTF("DEBUG: Pushing a new data packet to the packet queue\n");
    return bcp_queue_top(s);
    
}


void bcp_queue_clear(struct bcp_queue *s){
  //For every stored record
  while(bcp_queue_top(s) != NULL) {
    bcp_queue_pop(s);
  }
  
  PRINTF("DEBUG: Packet Queue has been cleared\n");
}