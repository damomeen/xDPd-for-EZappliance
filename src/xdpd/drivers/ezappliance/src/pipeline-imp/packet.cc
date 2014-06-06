//Must be the first one
#include "packet.h"

#include <inttypes.h>
#include <rofl/datapath/pipeline/physical_switch.h>
#include <rofl/datapath/pipeline/common/datapacket.h>
#include <rofl/datapath/pipeline/openflow/of_switch.h>
#include <rofl/datapath/pipeline/platform/packet.h>
#include <rofl/datapath/afa/openflow/openflow1x/of1x_cmm.h>
#include <rofl/datapath/pipeline/common/ipv6_exthdr.h>
#include <rofl/common/utils/c_logger.h>

//Frames
//#include <rofl/common/protocols/fetherframe.h>
//#include <rofl/common/protocols/fvlanframe.h>
//#include <rofl/common/protocols/fmplsframe.h>
//#include <rofl/common/protocols/farpv4frame.h>
//#include <rofl/common/protocols/fipv4frame.h>
//#include <rofl/common/protocols/ficmpv4frame.h>
//#include <rofl/common/protocols/fipv6frame.h>
//#include <rofl/common/protocols/ficmpv6frame.h>
//#include <rofl/common/protocols/fudpframe.h>
//#include <rofl/common/protocols/ftcpframe.h>
//#include <rofl/common/protocols/fsctpframe.h>
//#include <rofl/common/protocols/fpppoeframe.h>
//#include <rofl/common/protocols/fpppframe.h>
//#include <rofl/common/protocols/fgtpuframe.h>

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../io/datapacketx86.h"
#include "../io/bufferpool.h"
#include "../ezappliance/ez_packet_channel.h"

#include "../io/packet_classifiers/c_pktclassifier/c_pktclassifier.h"

//using namespace rofl;
using namespace xdpd::gnu_linux;

/* Cloning of the packet */
STATIC_PACKET_INLINE__
void clone_pkt_contents(datapacket_t* src, datapacket_t* dst){
	
	datapacketx86 *pack_src = (datapacketx86*)src->platform_state;
	datapacketx86 *pack_dst = (datapacketx86*)dst->platform_state;
	pack_dst->init(pack_src->get_buffer(), pack_src->get_buffer_length(), pack_src->lsw, pack_src->in_port, pack_src->in_phy_port, true, true);

	//copy checksum flags
	pack_dst->ipv4_recalc_checksum = pack_src->ipv4_recalc_checksum;
	pack_dst->icmpv4_recalc_checksum = pack_src->icmpv4_recalc_checksum;
	pack_dst->tcp_recalc_checksum = pack_src->tcp_recalc_checksum;
	pack_dst->udp_recalc_checksum = pack_src->udp_recalc_checksum;
}


/*
* ROFL-Pipeline packet mangling platform API implementation
*/

//Getters
STATIC_PACKET_INLINE__
uint32_t platform_packet_get_size_bytes(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	return pack->get_buffer_length();
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_port_in(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return 0;
	return pack->in_port;
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_phy_port_in(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return 0;
	return pack->in_phy_port;
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_eth_dst(datapacket_t * const pkt)
{

	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
        
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers,0))) return 0;

	return get_ether_dl_dst(get_ether_hdr(pack->headers,0));
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_eth_src(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
        
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers,0))) return 0;
        
	return get_ether_dl_src(get_ether_hdr(pack->headers,0));
}


STATIC_PACKET_INLINE__
uint16_t platform_packet_get_eth_type(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers,0))) return 0;
	
	if (get_vlan_hdr(pack->headers,-1) != NULL)
		return get_vlan_type(get_vlan_hdr(pack->headers,-1));
	
	return get_ether_type(get_ether_hdr(pack->headers,0));
}

