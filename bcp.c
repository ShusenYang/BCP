/**
 * \file
 *         The default implementation of backpressure routing (see \ref bcp.h).
 * 
 */
#include "bcp.h"

#include "net/rime.h"
#include "net/rime/unicast.h"
#include "net/rime/broadcast.h"
#include "net/netstack.h"
#include "bcp_extend.h"
#include "bcp_queue_allocator.h"

#include <stddef.h>  //For offsetof
#include "lib/list.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


/*********************************DECLARATIONS*********************************/
static const struct packetbuf_attrlist attributes[] = {
    BCP_ATTRIBUTES
    PACKETBUF_ATTR_LAST };

/**
 * \brief      A structure for beacon messages.
 *
 *             An opened BCP connection broadcast beacon messages on regular 
 *             basis to the one-hop neighbors.            
 */
struct beacon_msg {
  /**
  * The queue length of the node. 
  */
  uint16_t queuelog; 
};

/**
 * \brief      A structure for beacon request messages.
 *
 *             This is type of message is used when the node wants to ask its 
 *             neighbor to send their beacon messages. A beacon request message is 
 *             usually broadcasted when a node failed to delivery a data packet 
 *             to its best neighbor.
 */
struct beacon_request_msg {
  /**
  * The queue length of the node. 
  */
  uint16_t queuelog;
};

/**
 * \brief      A structure for acknowledgment messages.
 */
struct ack_msg {
};



static void send_beacon_request(void *ptr);
static void send_beacon(void *ptr);
static bool isBeacon();
static void prepare_packetbuf();
static bool isBeaconRequest();
static bool isBroadcast(rimeaddr_t * addr);
static void send_packet(void *ptr);
struct bcp_queue_item* push_packet_to_queue(struct bcp_conn *c);
static void send_ack(struct bcp_conn *bc, const rimeaddr_t *to);
static void retransmit_callback(void *ptr);


/*********************************CALLBACKS************************************/
/**
 * \breif Called when an ACK message is recieved
 * \param c The unicast connection 
 * \param from the address of the sender
 *      
 *      In BCP, the unicast channel is used to send ACK messages. This function 
 *      is the callback function for the opened unicast channel.
 */
static void recv_from_unicast(struct unicast_conn *c, const rimeaddr_t *from)
{
    struct bcp_queue_item *i;
    struct ack_msg m;
    struct routingtable_item * ri;

    PRINTF("DEBUG: Receiving an ACK via the unicast channel\n");
    
    // Cast the unicast connection as a BCP connection
    struct bcp_conn *bcp_conn = (struct bcp_conn *)((char *)c
        - offsetof(struct bcp_conn, unicast_conn));
    //Copy the header
    memcpy(&m, packetbuf_dataptr(), sizeof(struct ack_msg));
    
    //Remove the packet from the packet queue
    i = bcp_queue_top(&bcp_conn->packet_queue);
    
    if(i != NULL) {
      
        PRINTF("DEBUG: ACK received removing the current active packet from the queue\n");
        // Reset BCP connection for next packet to send
        bcp_conn->tx_attempts = 0;
        
        
        //Notify user that this packet has been sent
        if(bcp_conn->cb->sent != NULL){
            prepare_packetbuf();
            packetbuf_copyfrom(i->data, i->data_length);
            bcp_conn->cb->sent(bcp_conn);        
        }
        
        //Stop retransmission timer
        ctimer_stop(&bcp_conn->retransmission_timer);
        //Remove the packet from the queue
        bcp_queue_remove(&bcp_conn->packet_queue, i);
        
        //Notify the weight estimator
        ri = routing_table_find(&bcp_conn->routing_table, from);
        
        uint32_t link_estimate_time = DELAY_TIME 
                - timer_remaining(&bcp_conn->delay_timer);
        
        weight_estimator_sent(ri, i, bcp_conn->tx_attempts);

        bcp_conn->busy = false;
        
        // Reset the send data timer in case their are other packets in the queue
        clock_time_t time = SEND_TIME_DELAY;
        ctimer_set(&bcp_conn->send_timer, time, send_packet, bcp_conn);
        
    }else{
        PRINTF("ERROR: Cannot find the current active packet. ACK cannot be sent\n");
    }
}

