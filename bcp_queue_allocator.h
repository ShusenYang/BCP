/* 
 * File:   bcp_queue_allocator.h
 *
 *
 * Created on January 22, 2014, 3:16 PM
 */

#ifndef BCP_QUEUE_ALLOCATOR_H
#define	BCP_QUEUE_ALLOCATOR_H

/**
 * Called after bcp_queue is initialized. This function handles all the complexity details 
 * regarding the memory allocation of the queue list.
 */
void bcp_queue_allocator_init(struct bcp_conn *c);

#endif	/* BCP_QUEUE_ALLOCATOR_H */