STATIC_PACKET_INLINE__
bool
platform_packet_has_vlan(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack)
		return false;
	return (NULL != get_vlan_hdr(pack->headers, 0));
}
STATIC_PACKET_INLINE__
uint16_t platform_packet_get_vlan_vid(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_vlan_hdr(pack->headers,0))) return 0;
	
	return get_vlan_id(get_vlan_hdr(pack->headers, 0))&0xFFF;
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_vlan_pcp(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_vlan_hdr(pack->headers, 0))) return 0;
	return get_vlan_pcp(get_vlan_hdr(pack->headers, 0))&0x07;
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_arp_opcode(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return 0;
	return get_arpv4_opcode(get_arpv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_arp_sha(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return 0;
	return get_arpv4_dl_src(get_arpv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_arp_spa(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return 0;
	return get_arpv4_ip_src(get_arpv4_hdr(pack->headers,0));
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_arp_tha(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return 0;
	return get_arpv4_dl_dst(get_arpv4_hdr(pack->headers ,0));
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_arp_tpa(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return 0;
	return get_arpv4_ip_dst(get_arpv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_ip_ecn(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return 0;
	if (NULL != get_ipv4_hdr(pack->headers, 0))
		return get_ipv4_ecn(get_ipv4_hdr(pack->headers, 0))&0xFF;
	if (NULL != get_ipv6_hdr(pack->headers, 0))
		return get_ipv6_ecn(get_ipv6_hdr(pack->headers, 0));
	return 0;
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_ip_dscp(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return 0;
	if (NULL != get_ipv4_hdr(pack->headers, 0))
		return get_ipv4_dscp(get_ipv4_hdr(pack->headers, 0))&0xFF;
	if (NULL != get_ipv6_hdr(pack->headers, 0))
		return get_ipv6_dscp(get_ipv6_hdr(pack->headers, 0));
	return 0;
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_ip_proto(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return 0;
	if (NULL != get_ipv4_hdr(pack->headers, 0))
		return get_ipv4_proto(get_ipv4_hdr(pack->headers, 0))&0xFF;
	if (NULL != get_ipv6_hdr(pack->headers, 0))
		return get_ipv6_next_header(get_ipv6_hdr(pack->headers, 0))&0xFF;
	return 0;
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_ipv4_src(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv4_hdr(pack->headers, 0))) return 0;

	return get_ipv4_src(get_ipv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_ipv4_dst(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv4_hdr(pack->headers, 0))) return 0;

	return get_ipv4_dst(get_ipv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_tcp_dst(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_tcp_hdr(pack->headers, 0))) return 0;
	return get_tcp_dport(get_tcp_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_tcp_src(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_tcp_hdr(pack->headers, 0))) return 0;
	return get_tcp_sport(get_tcp_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_udp_dst(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_udp_hdr(pack->headers, 0))) return 0;
	return get_udp_dport(get_udp_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_udp_src(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_udp_hdr(pack->headers, 0))) return 0;
	return get_udp_sport(get_udp_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_sctp_dst(datapacket_t *const pkt){
	//TODO: add implementation when supported
	return 0x0;
}
STATIC_PACKET_INLINE__
uint16_t platform_packet_get_sctp_src(datapacket_t *const pkt){
	//TODO: add implementation when supported
	return 0x0;
}


STATIC_PACKET_INLINE__
uint8_t platform_packet_get_icmpv4_type(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv4_hdr(pack->headers, 0))) return 0;
	return get_icmpv4_type(get_icmpv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_icmpv4_code(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv4_hdr(pack->headers, 0))) return 0;
	return get_icmpv4_code(get_icmpv4_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint128__t platform_packet_get_ipv6_src(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pack) || (NULL == get_ipv6_hdr(pack->headers, 0))){
		uint128__t ipv6_src = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}; //memset 0?
		return ipv6_src;
	}
	return get_ipv6_src(get_ipv6_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint128__t platform_packet_get_ipv6_dst(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pack) || (NULL == get_ipv6_hdr(pack->headers, 0))){
		uint128__t ipv6_dst = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}; //memset 0?
		return ipv6_dst;
	}
	return get_ipv6_dst(get_ipv6_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_ipv6_flabel(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pack) || (NULL == get_ipv6_hdr(pack->headers, 0))) return 0;
	return get_ipv6_flow_label(get_ipv6_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint128__t platform_packet_get_ipv6_nd_target(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	uint128__t ipv6_nd_target = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}; //memset 0?
	void *icmpv6_hdr;
	if ( (NULL == pack) ||
		(NULL == (icmpv6_hdr=get_icmpv6_hdr(pack->headers, 0)))
		)	return ipv6_nd_target;
	
	return get_icmpv6_neighbor_taddr(icmpv6_hdr);
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_ipv6_nd_sll(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	void* lla_opt_hdr;
	if( (NULL == pack) ||
		(NULL == (lla_opt_hdr = get_icmpv6_opt_lladr_source_hdr(pack->headers, 0)))
		)	return 0;
	
	return get_icmpv6_ll_saddr(lla_opt_hdr);
}

STATIC_PACKET_INLINE__
uint64_t platform_packet_get_ipv6_nd_tll(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	void* lla_opt_hdr;
	if( (NULL == pack) ||
		(NULL == (lla_opt_hdr = get_icmpv6_opt_lladr_target_hdr(pack->headers, 0)))
		)	return 0;
	
	return get_icmpv6_ll_taddr(lla_opt_hdr);
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_ipv6_exthdr(datapacket_t * const pkt)
{
	uint64_t mask=0x0;
	/* TODO EXTENSION HEADERS NOT YET IMPLEMENTED	
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pkt) || (NULL == get_ipv6_hdr(pack->headers, 0))) return 0;
	//return get_ipv6_hdr(pack->headers, 0)->get_ipv6_ext_hdr();
	try{
		get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT);
		mask |= IPV6_EH_NONEXT;
	}catch(...){}
	
	//if(get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT)) //encripted
	//if(get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT)) //authentication
	//if(get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT)) //destination headers
	try{
		get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_FRAG);
		mask |= IPV6_EH_FRAG;
	}catch(...){}
	
	try{
		get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_ROUTE);
		mask |= IPV6_EH_ROUTER;
	}catch(...){}
	
	try{
		get_ipv6_hdr(pack->headers, 0)->get_ext_hdr(fipv6frame::IPPROTO_IPV6_HOPOPT);
		mask |= IPV6_EH_HOP;
	}catch(...){}
	//if(get_ipv6_hdr(pack->headers, 0)->get_ipv6_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT)) //unexpected repeats
	//if(get_ipv6_hdr(pack->headers, 0)->get_ipv6_ext_hdr(fipv6frame::IPPROTO_IPV6_NONXT)) //unexpected sequencing
	*/
	return mask;
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_icmpv6_type(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pack) || (NULL == get_icmpv6_hdr(pack->headers, 0))) return 0;
	return get_icmpv6_type(get_icmpv6_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_icmpv6_code(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if((NULL==pack) || (NULL == get_icmpv6_hdr(pack->headers, 0))) return 0;
	return get_icmpv6_code(get_icmpv6_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_mpls_label(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return 0;
	return get_mpls_label(get_mpls_hdr(pack->headers, 0))&0x000FFFFF;
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_mpls_tc(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return 0;
	return get_mpls_tc(get_mpls_hdr(pack->headers, 0))&0x07;
}

STATIC_PACKET_INLINE__
bool
platform_packet_get_mpls_bos(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return 0;
	return get_mpls_bos(get_mpls_hdr(pack->headers, 0))&0x01;
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_pbb_isid(datapacket_t *const pkt){
	//TODO: add implementation when supported
	return 0x0;
}

//Tunnel id
STATIC_PACKET_INLINE__
uint64_t platform_packet_get_tunnel_id(datapacket_t *const pkt){
	//TODO: add implementation when supported
	return 0x0ULL;
}


STATIC_PACKET_INLINE__
uint8_t platform_packet_get_pppoe_code(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return 0;
	return get_pppoe_code(get_pppoe_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_pppoe_type(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return 0;
	return get_pppoe_type(get_pppoe_hdr(pack->headers, 0))&0x0F;
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_pppoe_sid(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return 0;
	return get_pppoe_sessid(get_pppoe_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint16_t platform_packet_get_ppp_proto(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ppp_hdr(pack->headers, 0))) return 0;
	return get_ppp_prot(get_ppp_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint8_t platform_packet_get_gtp_msg_type(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_gtpu_hdr(pack->headers, 0))) return 0;
	return get_gtpu_msg_type(get_gtpu_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
uint32_t platform_packet_get_gtp_teid(datapacket_t * const pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;

	TM_STAMP_STAGE(pkt, TM_S4);

	if ((NULL == pack) || (NULL == get_gtpu_hdr(pack->headers, 0))) return 0;
	return get_gtpu_teid(get_gtpu_hdr(pack->headers, 0));
}


//Actions
STATIC_PACKET_INLINE__
void platform_packet_copy_ttl_in(datapacket_t* pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	// TODO
}

STATIC_PACKET_INLINE__
void platform_packet_pop_vlan(datapacket_t* pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	pop_vlan(pkt, pack->headers);
}

STATIC_PACKET_INLINE__
void platform_packet_pop_mpls(datapacket_t* pkt, uint16_t ether_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	pop_mpls(pkt, pack->headers, ether_type);
}

STATIC_PACKET_INLINE__
void platform_packet_pop_pppoe(datapacket_t* pkt, uint16_t ether_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	pop_pppoe(pkt, pack->headers, ether_type);
}

STATIC_PACKET_INLINE__
void platform_packet_push_pppoe(datapacket_t* pkt, uint16_t ether_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	push_pppoe(pkt, pack->headers, ether_type);
}

STATIC_PACKET_INLINE__
void platform_packet_push_mpls(datapacket_t* pkt, uint16_t ether_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	push_mpls(pkt, pack->headers, ether_type);
}

STATIC_PACKET_INLINE__
void platform_packet_push_vlan(datapacket_t* pkt, uint16_t ether_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	push_vlan(pkt, pack->headers, ether_type);
}

STATIC_PACKET_INLINE__
void platform_packet_copy_ttl_out(datapacket_t* pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	// TODO
}

STATIC_PACKET_INLINE__
void platform_packet_dec_nw_ttl(datapacket_t* pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack)
		return;
	if(NULL != get_ipv4_hdr(pack->headers, 0)){
		dec_ipv4_ttl(get_ipv4_hdr(pack->headers, 0));
		pack->ipv4_recalc_checksum = true;
	}
	if(NULL != get_ipv6_hdr(pack->headers, 0)){
		dec_ipv6_hop_limit(get_ipv6_hdr(pack->headers, 0));
	}
}

STATIC_PACKET_INLINE__
void platform_packet_dec_mpls_ttl(datapacket_t* pkt)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return;
	dec_mpls_ttl(get_mpls_hdr(pack->headers, 0));
}

STATIC_PACKET_INLINE__
void platform_packet_set_mpls_ttl(datapacket_t* pkt, uint8_t new_ttl)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return;
	set_mpls_ttl(get_mpls_hdr(pack->headers, 0),new_ttl);
}

STATIC_PACKET_INLINE__
void platform_packet_set_nw_ttl(datapacket_t* pkt, uint8_t new_ttl)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	if (NULL != get_ipv4_hdr(pack->headers, 0)){
		set_ipv4_ttl(get_ipv4_hdr(pack->headers, 0), new_ttl);
		pack->ipv4_recalc_checksum = true;
	}
	if (NULL != get_ipv6_hdr(pack->headers, 0)){
		set_ipv6_hop_limit(get_ipv6_hdr(pack->headers, 0), new_ttl);
	}
}

STATIC_PACKET_INLINE__
void platform_packet_set_queue(datapacket_t* pkt, uint32_t queue)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;

	pack->output_queue = queue;	
}

STATIC_PACKET_INLINE__
void platform_packet_set_eth_dst(datapacket_t* pkt, uint64_t eth_dst)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers, 0))) return;
	set_ether_dl_dst(get_ether_hdr(pack->headers, 0), eth_dst);
}

STATIC_PACKET_INLINE__
void platform_packet_set_eth_src(datapacket_t* pkt, uint64_t eth_src)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers, 0))) return;
	set_ether_dl_src(get_ether_hdr(pack->headers, 0), eth_src);
}

STATIC_PACKET_INLINE__
void platform_packet_set_eth_type(datapacket_t* pkt, uint16_t eth_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ether_hdr(pack->headers, 0))) return;
	set_ether_type(get_ether_hdr(pack->headers, 0), eth_type);
}

STATIC_PACKET_INLINE__
void platform_packet_set_vlan_vid(datapacket_t* pkt, uint16_t vlan_vid)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_vlan_hdr(pack->headers, 0))) return;
	set_vlan_id(get_vlan_hdr(pack->headers, 0), vlan_vid);
}

STATIC_PACKET_INLINE__
void platform_packet_set_vlan_pcp(datapacket_t* pkt, uint8_t vlan_pcp)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_vlan_hdr(pack->headers, 0))) return;
	set_vlan_pcp(get_vlan_hdr(pack->headers, 0), vlan_pcp);
}