/**
 * \breif Called whenever a new packet has been received by the broadcast channel
 * \param c Broadcast channel
 * \param from The sender rime address
 * 
 *      In BCP, the broadcast channel is used to send two types of packets (user
 *      data packets and beacons). This function is the callback function for the
 *      opened broadcast channel.
 */
static void recv_from_broadcast(struct broadcast_conn *c,
                                const rimeaddr_t *from)
{
    //Convert c to bcp instance
    struct bcp_conn *bc = (struct bcp_conn *)((char *)c
      - offsetof(struct bcp_conn, broadcast_conn));
    
    //Find the destination address of the packet 
    rimeaddr_t destinationAddress;
    rimeaddr_copy(&destinationAddress, packetbuf_addr(PACKETBUF_ADDR_ERECEIVER));
   
    
    //If it is a broadcast
    if(isBroadcast(&destinationAddress)){
        //It is either beacon or beacon request. 
        if(isBeacon()){
            PRINTF("DEBUG: Receiving a beacon from the broadcast channel\n");
    
            //Construct the beacon message
            struct beacon_msg beacon;
            memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));
            
            //Update the queue for that neighbor
            routing_table_update_queuelog(&bc->routing_table, from, beacon.queuelog);
        }else{
            PRINTF("DEBUG: Receiving a beacon request from the broadcast channel\n");
            struct beacon_request_msg br_msg;
            memcpy(&br_msg, packetbuf_dataptr(), sizeof(struct beacon_request_msg));
            
            //Update the queue for that neighbor
            routing_table_update_queuelog(&bc->routing_table, from, br_msg.queuelog);
            
            //Schedule a new beacon for the node
            //Generate random reply time to avoid collision (50ms - 1s)
            clock_time_t time = CLOCK_SECOND * 0.50f * (1+(random_rand() % 20)) ; 
            ctimer_set(&bc->beacon_timer, time, send_beacon, bc);
        }
        
    }else //If this node is the destination 
        if(rimeaddr_cmp(&destinationAddress, &rimeaddr_node_addr)){
            
            //Abstract the message
            struct bcp_queue_item * dm = (struct bcp_queue_item *) packetbuf_dataptr();
            PRINTF("DEBUG: Received a forwarded data packet sent to node[%d].[%d] (Origin: [%d][%d]), BCP=%d, delay=%x \n",
                  destinationAddress.u8[0], 
                  destinationAddress.u8[1], 
                  dm->hdr.origin.u8[0],
                  dm->hdr.origin.u8[1],
                  dm->hdr.bcp_backpressure,
                  dm->hdr.delay);
            
            if(!bc->isSink){
                //Add this packet to the queue so that we can forward it in the near future
                struct bcp_queue_item* itm;
                itm = bcp_queue_push(&bc->packet_queue, dm);
                 //Notify the extender
                if(bc->ce != NULL && bc->ce->onReceivingData != NULL)
                        bc->ce->onReceivingData(bc, itm);
                if(itm != NULL){
                     itm->hdr.lastProcessTime = clock_time();
                    
                     // Reset the send data timer
                    if(ctimer_expired(&(bc->send_timer))) {
                      clock_time_t time = SEND_TIME_DELAY;
                      ctimer_set(&bc->send_timer, time, send_packet, bc);
                    }
                }
                
                //Update the routing table
               routing_table_update_queuelog(&bc->routing_table, from, dm->hdr.bcp_backpressure);
                
             }else{
               //If it is Sink
               PRINTF("DEBUG: Sink Received a new data packet, user will be notified, total delay(ms)=%x\n", dm->hdr.delay);
               
               //Save the message
               struct bcp_queue_item pk;
               memcpy(&pk, dm, dm->data_length);
               
               //Send ACK
               send_ack(bc, from);

               //Notify end user callbacks
               //We need to rebuild packetbuf since we called send_ack
               prepare_packetbuf();
               
               memcpy(packetbuf_dataptr(), &pk.data, MAX_USER_PACKET_SIZE);
                        
               //Notify user callback
               if(bc->cb->recv != NULL)
                  bc->cb->recv(bc, &pk.hdr.origin);
               else 
                  PRINTF("ERROR: BCP cannot notify user as the receive callback function is not set.\n");
               
               //Update the routing table
               routing_table_update_queuelog(&bc->routing_table, from, pk.hdr.bcp_backpressure);
            }

           
            
    }else{
        //When the node is not the destination for the data pack. Just abstract 
        //the queue log from the header of the packet
        struct bcp_packet_header header;
        memcpy(&header, packetbuf_dataptr(), sizeof(struct bcp_packet_header));
        
        PRINTF("DEBUG: Receiving a data packet from node[%d].[%d] sent to node[%d].[%d] via the broadcast channel\n",
               from->u8[0] ,
               from->u8[1], 
               destinationAddress.u8[0], 
               destinationAddress.u8[1] );
        
        routing_table_update_queuelog(&bc->routing_table, from, header.bcp_backpressure);
    }
    
}


