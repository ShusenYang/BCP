/**
 * \file
 *         Header file for routing table.
 * 
 */

#ifndef __ROUTINGTABLE_H__
#define __ROUTINGTABLE_H__

#include <stdbool.h>
#include <string.h>
#include "lib/list.h"
#include "lib/memb.h"
#include "net/rime.h"

/**
 * \brief      A structure defines routing table
 * 
 *             Every BCP connection has one routing table which is used to store 
 *             neighbor information. This structure can be extent by the weight 
 *             estimator (see \ref bcp_weight_estimator.h).
 * 
 */
struct routingtable {
  //link list
  list_t *list;
  //Memory allocation part
  struct memb *memb;
  //The parent BCP connection
  void* bcp_connection;
};

/**
 * \brief      A structure for records in routing table 
 *             
 *             Each record(item) in routing table represents a neighbor node existing
 *             in one-hop area. 
 */
struct routingtable_item {
  //link list
  struct routingtable_item *next;
  //Neighbor rime address
  rimeaddr_t neighbor;
  //Queue log; updated frequently by the BCP routing
  uint16_t backpressure;
  
};


/**
 * \breif Initializes the routing table for the given backpressure connection
 * 
 * \param c the opened backpressure connection
 */
void routing_table_init(void *c);

/**
 * \breif Updates the queuelog of the given neighbor
 * 
 * \param t the routing table containing neighbor records
 * \param addr the rime address of the neighbor 
 * \param queuelog the new queue log 
 * \return Non-zero if the neighbor record was updated. Otherwise, zero
 */
int routing_table_update_queuelog(struct routingtable *t,
                               const rimeaddr_t * addr,
                               uint16_t queuelog);
/**
 * \breif finds the given neighbor in the routing table
 * 
 * \param t the routing table 
 * \param addr the rime address of the neighbor
 * \return the routingtable_item for the neighbor or NULL is this neighbor does not exist in the routing table
 */
struct routingtable_item* routing_table_find(struct routingtable *t,
                               const rimeaddr_t * addr);

/**
 * \breif Clears and deallocates the given routing table
 * \param t the routing table that is required to be cleared.
 */
void routingtable_clear(struct routingtable *t);

/**
 * 
 * \param t the routing table 
 * \return returns the number of the records (neighbors) that is stored in the
 *  given routing table
 */
int routingtable_length(struct routingtable *t);

/**
 * 
 * \param t
 * \return Finds the neighbor which has the highest weight in the routing table
 */
rimeaddr_t* routingtable_find_routing(struct routingtable *t);


#endif /* __ROUTINGTABLE_H__ */