STATIC_PACKET_INLINE__
void platform_packet_set_arp_opcode(datapacket_t* pkt, uint16_t arp_opcode)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return;
	set_arpv4_opcode(get_arpv4_hdr(pack->headers, 0), arp_opcode);
}

STATIC_PACKET_INLINE__
void platform_packet_set_arp_sha(datapacket_t* pkt, uint64_t arp_sha)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return;
	set_arpv4_dl_src(get_arpv4_hdr(pack->headers, 0), arp_sha);
}

STATIC_PACKET_INLINE__
void platform_packet_set_arp_spa(datapacket_t* pkt, uint32_t arp_spa)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return;
	set_arpv4_ip_src(get_arpv4_hdr(pack->headers, 0), arp_spa);
}

STATIC_PACKET_INLINE__
void platform_packet_set_arp_tha(datapacket_t* pkt, uint64_t arp_tha)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return;
	set_arpv4_dl_dst(get_arpv4_hdr(pack->headers, 0), arp_tha);
}

STATIC_PACKET_INLINE__
void platform_packet_set_arp_tpa(datapacket_t* pkt, uint32_t arp_tpa)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_arpv4_hdr(pack->headers, 0))) return;
	set_arpv4_ip_dst(get_arpv4_hdr(pack->headers, 0), arp_tpa);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ip_dscp(datapacket_t* pkt, uint8_t ip_dscp)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	if (NULL != get_ipv4_hdr(pack->headers, 0)) {
		set_ipv4_dscp(get_ipv4_hdr(pack->headers, 0), ip_dscp);
		pack->ipv4_recalc_checksum = true;
	}
	if (NULL != get_ipv6_hdr(pack->headers, 0)) {
		set_ipv6_dscp(get_ipv6_hdr(pack->headers, 0), ip_dscp);
	}
}