/**
 * \brief Called when a data packet or beacon has been sent via the broadcast channel
 * \param c A pointer to the broadcast channel
 * \param status the result of sending
 * \param transmissions number of attempts 
 * 
 *      In BCP, the broadcast channel is used to send two types of packets (user
 *      data packets and beacons). This function is the callback function for the
 *      opened broadcast channel.
 */
static void sent_from_broadcast(struct broadcast_conn *c, int status,
                                int transmissions)
{
        
     // Cast the broadcast connection as a BCP connection
    struct bcp_conn *bcp_conn = (struct bcp_conn *)((char *)c
      - offsetof(struct bcp_conn, broadcast_conn));

    // If it is a beacon
    if(isBeacon()) {
      bcp_conn->busy = false;

      // Reset the beacon timer
      if(ctimer_expired(&bcp_conn->beacon_timer)) {
        clock_time_t time = BEACON_TIME;
        ctimer_set(&bcp_conn->beacon_timer, time, send_beacon, bcp_conn);
      }
    }else if(isBeaconRequest()){
         bcp_conn->busy = false;
         
    }else{
       //If it is a data packet, setup the retransmit timer in case we didn't receive ACK
       clock_time_t time = RETX_TIME * bcp_conn->tx_attempts;
       ctimer_stop(&bcp_conn->retransmission_timer);
       ctimer_set(&bcp_conn->retransmission_timer, time, retransmit_callback,
               bcp_conn); 
    }
}

/**
 * Defines the callback fuctions for the broadcast and unicast channels
 */
static const struct broadcast_callbacks broadcast_callbacks = {
    recv_from_broadcast,
    sent_from_broadcast };
static const struct unicast_callbacks unicast_callbacks = { recv_from_unicast };
/******************************************************************************/

/*********************************UTILITIES************************************/
/**
 * 
 * \param addr
 * \return true if the given address is the general broadcast address. Otherwise, false
 */
static bool isBroadcast(rimeaddr_t * addr){
    rimeaddr_t broadcastAddress;
    broadcastAddress.u8[0] = 0;
    broadcastAddress.u8[1] = 0;
    return (rimeaddr_cmp(&broadcastAddress, addr));
}

/**
 * \return true if the current packet in packetbuf is beacon(see \ref "struct beacon_msg"). Otherwise, false.
 */
static bool isBeacon(){
    return (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) 
            == PACKETBUF_ATTR_PACKET_TYPE_BEACON);
}

/**
 * \return true if the current packet in packetbuf is beacon_request(see \ref "struct beacon_request_msg"). Otherwise, false.
 */
static bool isBeaconRequest(){
    return (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) 
            == PACKETBUF_ATTR_PACKET_TYPE_BEACON_REQUEST);
}

/**
 * \breif Called by the retransmission timer
 * \param ptr the bcp connection
 * 
 *     Every BCP connection has a retransmission timer. retransmission timer is 
 *     used to send a beacon request message when no ACK message is received. 
 */
