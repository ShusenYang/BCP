/**
 * \file
 *         Header file for weight estimator. 
 *         
 *         Weight estimator calculates link weight between nodes. When a node wants 
 *         to transfer a data packet, the backpressure selects the node that has 
 *         the highest link weight. This component defines how the link weight is 
 *         calculated. 
 * 
 *         This component can be seen as an extension point for the backpressure.
 *         Defining a custom weight estimator implementation allows users to easily 
 *         customize the backpressure implementation. 
 *          
 * 
 */

#ifndef __WEIGHT_ESTIMATOR_H__
#define __WEIGHT_ESTIMATOR_H__
#include "bcp.h"

/**
 * \breif Initializes weight estimator component for the given bcp connection
 * 
 * \param c an opened bcp connection.
 * 
 *      This function is called before using any other weight_estimator_* functions.
 */
void weight_estimator_init(struct bcp_conn *c);

/**
 * \breif Initializes weight estimator metrics for the given routing table record
 * 
 * \param it routing table record
 *      This function is called when a new neighbor is added to the routing
 *      table to ask the weight estimator to initialize its custom fields. 
 */
void weight_estimator_record_init(struct routingtable_item * it);

/**
 * \breif Informs the weight estimators that a new packet has been successfully sent. 
 * 
 * \param it the routing table record for the destination address
 * \param i the packet record in the packet queue of the bcp connection
 * \param attempts the number of required transactions 
 */
void weight_estimator_sent(struct routingtable_item * it, 
                                struct bcp_queue_item *i, 
                                uint16_t attempts);

/**
 * \breif Calculates the weight for the given neighbor
 * 
 * \param c The bcp connection for the routing table
 * \param it the routing table record for the neighbor
 * \return The weight of the given neighbor 
 */
int weight_estimator_getWeight(struct bcp_conn *c, struct routingtable_item * it);

/**
 * \breif Prints weight estimator metrics for the given routing record
 * 
 * \param c The bcp connection of the routing table
 * \param item the routing table record
 */
void weight_estimator_print_item(struct bcp_conn *c, struct routingtable_item *item);

#endif /* __WEIGHT_ESTIMATOR_H__*/