STATIC_PACKET_INLINE__
void platform_packet_set_ip_ecn(datapacket_t* pkt, uint8_t ip_ecn)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	if (NULL != get_ipv4_hdr(pack->headers, 0)){
		set_ipv4_ecn(get_ipv4_hdr(pack->headers, 0), ip_ecn);
		pack->ipv4_recalc_checksum = true;
	}
	if (NULL != get_ipv6_hdr(pack->headers, 0)){
		set_ipv6_ecn(get_ipv6_hdr(pack->headers, 0), ip_ecn);
	}
}

STATIC_PACKET_INLINE__
void platform_packet_set_ip_proto(datapacket_t* pkt, uint8_t ip_proto)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if (NULL == pack) return;
	if (NULL != get_ipv4_hdr(pack->headers, 0)) {
		set_ipv4_proto(get_ipv4_hdr(pack->headers, 0), ip_proto);
		pack->ipv4_recalc_checksum = true;
	}
	if (NULL != get_ipv6_hdr(pack->headers, 0)) {
		set_ipv6_next_header(get_ipv6_hdr(pack->headers, 0), ip_proto);
	}
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv4_src(datapacket_t* pkt, uint32_t ip_src)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv4_hdr(pack->headers, 0))) return;
	set_ipv4_src(get_ipv4_hdr(pack->headers, 0), ip_src);
	pack->ipv4_recalc_checksum = true;
	pack->tcp_recalc_checksum = true;
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv4_dst(datapacket_t* pkt, uint32_t ip_dst)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv4_hdr(pack->headers, 0))) return;
	set_ipv4_dst(get_ipv4_hdr(pack->headers, 0), ip_dst);
	pack->ipv4_recalc_checksum = true;
	pack->tcp_recalc_checksum = true;
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_src(datapacket_t* pkt, uint128__t ipv6_src)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv6_hdr(pack->headers, 0))) return;
	set_ipv6_src(get_ipv6_hdr(pack->headers, 0), ipv6_src);
	pack->tcp_recalc_checksum = true;
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_dst(datapacket_t* pkt, uint128__t ipv6_dst)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv6_hdr(pack->headers, 0))) return;
	set_ipv6_dst(get_ipv6_hdr(pack->headers, 0), ipv6_dst);
	pack->tcp_recalc_checksum = true;
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_flabel(datapacket_t* pkt, uint64_t ipv6_flabel)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ipv6_hdr(pack->headers, 0))) return;
	set_ipv6_flow_label(get_ipv6_hdr(pack->headers, 0), ipv6_flabel);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_nd_target(datapacket_t* pkt, uint128__t ipv6_nd_target)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv6_hdr(pack->headers, 0))) return;
	set_icmpv6_neighbor_taddr(get_icmpv6_hdr(pack->headers, 0), ipv6_nd_target);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_nd_sll(datapacket_t* pkt, uint64_t ipv6_nd_sll)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	void *lla_opt_hdr;
	if ( (NULL == pack) || 
		NULL == (lla_opt_hdr = get_icmpv6_opt_lladr_source_hdr(pack->headers, 0))
		) return;
	set_icmpv6_ll_saddr(lla_opt_hdr, ipv6_nd_sll);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_nd_tll(datapacket_t* pkt, uint64_t ipv6_nd_tll)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	void *lla_opt_hdr;
	if ( (NULL == pack) || 
		(NULL == (lla_opt_hdr = get_icmpv6_opt_lladr_target_hdr(pack->headers, 0)))
		) return;
	set_icmpv6_ll_taddr(lla_opt_hdr,ipv6_nd_tll);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ipv6_exthdr(datapacket_t* pkt, uint16_t ipv6_exthdr)
{
	/*TODO Extension headers not yet implemented*/
}