static void retransmit_callback(void *ptr)
{
    struct bcp_conn *c = ptr;
    c->busy = false;
    PRINTF("DEBUG: Attempt to retransmit the data packet\n");
    //Send beacon request message
    send_beacon_request(c);
    
    //Reschedule the send timer.
    if(ctimer_expired(&c->send_timer)) {
        clock_time_t time = RETX_TIME ;
        ctimer_set(&c->send_timer, time, send_packet, c); 
    }
}

/**
 * \breif Broadcasts a beacon request message(see \ref "struct beacon_request_msg") to the one-hop neighbors.
 * \param ptr the bcp connection
 * 
 */
static void send_beacon_request(void * ptr){
    struct bcp_conn *c = ptr; 
    struct beacon_request_msg * br_msg;
    
    if(c->busy == false)
        c->busy = true;
    else
        return;
    
    //Delete all the records in the routing table
    routingtable_clear(&c->routing_table);
    
    prepare_packetbuf();
    packetbuf_set_datalen(sizeof(struct beacon_request_msg));
    
    br_msg = packetbuf_dataptr();
    memset(br_msg, 0, sizeof(br_msg));
    
    // Store the local backpressure level to the backpressure field
    br_msg->queuelog = bcp_queue_length(&c->packet_queue);
     
    //Update the packet buffer 
    //TDOO: Check if this is required
    memcpy(packetbuf_dataptr(), br_msg, sizeof(struct beacon_request_msg));
    
    // Set the packet type using packetbuf attribute
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                     PACKETBUF_ATTR_PACKET_TYPE_BEACON_REQUEST);

    PRINTF("DEBUG: Beacon Request sent via the broadcast channel. BCP=%d\n",  br_msg->queuelog);
    
    // Broadcast the beacon
    broadcast_send(&c->broadcast_conn);
}

/**
 * \breif Broadcasts a beacon to the one-hop neighbors 
 * \param ptr The opened BCP connection
 * 
 *      Sends a beacon via the opened broadcast channel. This function is usually
 *      called by the beacon_timer to send a beacon message when the broadcast 
 *      channel is free. 
 * 
 */
static void send_beacon(void *ptr)
{
  struct bcp_conn *c = ptr; 
  struct beacon_msg *beacon;

  //Check if the channel is free 
  if(c->busy == false)
    c->busy = true;
  else
    return;

  //Prepare the packet for the beacon 
  prepare_packetbuf();
  packetbuf_set_datalen(sizeof(struct beacon_msg));
  beacon = packetbuf_dataptr();
  memset(beacon, 0, sizeof(beacon));

  // Store the local backpressure level to the backpressure field
  beacon->queuelog = bcp_queue_length(&c->packet_queue); 

  //Update the packet buffer
  //TDOO: Check if this is required
  memcpy(packetbuf_dataptr(), beacon, sizeof(struct beacon_msg));

  // Set the packet type using packetbuf attribute
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                     PACKETBUF_ATTR_PACKET_TYPE_BEACON);

  PRINTF("DEBUG: Sending a beacon via the broadcast channel. BCP=%d\n",  beacon->queuelog);
    
  // Broadcast the beacon
  broadcast_send(&c->broadcast_conn);
}

