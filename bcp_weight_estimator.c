/**
 * \file
 *         Default implementation for the weight estimator
 *         
 *         In this implementation the weight is calculated based on the orginal
 *         backpressure weight equation (delta queuelogs).
 *          
 * 
 */
#include "bcp_weight_estimator.h"
#include "bcp-config.h"
#include <string.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


/*********************************DECLARATIONS*********************************/
/**
 * \brief      A structure add custom weight estimator metrics to routingtable item 
 *            
 */
struct routingtable_item_bcp {
  struct routingtable_item item;
};



//Memory allocation for the routing table. This is defined here because 
//weight estimators may require to add extra columns to the routingtable_item 
MEMB(routing_table_memb, struct routingtable_item_bcp, MAX_ROUTING_TABLE_SIZE);



/*********************************BCP PUBLIC FUNCTION**************************/
int weight_estimator_getWeight(struct bcp_conn *c, struct routingtable_item * it){
    struct routingtable_item_bcp * i = (struct routingtable_item_bcp *) it;
    int w = 0;
    
    
    //Calculate the weight 
    w = (int) bcp_queue_length(&c->packet_queue);
    w -= i->item.backpressure;
  
    return (int)w; 
}

void weight_estimator_sent(struct routingtable_item * it, 
                                struct bcp_queue_item *qi, 
                                uint16_t attempts){
    uint16_t new_link_packet_tx_time;
    struct routingtable_item_bcp * i = (struct routingtable_item_bcp *) it;
    
    PRINTF("DEBUG: Weight estimator updates routingtable_item metrics. Neighbor[%d].[%d], Attempts=[%d]\n"
        , it->neighbor.u8[0]
        , it->neighbor.u8[1]
        , attempts
        );
    
}

void weight_estimator_init(struct bcp_conn *c){
    c->routing_table.memb = &routing_table_memb;
    memb_init(&routing_table_memb);
}

void weight_estimator_record_init(struct routingtable_item * it){
    struct routingtable_item_bcp * i =  it;
    
}

void weight_estimator_print_item(struct bcp_conn *c, struct routingtable_item *item){
    struct routingtable_item_bcp * i = item;
    
    PRINTF("Weight: %d\n", weight_estimator_getWeight(c, item));
}