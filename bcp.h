/**
 * \file
 *         Header file for Backpressure Routing
 *
 */

#ifndef __BCP_H__
#define __BCP_H__

#include <stdbool.h>
#include "bcp-config.h"
#include "bcp_routing_table.h"
#include "bcp_queue.h"
#include "bcp_extend.h"
#include "bcp_weight_estimator.h"


#define BCP_ATTRIBUTES 	{ PACKETBUF_ADDR_ERECEIVER,     PACKETBUF_ADDRSIZE }, \
						{ PACKETBUF_ATTR_PACKET_ID,   PACKETBUF_ATTR_BIT * 16 }, \
						{ PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BIT * 3 }, \
                            BROADCAST_ATTRIBUTES

/**
 * \brief      A structure with callback functions for a bcp connection.
 *
 *             This structure holds a list of callback functions used
 *             with a bcp connection. The functions are called when
 *             events occur on the connection at runtime.
 *
 */
struct bcp_callbacks {
   /**
   * Called when a packet is received on the connection.
   */
  void (*recv)(struct bcp_conn *c, rimeaddr_t * from);
  
  /**
   * Called when a packet is sent on the bcp connection. Use packetbuf library to 
   * check packet details
   */
  void (* sent)(struct bcp_conn *c);
  
  /**
   * Called when a packet is dropped. Use packetbuf library to check packet 
   * details.
   */
  void (* dropped)(struct bcp_conn *c);
};

struct bcp_conn {
  //Used to broadcast user data packets and beacons
  struct broadcast_conn broadcast_conn;

  // Used for sending ACKS
  struct unicast_conn unicast_conn;

  // End user Callbacks
  const struct bcp_callbacks *cb;
  
  //Component Extender - SPI
  const struct bcp_extender * ce;

  //Flag to indicate whether the bcp channel is busy or not
  bool busy;
  
  //Flag to indicate whether the node is sink or not
  bool isSink;
  
  // Timer for triggering a send data packet task
  struct ctimer send_timer;

  // Timer for triggering a send beacon packet task
  struct ctimer beacon_timer;
  
  // Timer for retransmissions
  struct ctimer retransmission_timer;
  
  // Timer for measuring the amount of time took to send the current packet
  struct timer delay_timer;
  
  //Queue for the waiting packets
  LIST_STRUCT(packet_queue_list);
  struct bcp_queue packet_queue;

  // Routing table of neighbors
  LIST_STRUCT(routing_table_list);
  struct routingtable routing_table;
  
  //Counts tx attempts achieved so far to send the current packet 
  uint16_t tx_attempts;
  
  
};

/**
* \brief	Opens a bcp connection.
* \param c	A pointer to a struct bcp_conn
* \param channel The channel number to be used for this connection.
* \param cb   A pointer to the callbacks used for this connection
*		
*	      This function opens a bcp connection on the
*             specified channel. The BCP connection will use two channel ports 
*            (channel, and channel+1). The callbacks are called when a
*             packet is received (check \ref "struct bcp_callbacks").
*
*/
void bcp_open(struct bcp_conn *c, uint16_t channel,
              const struct bcp_callbacks *callbacks
              );

/**
* \brief      Close an opened bcp connection
* \param c    A pointer to a struct bcp_conn that has previously been opened with bcp_open().
*
*             This function closes a bcp connection that has
*             previously been opened with bcp_open().
*/
void bcp_close(struct bcp_conn *c);


/**
* \brief      Send a packet using the given bcp connection.
* \param c    A pointer to a struct bcp_conn that has previously been opened with bcp_open().
* \retval     Non-zero if the packet can be sent, zero otherwise.
*             
*	      This function sends a packet from the packetbuf on the
*             given bcp connection. The packet must be present in the packetbuf
*             before this function is called.
*
*             The parameter c must point to a bcp connection that
*             must have previously been set up with bcp_open().
*
*/
int bcp_send(struct bcp_conn *c);


/**
 * \brief Sets whether the current node is sink for the given bcp connection or not.
 * \param c the opened bcp connection
 * \param isSink true if the current node is sink. Otherwise, false.
 */
void bcp_set_sink(struct bcp_conn *c, bool isSink);

#endif /* __BCP_H__ */