/**
 * \breif Adds the current packetbuf to the packet queue for the given bcp connection.
 * \param c the bcp connection
 * \return The queue item for the packet. If the packet cannot be added to the queue, this function will return NULL.
 */
 struct bcp_queue_item* push_packet_to_queue(struct bcp_conn *c){

  
     struct bcp_queue_item newRow;
    
    //Packetbuf should not be empty
    if(packetbuf_dataptr() == NULL){
        PRINTF("ERROR: Packetbuf is empty; data cannot be added to the queue\n");
        return NULL;
    }
    
    //Sets the fields of the new record
    newRow.next = NULL;
    newRow.hdr.bcp_backpressure = 0;
    newRow.data_length = packetbuf_datalen();
    memcpy(newRow.data, packetbuf_dataptr(), newRow.data_length);
    
    return bcp_queue_push(&c->packet_queue, &newRow);
    
    
}

 /**
  * \breif Sends user data packet via the broadcast channel for the given bcp connection.
  * \param ptr the bcp connection.
  * 
  *     This function sends the top packet in the packet queue of the given bcp 
  *     connection. It is usually called by the 'send' timer. Each opened bcp 
  *     channel has a timer to send the packets existing in the packet queue. 
  *     
  */
 static void send_packet(void *ptr)
{
    struct bcp_conn *c = ptr;
    struct bcp_queue_item * i;
    
    // If it is busy, just return and wait for the second opportunity
    if(c->busy == true)
      return;
    
    i = bcp_queue_top(&c->packet_queue);
    
    if( i == NULL){
        PRINTF("DEBUG: Packet queue is empty; start beaconing \n");
        // Start beaconing
        if(ctimer_expired(&c->beacon_timer))
          ctimer_reset(&c->beacon_timer);
        return;
    }
    
    
    //Make sure queuebuf is not null
    if(1 != NULL) {
        //Find the best neighbor to send
        rimeaddr_t* neighborAddr = routingtable_find_routing(&c->routing_table);
        
        if(neighborAddr == NULL){
            PRINTF("ERROR: No neighbor has been found; sending a beacon request\n");
            retransmit_callback(c);
            return;
        }
        //Preparing bcp to send a new message
        c->busy = true;
        
        // Stop the beaconing timer
        ctimer_stop(&c->beacon_timer);
        
        
        //Clear the header of the packet
        prepare_packetbuf();
       
        // Set the packet type as data
        packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                         PACKETBUF_ATTR_PACKET_TYPE_DATA);
        
        //Update the header
        packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, neighborAddr); //Set the destination address
       
        //Add backpressure meta data to the header. All these meta data can be overwritten by the extender
        i->hdr.bcp_backpressure = bcp_queue_length(&c->packet_queue); 
        i->hdr.delay = i->hdr.delay + clock_time() - i->hdr.lastProcessTime;
        i->hdr.lastProcessTime = i->hdr.lastProcessTime;
        i->data_length = sizeof(struct bcp_queue_item);
        
        
        //Notify the extender
        if(c->ce != NULL && c->ce->beforeSendingData != NULL)
                        c->ce->beforeSendingData(c, i);
        
        //Copy the data to the packetbuf
        packetbuf_set_datalen(i->data_length);
        memcpy(packetbuf_dataptr(),i, i->data_length);
        
        //Remove pointers
        struct bcp_queue_item* pI = packetbuf_dataptr();
        pI->next = NULL;
       
        c->tx_attempts += 1;
         
        PRINTF("DEBUG: Sending a data packet to node[%d].[%d] (Origin: [%d][%d]), BC=%d,len=%d, data=%s \n", 
                neighborAddr->u8[0], 
                neighborAddr->u8[1],
                pI->hdr.origin.u8[0],
                pI->hdr.origin.u8[1],
                pI->hdr.bcp_backpressure,
                pI->data_length,
                pI->data);
        
        //Send the data packet via the broadcast channel
        broadcast_send(&c->broadcast_conn);
        
        //Notify the extender
        if(c->ce != NULL && c->ce->afterSendingData != NULL)
                        c->ce->afterSendingData(c, i);
        
    }
    
    
}
 /**
  * Sends an ACK to the given neighbor.
  * @param bc the BCP connection.
  * @param to the rime address of the neighbor
  */
 static void send_ack(struct bcp_conn *bc, const rimeaddr_t *to){
    
     struct ack_msg *ack;

     prepare_packetbuf();
     packetbuf_set_datalen(sizeof(struct ack_msg));
     ack = packetbuf_dataptr();
     memset(ack, 0, sizeof(struct ack_msg));
     packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                       PACKETBUF_ATTR_PACKET_TYPE_ACK);
     //We use a unicast channel to send ACKS
     unicast_send(&bc->unicast_conn, to);
 }
 
 /**
  * \breif Stops all the timers of the given BCP connection.
  * \param c an opened BCP connection
  */
 static void stopTimers(struct bcp_conn *c){
     ctimer_stop(&c->send_timer);
     ctimer_stop(&c->beacon_timer);
     ctimer_stop(&c->retransmission_timer);
     //ctimer_stop(&c->delay_timer); 
 }
 
 /**
  * Prepares the packetbuf of the node for a new packet.
  */
 static void prepare_packetbuf(){
     //PRINTF("DEBUG: Prepare Packetbuf\n");
     memset(packetbuf_dataptr(), '\0', packetbuf_datalen()+1);
     packetbuf_clear();
     
 }
 
 /**
  * \breif Notifies users that the current packet in packetbuf is dropped from bcp
  */
 static void packet_dropped(struct bcp_conn *c){
        //Notify user that this packet has been sent
        if(c->cb->dropped != NULL){
            c->cb->dropped(c);        
        }
 }
