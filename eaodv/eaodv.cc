#include "protoname.h"
#include "protoname_pkt.h"
#include <random.h>
#include <cmu-trace.h>
#include <iostream>

int hdr_protoname_pkt::offset_;
static class ProtonameHeaderClass : public PacketHeaderClass {

	public:
	ProtonameHeaderClass() : PacketHeaderClass("PacketHeader/Protoname", sizeof(hdr_protoname_pkt)) {
		bind_offset(&hdr_protoname_pkt::offset_);
	}
} class_rtProtoProtoname_hdr;

static class ProtonameClass : public TclClass {
	public:
	ProtonameClass() : TclClass("Agent/Protoname") {}
	TclObject* create(int argc, const char*const* argv) {
		assert(argc == 5);
		return (new Protoname((nsaddr_t) Address::instance().str2addr(argv[4])));
	}
} class_rtProtoProtoname;

void
Protoname_PktTimer::expire(Event* e) {
  	agent_->send_protoname_pkt();
  	agent_->reset_protoname_pkt_timer();
}

Protoname::Protoname(nsaddr_t id) : Agent(PT_PROTONAME), pkt_timer_(this) {
 	bind_bool("accesible_var_", &accesible_var_);
 	ra_addr_ = id;
  	node_ = (MobileNode*)Node::get_node_by_address(id);
}

int
Protoname::command(int argc, const char*const* argv) {
 	if (argc == 2) {
 		if (strcasecmp(argv[1], "start") == 0) {
 			pkt_timer_.resched(0.0);
 			return TCL_OK;
 		}
 		else if (strcasecmp(argv[1], "print_rtable") == 0) {
 			if (logtarget_ != 0) {
 				sprintf(logtarget_->pt_->buffer(), "P %f _%d_ Routing Table", CURRENT_TIME, ra_addr());
 				logtarget_->pt_->dump();
 				rtable_.print(logtarget_);
 			}
 			else {
 				fprintf(stdout, "%f _%d_ If you want to print this routing table "
 				"you must create a trace file in your tcl script", CURRENT_TIME, ra_addr());
 			}
 			return TCL_OK;
 		}
 	}

 	else if (argc == 3) {
 	// Obtains corresponding dmux to carry packets to upper layers
 		if (strcmp(argv[1], "port-dmux") == 0) {
 			dmux_ = (PortClassifier*)TclObject::lookup(argv[2]);
 			if (dmux_ == 0) {
 				fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1], argv[2]);
			 return TCL_ERROR;
 			}
 			return TCL_OK;
 		}
 		// Obtains corresponding tracer
 		else if (strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
 			logtarget_ = (Trace*)TclObject::lookup(argv[2]);
 			if (logtarget_ == 0)
 				return TCL_ERROR;
			return TCL_OK;
 		}
 	}
 	// Pass the command to the base class
 	return Agent::command(argc, argv);
}

void
Protoname::recv(Packet* p, Handler* h) {
	struct hdr_cmn* ch = HDR_CMN(p);
 	struct hdr_ip* ih = HDR_IP(p);

 	if (ih->saddr() == ra_addr()) {
 		// If there exists a loop, must drop the packet
 		if (ch->num_forwards() > 0) {
 			drop(p, DROP_RTR_ROUTE_LOOP);
 			return;
 		}
 		// else if this is a packet I am originating, must add IP header
 		else if (ch->num_forwards() == 0)
 			ch->size() += IP_HDR_LEN;
 	}

 	// If it is a protoname packet, must process it
 	if (ch->ptype() == PT_PROTONAME)
 		recv_protoname_pkt(p);
 	// Otherwise, must forward the packet (unless TTL has reached zero)
 	else {
 		ih->ttl_--;
 		if (ih->ttl_ == 0) {
 			drop(p, DROP_RTR_TTL);
 			return;
 		}
 		forward_data(p);
 	}
}

void
Protoname::recv_protoname_pkt(Packet* p) {
	struct hdr_ip* ih = HDR_IP(p);
	struct hdr_protoname_pkt* ph = HDR_PROTONAME_PKT(p);

 	// All routing messages are sent from and to port RT_PORT,
 	// so we check it.
 	assert(ih->sport() == RT_PORT);
 	assert(ih->dport() == RT_PORT);

 	/* ... processing of protoname packet ... */

 	// Release resources
 	Packet::free(p);
}

void
Protoname::send_protoname_pkt() {
	Packet* p = allocpkt();
 	struct hdr_cmn* ch = HDR_CMN(p);
 	struct hdr_ip* ih = HDR_IP(p);
 	struct hdr_protoname_pkt* ph = HDR_PROTONAME_PKT(p);

 	ph->pkt_src() = ra_addr();
 	ph->pkt_len() = 7;
 	ph->pkt_seq_num() = seq_num_++;

 	ch->ptype() = PT_PROTONAME;
 	ch->direction() = hdr_cmn::DOWN;
 	ch->size() = IP_HDR_LEN + ph->pkt_len();
 	ch->error() = 0;
 	ch->next_hop() = IP_BROADCAST;
 	ch->addr_type() = NS_AF_INET;

 	ih->saddr() = ra_addr();
	ih->daddr() = IP_BROADCAST;
 	ih->sport() = RT_PORT;
 	ih->dport() = RT_PORT;
 	ih->ttl() = IP_DEF_TTL;

 	Scheduler::instance().schedule(target_, p, JITTER);
}

void
Protoname::reset_protoname_pkt_timer() {
	pkt_timer_.resched((double)5.0);
}

void
Protoname::forward_data(Packet* p) {
      struct hdr_cmn* ch = HDR_CMN(p);
      struct hdr_ip* ih = HDR_IP(p);
  
      if (ch->direction() == hdr_cmn::UP &&
          ((u_int32_t)ih->daddr() == IP_BROADCAST || ih->daddr() == ra_addr())) {
          dmux_->recv(p, 0);
          return;
     }
     else {
         ch->direction() = hdr_cmn::DOWN;
         ch->addr_type() = NS_AF_INET;
         if ((u_int32_t)ih->daddr() == IP_BROADCAST)
             ch->next_hop() = IP_BROADCAST;
         else {
             nsaddr_t next_hop = rtable_.lookup(ih->daddr());
             if (next_hop == IP_BROADCAST) {
                 debug("%f: Agent %d can not forward a packet destined to %d\n",
                     CURRENT_TIME,
                     ra_addr(),
                     ih->daddr());
                 drop(p, DROP_RTR_NO_ROUTE);
                 return;
             }
             else
                 ch->next_hop() = next_hop;
         }
         Scheduler::instance().schedule(target_, p, 0.0);
     }
}