STATIC_PACKET_INLINE__
void platform_packet_set_icmpv6_type(datapacket_t* pkt, uint8_t icmpv6_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv6_hdr(pack->headers, 0))) return;
	set_icmpv6_type(get_icmpv6_hdr(pack->headers, 0), icmpv6_type);
}

STATIC_PACKET_INLINE__
void platform_packet_set_icmpv6_code(datapacket_t* pkt, uint8_t icmpv6_code)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv6_hdr(pack->headers, 0))) return;
	set_icmpv6_code(get_icmpv6_hdr(pack->headers, 0), icmpv6_code);
}

STATIC_PACKET_INLINE__
void platform_packet_set_tcp_src(datapacket_t* pkt, uint16_t tcp_src)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_tcp_hdr(pack->headers, 0))) return;
	set_tcp_sport(get_tcp_hdr(pack->headers, 0), tcp_src);
	pack->tcp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_tcp_dst(datapacket_t* pkt, uint16_t tcp_dst)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_tcp_hdr(pack->headers, 0))) return;
	set_tcp_dport(get_tcp_hdr(pack->headers, 0), tcp_dst);
	pack->tcp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_udp_src(datapacket_t* pkt, uint16_t udp_src)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_udp_hdr(pack->headers, 0))) return;
	set_udp_sport(get_udp_hdr(pack->headers, 0), udp_src);
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_udp_dst(datapacket_t* pkt, uint16_t udp_dst)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_udp_hdr(pack->headers, 0))) return;
	set_udp_dport(get_udp_hdr(pack->headers, 0), udp_dst);
	pack->udp_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_sctp_src(datapacket_t* pkt, uint16_t sctp_src)
{
	//TODO: implement
}

