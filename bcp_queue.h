/**
 * \file
 *         Header file for packet queue.
 * 
 */
#ifndef __BCP_QUEUE_H__
#define __BCP_QUEUE_H__

#include "lib/list.h"
#include "lib/memb.h"
#include "net/packetbuf.h"
#include "bcp-config.h"

/**
 * \brief      A structure defines a bcp queue
 *             Every BCP connection has one queue which is used to store user packets
 *             at runtime. 
 */
struct bcp_queue {
  //It is a list
  list_t *list;
  //Memory allocation
  struct memb *memb;
  //Parent BCP connection for the queue
  void* bcp_connection;
};

/**
 * \brief      A structure for the header part of bcp packets
 */
struct bcp_packet_header {
    /**
     * Backlog
     */
    uint16_t bcp_backpressure;
    /**
     * The addressed of the node which generated the packet
     */
    rimeaddr_t origin;
    /**
     * Packet processing delay
     */
    clock_time_t delay; 
    /**
     * last time this packet has been processed. Used to calculate the packet delay
     */
    clock_time_t lastProcessTime;
};

/**
 * \brief      A structure for the records of the packet queue
 */
struct bcp_queue_item {
  //Linked list
  struct bcp_queue_item *next;
  /**
   * The data section
   */
  char data[MAX_USER_PACKET_SIZE]; //Data
  /**
   * The length of the data section
   */
  uint16_t data_length;
  /**
   * The header section
   */
  struct bcp_packet_header hdr; //Header
};

/**
 * \breif Initializes the packet queue.
 * 
 * \param c the BCP connection for the packet queue.
 * 
 *        This function has to be called before using any other bcp_queue_* functions.
 *        Packet queue is a data structure used to store user data packets at runtime. 
 *        Packet queue needs to be initialized before using it to allocate necessary 
 *        memory allocation tasks. 
 */
void bcp_queue_init(void *c);

/**
 * \breif Returns the first packet in the packet queue
 * 
 * \param s the packet queue
 * \return the first record item stored in the given packet queue
 */
struct bcp_queue_item * bcp_queue_top(struct bcp_queue *s);

/**
 * \return the queue item for the given index. 
 */
struct bcp_queue_item * bcp_queue_element(struct bcp_queue *s, uint16_t index);

/**
 * \brief Adds the given queue item to the queue
 * \param s the packet queue
 * \param i the new queue item
 * \return the record representation of the packet after being added to the queue.
 * 
 *          This function adds the given item to the queue. It uses memory copy API
 *          to copy the item into a new memory location. Thus, the parameter (i) can
 *          be local or released safely after calling this function.
 */
struct bcp_queue_item * bcp_queue_push(struct bcp_queue *s, struct bcp_queue_item *i);


/**
 * \breif Removes the first packet from the packet queue
 * \param s the packet queue
 */
void bcp_queue_pop(struct bcp_queue *s);

/**
 * \breif Removes the given record from the packet queue
 * 
 * \param s the packet queue
 * \param i the record which is required to be removed.
 * 
 *      This function permanently deletes the given queue_item record from the 
 *      packet queue. If queue item does not exist in the queue, this function
 *      will not do anything.
 *      
 */
void bcp_queue_remove(struct bcp_queue *s, struct bcp_queue_item *i);

/**
 * 
 * \param s the packet queue
 * \return the number of the packets existing in the given packet queue.
 */
int bcp_queue_length(struct bcp_queue *s);

/**
 * \breif Deletes all the records of the given packet queue
 * 
 * \param s the packet queue
 */
void bcp_queue_clear(struct bcp_queue *s);

#endif