/******************************************************************************/

/*********************************BCP PUBLIC FUNCTION**************************/
void bcp_open(struct bcp_conn *c, uint16_t channel,
              const struct bcp_callbacks *callbacks)
{
    PRINTF("DEBUG: Opening a bcp connection\n");
    //Set the end user callback function
    c->cb = callbacks;
    //Set the default extender interface 
    c->ce = NULL;
    
    // Initialize the lists containing in the BCP object
    LIST_STRUCT_INIT(c, packet_queue_list);
    LIST_STRUCT_INIT(c, routing_table_list);
    
    //Initialize nested components
    routing_table_init(c);
    weight_estimator_init(c);
    bcp_queue_init(c);
    //Ask queue allocator to allocate memeory for the queue
    bcp_queue_allocator_init(c);
    
    PRINTF("DEBUG: Open a broadcast connection for the data packets and beacons of the BCP\n");
    broadcast_open(&c->broadcast_conn, channel, &broadcast_callbacks);
    channel_set_attributes(channel, attributes);
    
    PRINTF("DEBUG: Open the unicast connection for BCP's ACKs\n");
    unicast_open(&c->unicast_conn, channel + 1, &unicast_callbacks);
    channel_set_attributes(channel + 1, attributes);
   
    //Broadcast the first beacon
    send_beacon(c);
}

void bcp_close(struct bcp_conn *c){
  // Close the broadcast connection
  broadcast_close(&c->broadcast_conn);

  // Close the unicast connection
  unicast_close(&c->unicast_conn);
  
  //Clear both routing table and packet queue
  routingtable_clear(&c->routing_table);
  bcp_queue_clear(&c->packet_queue);
  
  //Stop the timers
  stopTimers(c);
 
}

int bcp_send(struct bcp_conn *c){
    struct bcp_queue_item *qi;
    int result = 0;
    int maxSize = MAX_USER_PACKET_SIZE;
    
    //Check the length of the packet
    if(packetbuf_datalen()> maxSize){
        PRINTF("ERROR: Packet cannot be sent. Data length is bigger than maximum packet size\n");
        packet_dropped(c);
        return 0;
    }
    
    qi = push_packet_to_queue(c);
    PRINTF("DEBUG: Receiving user request to send a data packet, data=%s \n", packetbuf_dataptr() );
    
    if(qi != NULL){
        // Set the origin of the packet
        rimeaddr_copy(&(qi->hdr.origin), &rimeaddr_node_addr);
        qi->hdr.delay = 0;
        qi->hdr.lastProcessTime = clock_time();
        // We have data to send, stop beaconing
        ctimer_stop(&c->beacon_timer);
        result = 1;
    }else{
        packet_dropped(c);
    }
    
    // Reset the send data timer
    if(ctimer_expired(&c->send_timer)) {
      clock_time_t time = SEND_TIME_DELAY;
      ctimer_set(&c->send_timer, time, send_packet, c);
    }

    return result;
}

void bcp_set_sink(struct bcp_conn *c, bool isSink){
    if(isSink)
        PRINTF("DEBUG: This node is set as a sink \n");
    c->isSink = isSink;
}