STATIC_PACKET_INLINE__
void platform_packet_set_sctp_dst(datapacket_t* pkt, uint16_t sctp_dst)
{
	//TODO: implement
}

STATIC_PACKET_INLINE__
void platform_packet_set_icmpv4_type(datapacket_t* pkt, uint8_t type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv4_hdr(pack->headers, 0))) return;
	set_icmpv4_type(get_icmpv4_hdr(pack->headers, 0), type);
	pack->icmpv4_recalc_checksum = true;
}

STATIC_PACKET_INLINE__
void platform_packet_set_icmpv4_code(datapacket_t* pkt, uint8_t code)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_icmpv4_hdr(pack->headers, 0))) return;
	set_icmpv4_code(get_icmpv4_hdr(pack->headers, 0), code);
	pack->icmpv4_recalc_checksum = true;
}


STATIC_PACKET_INLINE__
void platform_packet_set_mpls_label(datapacket_t* pkt, uint32_t label)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return;
	set_mpls_label(get_mpls_hdr(pack->headers, 0), label);
}

STATIC_PACKET_INLINE__
void platform_packet_set_mpls_tc(datapacket_t* pkt, uint8_t tc)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return;
	set_mpls_tc(get_mpls_hdr(pack->headers, 0), tc);
}
STATIC_PACKET_INLINE__
void platform_packet_set_mpls_bos(datapacket_t* pkt, bool bos)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_mpls_hdr(pack->headers, 0))) return;
	set_mpls_bos(get_mpls_hdr(pack->headers, 0), bos);
}
STATIC_PACKET_INLINE__
void platform_packet_set_pbb_isid(datapacket_t*pkt, uint32_t pbb_isid)
{
	//TODO: implement
}
STATIC_PACKET_INLINE__
void platform_packet_set_tunnel_id(datapacket_t*pkt, uint64_t tunnel_id)
{
	//TODO: implement
}
STATIC_PACKET_INLINE__
void platform_packet_pop_pbb(datapacket_t* pkt, uint16_t ether_type)
{
	//TODO: implement
}
STATIC_PACKET_INLINE__
void platform_packet_push_pbb(datapacket_t* pkt, uint16_t ether_type)
{
	//TODO: implement
}
STATIC_PACKET_INLINE__
void platform_packet_set_pppoe_type(datapacket_t* pkt, uint8_t type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return;
	set_pppoe_type(get_pppoe_hdr(pack->headers, 0), type);
}

