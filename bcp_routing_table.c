/**
 * \file
 *         default implementation of routing table.
 * 
 */

#include "bcp_routing_table.h"
#include "bcp.h"


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


void routing_table_init(void *c){
    //Setup bcp
    struct bcp_conn * bcp_c = (struct bcp_conn *) c;
    bcp_c->routing_table.list = &(bcp_c->routing_table_list);
    bcp_c->routing_table.bcp_connection = c;
    //Init the list
    list_init(bcp_c->routing_table_list);
    
    PRINTF("DEBUG: Bcp routing table has been initialized \n");
}

struct routingtable_item* routing_table_find(struct routingtable *t,
                               const rimeaddr_t * addr){
     struct routingtable_item *i = NULL;
   
    // Check for entry using binary search as number of records is usually very limited
    for(i = list_head(*t->list); i != NULL; i = list_item_next(i)) {
      if(rimeaddr_cmp(&(i->neighbor), addr))
        break;
    }
   
     return i;
}

int routing_table_update_queuelog(struct routingtable *t,
                               const rimeaddr_t * addr,
                               uint16_t queuelog){
    struct routingtable_item *i;
   
    i = routing_table_find(t, addr);
    
    //No record for this neighbor address
    if(i == NULL) {
        // Allocate memory for the new record
        i = memb_alloc(t->memb);

        //Failed to allocate memory
        if(i == NULL) {
          return -1;
        }

        // Set default attributes
        i->next = NULL;
        rimeaddr_copy(&(i->neighbor), addr);
        i->backpressure = queuelog;
        
        //Ask weight estimator to initialize its fields 
        weight_estimator_record_init(i);
        
        //Insert the new record
        list_add(*t->list, i);
    }else{
        i->backpressure = queuelog;
    }
    //dbg_print_rtable(t);
    return 1;
}

int routingtable_length(struct routingtable *t)
{
  return list_length(*t->list);
}



void routingtable_clear(struct routingtable *t){
    
   struct routingtable_item *i;
   for(i = list_head(*t->list); i != NULL; i = list_item_next(i)) {
       list_remove(*t->list, i);
       memb_free(t->memb, i);
   }
   
   PRINTF("DEBUG: Routing table has been cleared\n");
}


rimeaddr_t* routingtable_find_routing( struct routingtable *t){
   
   int largestWeight = -32768;
   int neighborWeight;
   struct routingtable_item * largestNeightbor = NULL;
   struct routingtable_item *i;
   //For each neighbor stored 
   for(i = list_head(*t->list); i != NULL; i = list_item_next(i)) {
       //If smallest weight variable is not yet set 
       neighborWeight = weight_estimator_getWeight(t->bcp_connection, i);
       //Has this neighbor smaller weight
       if(largestWeight <= neighborWeight){
           largestWeight = neighborWeight;
           largestNeightbor = i;
       }
   }
   //No result
   if(largestNeightbor == NULL)
       return NULL;
   
   
     PRINTF("DEBUG: Best neighbor to send the data packet is node[%d].[%d] \n",
               largestNeightbor->neighbor.u8[0],
               largestNeightbor->neighbor.u8[1]);
   //The rime address of the neighbor
   return (&largestNeightbor->neighbor);
}

/*---------------------------------------------------------------------------*/
 void print_routingtable(struct routingtable *t)
{
  struct routingtable_item *i;
  uint8_t numItems = 0;
  uint8_t count = 1;
  numItems = routingtable_length(t);

  PRINTF("Routing Table Contents: %d entries found\n", numItems);
  PRINTF("------------------------------------------------------------\n");
  for(i = list_head(*t->list); i != NULL; i = list_item_next(i)) {
    PRINTF("Routing table item: %d\n", count);
    PRINTF("neighbor: %d.%d\n", i->neighbor.u8[0], i->neighbor.u8[1]);
    PRINTF("backpressure: %d\n", i->backpressure);
    weight_estimator_print_item(t->bcp_connection, i);
    PRINTF("------------------------------------------------------------\n");
    count++;
  }
}
/*---------------------------------------------------------------------------*/