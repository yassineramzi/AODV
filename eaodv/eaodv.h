#ifndef __protoname_h__
#define __protoname_h__

#include "protoname_pkt.h"
#include "protoname_rtable.h"
#include <agent.h>
#include <packet.h>
#include <trace.h>
#include <timer-handler.h>
#include <random.h>
#include <classifier-port.h>
#include <mobilenode.h>
#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "ip.h"
#include "delay.h"

#define CURRENT_TIME Scheduler::instance().clock()
#define JITTER (Random::uniform()*0.5)

class Protoname; // forward declaration

/* Timers */

class Protoname_PktTimer : public TimerHandler {
    public:
    Protoname_PktTimer(Protoname* agent) : TimerHandler() {
    	agent_ = agent;
    }
    
    protected:
    Protoname* agent_;
    virtual void expire(Event* e);
};

/* Agent */

class Protoname : public Agent {

   /* Friends */
   friend class Protoname_PktTimer;

   /* Private members */
   nsaddr_t ra_addr_;
   //protoname_state state_;
   protoname_rtable rtable_;
   int accesible_var_;
   u_int8_t seq_num_;

   protected:

   MobileNode* node_;
   PortClassifier* dmux_; // For passing packets up to agents.
   Trace* logtarget_; // For logging.
   Protoname_PktTimer pkt_timer_; // Timer for sending packets.

   inline nsaddr_t& ra_addr() { return ra_addr_; }
   //inline protoname_state& state() { return state_; }
   inline int& accessible_var() { return accesible_var_; }
 
   void forward_data(Packet*);
   void recv_protoname_pkt(Packet*);
   void send_protoname_pkt();
  
   void reset_protoname_pkt_timer();
 
   public:

   Protoname(nsaddr_t);
   int command(int, const char*const*);
   void recv(Packet*, Handler*);
   //void mac_failed(Packet*);
};

#endif
