/**
 * \file
 *      This interface can be extended by any component wishing to extend the 
 *      functionality of BCP implementation.
 *    
 *      
 */
#ifndef BCP_EXTENDER_H
#define	BCP_EXTENDER_H



/** 
 *  An interface which can be attached to an opened BCP connection in order to 
 *  extends the functionality of BCP. All the callbacks functions are called by 
 *  the BCP when corresponding events occur. 
 */
struct bcp_extender{ 
  /**
   * Called by BCP before broadcasting a data packet
   */
  void (*beforeSendingData)(struct bcp_conn *c,  struct bcp_queue_item* itm);
  /**
   * Called by BCP after broadcasting a data packet
   */
  void (*afterSendingData)(struct bcp_conn *c,  struct bcp_queue_item* itm);
  /**
   * Called by BCP after successfully receiving a new data packet
   */
  void (*onReceivingData)(struct bcp_conn *c, struct bcp_queue_item* itm);
};

#endif	/* BCP_EXTENDER_H */