STATIC_PACKET_INLINE__
void platform_packet_set_pppoe_code(datapacket_t* pkt, uint8_t code)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return;
	set_pppoe_code(get_pppoe_hdr(pack->headers, 0), code);
}

STATIC_PACKET_INLINE__
void platform_packet_set_pppoe_sid(datapacket_t* pkt, uint16_t sid)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_pppoe_hdr(pack->headers, 0))) return;
	set_pppoe_sessid(get_pppoe_hdr(pack->headers, 0), sid);
}

STATIC_PACKET_INLINE__
void platform_packet_set_ppp_proto(datapacket_t* pkt, uint16_t proto)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_ppp_hdr(pack->headers, 0))) return;
	set_ppp_prot(get_ppp_hdr(pack->headers, 0), proto);
}

STATIC_PACKET_INLINE__
void platform_packet_set_gtp_msg_type(datapacket_t* pkt, uint8_t msg_type)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_gtpu_hdr(pack->headers, 0))) return;
	set_gtpu_msg_type(get_gtpu_hdr(pack->headers, 0), msg_type);
}

STATIC_PACKET_INLINE__
void platform_packet_set_gtp_teid(datapacket_t* pkt, uint32_t teid)
{
	datapacketx86 *pack = (datapacketx86*)pkt->platform_state;
	if ((NULL == pack) || (NULL == get_gtpu_hdr(pack->headers, 0))) return;
	set_gtpu_teid(get_gtpu_hdr(pack->headers, 0), teid);
}


STATIC_PACKET_INLINE__
void platform_packet_pop_gtp(datapacket_t* pkt)
{
	//TODO: implement
}
STATIC_PACKET_INLINE__
void platform_packet_push_gtp(datapacket_t* pkt)
{
	//TODO: implement
}

STATIC_PACKET_INLINE__
void platform_packet_drop(datapacket_t* pkt)
{
	ROFL_DEBUG("Dropping packet (%p)\n",pkt);
	
	//Release buffer
	bufferpool::release_buffer(pkt);

}

STATIC_PACKET_INLINE__
static void output_single_packet(datapacket_t* pkt, datapacketx86* pack, switch_port_t* port){

	//Output packet to the appropiate port_num
	if(port && port->platform_port_state && port->up && port->forward_packets) {
		
		ROFL_DEBUG("[%s] OUTPUT packet (%p) to port %d\n", port->name, pkt, port->of_port_num);
#ifdef DEBUG
		dump_packet_matches(&pkt->matches, false);
#endif
                send_packet_via_ez_packet_channel(pkt, port->of_port_num);
	}
	else {
		//Silently drop the packet
		bufferpool::release_buffer(pkt);
	}
}

