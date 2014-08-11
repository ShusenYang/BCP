#include "bcp.h"
#include "bcp_queue.h"
#include "bcp_queue_allocator.h" //To customize the queue item 


//Memory allocation for the routing table. This is defined here because 
MEMB(packet_queue_memb, struct bcp_queue_item, MAX_PACKET_QUEUE_SIZE);

void bcp_queue_allocator_init(struct bcp_conn *c){    
    c->packet_queue.memb = &packet_queue_memb;
    memb_init(&packet_queue_memb);
}
