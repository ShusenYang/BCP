#include "contiki.h"
#include "net/rime.h"
#include "bcp.h"
#include "bcp_queue.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/**
 * A main file for the 'Queue length vs Pack Generation Rate' test - BCP-21
 */

//Veriables 
static struct bcp_conn bcp;
 static rimeaddr_t addr;
  clock_time_t time_ee = (CLOCK_SECOND * 10)/(1);
  clock_time_t Monitoringtime = CLOCK_SECOND * 10;
  uint16_t avgQueueLength = 0;
 // Timer for triggering a send beacon packet task
  struct ctimer send_data_timer;
  struct ctimer queue_Monitoring_timer;
  uint16_t counter;
  uint16_t counter_recv;
  
  //Callback for receiving a message from bcp
  static void recv_bcp(struct bcp_conn *c, rimeaddr_t * from)
{
  PRINTF("Inside BCP Recv callback. '%s' from node[%d].[%d]; counter=%d\n", 
          (char *)packetbuf_dataptr(), 
          from->u8[0], 
          from->u8[1],
          ++counter_recv);
  
}
  
  void sent_bcp(struct bcp_conn *c){
      PRINTF("Inside BCP sent callback. data='%s' and length=%d\n",
              (char *)packetbuf_dataptr(),
              packetbuf_datalen());
      
  }
  
  
  //Send function
  void sn(void* v){
      //PRINTF("Sending function\n");
       packetbuf_copyfrom("HI", 2);
       //PRINTF("$$$Generating a new packet, data=%s; counter=%d \n", packetbuf_dataptr(), ++counter );
       bcp_send(&bcp); 
       //Reset the  timer
       ctimer_set(&send_data_timer, time_ee, sn, NULL);
  }
  

  static const struct bcp_callbacks bcp_callbacks = { recv_bcp, sent_bcp };

  
/*---------------------------------------------------------------------------*/
PROCESS(main_process, "Main process");
AUTOSTART_PROCESSES(&main_process);
PROCESS_THREAD(main_process, ev, data)
{
  
  PROCESS_BEGIN();
    PRINTF("Hi function\n");
  counter_recv = counter = 0; 
  bcp_open(&bcp, 146, &bcp_callbacks);

  //Set the sink node
  addr.u8[0] = 1;
  addr.u8[1] = 0;
  if(rimeaddr_cmp(&addr, &rimeaddr_node_addr)){
      bcp_set_sink(&bcp, true);
     
      PRINTF("size of %d", sizeof(struct bcp_queue_item)*MAX_PACKET_QUEUE_SIZE);
  }else{
      //Set the sending timer
      ctimer_set(&send_data_timer, time_ee, sn, NULL);
      
  }
  
  PROCESS_END();
}