/**
* Output packet to the port(s)
* The action HAS to implement the destruction/release of the pkt
* (including if the pkt is a replica).
*
* If a flooding output actions needs to be done, the function
* has itself to deal with packet replication.
*/
STATIC_PACKET_INLINE__
void platform_packet_output(datapacket_t* pkt, switch_port_t* output_port){

	of_switch_t const* sw;
	datapacketx86* pack;

	if(!output_port){
		assert(0);
		return;
	}

	//Check whether dpx86 is NULL
	if (NULL == (pack = (datapacketx86*) (pkt->platform_state))){
		//TODO: in DEBUG do an EXIT(-1)
		assert(0);
		return;
	}

	//IP Checksum recalculation
	if(pack->ipv4_recalc_checksum){
		if(get_ipv4_hdr(pack->headers, 0))	
			ipv4_calc_checksum(get_ipv4_hdr(pack->headers, 0));
	}

	//Outer most IPv4 frame
	void *fipv4 = get_ipv4_hdr(pack->headers, 0);

	if ((pack->tcp_recalc_checksum) && get_tcp_hdr(pack->headers, 0) && fipv4) {
		
		
	tcp_calc_checksum(
			get_tcp_hdr(pack->headers, 0),
			get_ipv4_src(fipv4),
			get_ipv4_dst(fipv4),
			get_ipv4_proto(fipv4),
			get_pkt_len(pkt, pack->headers, get_tcp_hdr(pack->headers,0), NULL) ); // start at innermost IPv4 up to and including last frame

	} else if ((pack->udp_recalc_checksum) && (get_udp_hdr(pack->headers, 0)) && fipv4) {

		udp_calc_checksum(
				get_udp_hdr(pack->headers, 0),
				get_ipv4_src(fipv4),
				get_ipv4_dst(fipv4),
				get_ipv4_proto(fipv4),
				get_pkt_len(pkt, pack->headers, get_udp_hdr(pack->headers, 0), NULL) ); // start at innermost IPv4 up to and including last frame

	} else if ((pack->icmpv4_recalc_checksum) && (get_icmpv4_hdr(pack->headers,0))) {

		icmpv4_calc_checksum(
			get_icmpv4_hdr(pack->headers, 0),
			get_pkt_len(pkt, pack->headers, get_icmpv4_hdr(pack->headers, 0), NULL) );
	}


	//flood_meta_port is a static variable defined in the physical_switch
	//the meta_port
	if(output_port == flood_meta_port || output_port == all_meta_port){ //We don't have STP, so it is the same
		datapacket_t* replica;
		switch_port_t* port_it;
		datapacketx86* replica_pack;

		//Get switch
		sw = pkt->sw;	
		
		if(unlikely(!sw)){
			bufferpool::release_buffer(pkt);
			return;
		}
	
		//We need to flood
		for(unsigned i=0;i<LOGICAL_SWITCH_MAX_LOG_PORTS;++i){

			port_it = sw->logical_ports[i].port;

			//Check port is not incomming port, exists, and is up 
			if( (i == pack->in_port) || !port_it || port_it->no_flood)
				continue;

			//replicate packet
			replica = platform_packet_replicate(pkt); 	
			replica_pack = (datapacketx86*) (replica->platform_state);

			ROFL_DEBUG(DRIVER_NAME"[pkt][%s] OUTPUT FLOOD packet(%p), origin(%p)\n", port_it->name, replica, pkt);
			
			//send the replica
			output_single_packet(replica, replica_pack, port_it);
		}

#ifdef DEBUG
		dump_packet_matches(&pkt->matches, false);
#endif
			
		//discard the original packet always (has been replicated)
		bufferpool::release_buffer(pkt);
	}else if(output_port == in_port_meta_port){
		
		//In port
		switch_port_t* port;
		sw = pkt->sw;	

		if(unlikely(pack->in_port >= LOGICAL_SWITCH_MAX_LOG_PORTS)){
			assert(0);
			return;
		}

		port = sw->logical_ports[pack->in_port].port;
		if( unlikely(port == NULL)){
			assert(0);
			return;
		
		}
	
		//Send to the incomming port 
		output_single_packet(pkt, pack, port);
	}else{
		//Single output	
		output_single_packet(pkt, pack, output_port);
	}

}

/**
* Creates a copy (in heap) of the datapacket_t structure including any
* platform specific state (->platform_state). The following behaviour
* is expected from this hook:
* 
* - All data fields and pointers of datapacket_t struct must be memseted to 0, except:
* - datapacket_t flag is_replica must be set to true
* - platform_state, if used, must be replicated (copied) otherwise NULL
*
*/
STATIC_PACKET_INLINE__
datapacket_t* platform_packet_replicate(datapacket_t* pkt){

	//Get a free buffer
	datapacket_t* copy = bufferpool::get_free_buffer_nonblocking();
	
	if(!copy){
		return copy;
		return NULL;
	}
	
	//Make sure everything is memseted to 0
	memcpy(&copy->matches, &pkt->matches, sizeof(pkt->matches));
	memcpy(&copy->write_actions, &pkt->write_actions ,sizeof(pkt->write_actions));

	//mark as replica
	copy->is_replica = true;
	copy->sw = pkt->sw;

	//Clone contents
	clone_pkt_contents(pkt,copy);
	return copy;	
}


