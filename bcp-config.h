#ifndef __BCP_CONFIG_H__
#define __BCP_CONFIG_H__

//custom Packet types used in this API
#define PACKETBUF_ATTR_PACKET_TYPE_BEACON    5
#define PACKETBUF_ATTR_PACKET_TYPE_BEACON_REQUEST    6

//RAM consumption parameters
#define MAX_PACKET_QUEUE_SIZE 	100
#define MAX_ROUTING_TABLE_SIZE 	40
#define USER_PACKET_CONF_SIZE 4

#ifdef USER_PACKET_CONF_SIZE
  #define MAX_USER_PACKET_SIZE USER_PACKET_CONF_SIZE
#else
 #define MAX_USER_PACKET_SIZE PACKETBUF_SIZE
#endif

//Delays parameters
//Time between beacons
#define BEACON_TIME CLOCK_SECOND * 5 
//General delay before sending a packet
#define SEND_TIME_DELAY     CLOCK_SECOND * 0.05f	// 50 ms
//Time
#define DELAY_TIME	    CLOCK_SECOND * 120
#define RETX_TIME           CLOCK_SECOND * 2

//Other
#define LINK_LOSS_ALPHA   90  // Decay parameter. 90 = 90% weight of previous link loss Estimate
#define LINK_LOSS_V       2   // V Value used to weight link losses in Lyapunov Calculation
#define LINK_EST_ALPHA    9   // Decay parameter. 9 = 90% weight of previous rate Estimation

#endif
