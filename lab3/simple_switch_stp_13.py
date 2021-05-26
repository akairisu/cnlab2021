# Copyright (C) 2016 Nippon Telegraph and Telephone Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import socket
import netifaces as ni
from operator import attrgetter
from uuid import getnode
from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, MAIN_DISPATCHER, DEAD_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_3
from ryu.lib import dpid as dpid_lib
from ryu.lib import stplib
from ryu.lib.packet import packet
from ryu.lib.packet import ethernet
from ryu.lib.packet import ipv4
from ryu.lib.packet import icmp
from ryu.lib.packet import arp
from ryu.lib.packet import *
from ryu.app import simple_switch_13
from ryu.lib import hub
from collections import defaultdict


class SimpleSwitch13(simple_switch_13.SimpleSwitch13):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]
    _CONTEXTS = {'stplib': stplib.Stp}

    def __init__(self, *args, **kwargs):
        super(SimpleSwitch13, self).__init__(*args, **kwargs)
        self.count = 0
        self.mac_to_port = {}
        self.flow = defaultdict(lambda: defaultdict(int))
        self.stp = kwargs['stplib']
        self.hw_addr = ':'.join(format(s, '02x') for s in bytes.fromhex(hex(getnode())[2:]))
        iface = ni.gateways()['default'][ni.AF_INET][1]
        self.ip_addr = ni.ifaddresses(iface)[ni.AF_INET][0]['addr']
        
        # Sample of stplib config.
        #  please refer to stplib.Stp.set_config() for details.
        config = {dpid_lib.str_to_dpid('0000000000000001'):
                  {'bridge': {'priority': 0x8000}},
                  dpid_lib.str_to_dpid('0000000000000002'):
                  {'bridge': {'priority': 0x9000}},
                  dpid_lib.str_to_dpid('0000000000000003'):
                  {'bridge': {'priority': 0xa000}}}
        self.stp.set_config(config)
        self.datapaths = {}
        self.monitor_thread = hub.spawn(self._monitor)

    def delete_flow(self, datapath):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        for dst in self.mac_to_port[datapath.id].keys():
            match = parser.OFPMatch(eth_dst=dst)
            mod = parser.OFPFlowMod(
                datapath, command=ofproto.OFPFC_DELETE,
                out_port=ofproto.OFPP_ANY, out_group=ofproto.OFPG_ANY,
                priority=1, match=match)
            datapath.send_msg(mod)
            
    def drop_flow(self, datapath, priority, match, buffer_id=None):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        inst = [parser.OFPInstructionActions(ofproto.OFPIT_CLEAR_ACTIONS, [])]
        if buffer_id:
            mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
                                    priority=priority, match=match,
                                    instructions=inst)
        else:
            mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
                                    match=match, instructions=inst)
        datapath.send_msg(mod)
    
    def remove_flow(self, datapath, match):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        mod = parser.OFPFlowMod(
            datapath, command=ofproto.OFPFC_DELETE,
            out_port=ofproto.OFPP_ANY, out_group=ofproto.OFPG_ANY,
            priority=1, match=match)
        datapath.send_msg(mod)
        
    @set_ev_cls(stplib.EventPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, ev):
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        in_port = msg.match['in_port']

        pkt = packet.Packet(msg.data)
        
        pkt_eth = pkt.get_protocols(ethernet.ethernet)[0]       
        dst = pkt_eth.dst
        src = pkt_eth.src
        
        pkt_ip = pkt.get_protocols(ipv4.ipv4)
        ip_dst = 0
        ip_src = 0
        port_src = 0
        port_dst = 0
        ip_proto = 0
        
        pkt_arp = pkt.get_protocols(arp.arp)
        
        pkt_icmp = pkt.get_protocols(icmp.icmp)
        pkt_tcp = pkt.get_protocols(tcp.tcp)
        pkt_udp = pkt.get_protocols(udp.udp)
        
        if pkt_arp:
            ip_dst = pkt_arp[0].dst_ip
            ip_src = pkt_arp[0].src_ip
            ip_dst_last = int(ip_dst.split('.')[3])
            ip_src_last = int(ip_src.split('.')[3])
            #if ip_src_last % 2 != ip_dst_last % 2:
            #    return
        elif pkt_ip:
            ip_proto = pkt_ip[0].proto
            ip_dst = pkt_ip[0].dst
            ip_src = pkt_ip[0].src
            ip_dst_last = int(ip_dst.split('.')[3])
            ip_src_last = int(ip_src.split('.')[3])
            
        if pkt_tcp:
            port_src = pkt_tcp[0].src_port
            port_dst = pkt_tcp[0].dst_port
        elif pkt_udp:
            port_src = pkt_udp[0].src_port
            port_dst = pkt_udp[0].dst_port
              
        #self.logger.info("ip_src: %d ip_dst: %d", ip_src, ip_dst)
        
        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})

        #self.logger.info("packet in %s %s %s %s", dpid, src, dst, in_port)

        # learn a mac address to avoid FLOOD next time.
        self.mac_to_port[dpid][src] = in_port

        if dst in self.mac_to_port[dpid]:
            out_port = self.mac_to_port[dpid][dst]
        else:
            out_port = ofproto.OFPP_FLOOD

        actions = [parser.OFPActionOutput(out_port)]

        # install a flow to avoid packet_in next time
        if out_port != ofproto.OFPP_FLOOD:
            match = parser.OFPMatch(eth_type=pkt_eth.ethertype, in_port=in_port, eth_dst=dst, ipv4_src=ip_src, ipv4_dst=ip_dst, ip_proto=ip_proto)
            if pkt_tcp:
                #print("tcp")
                match = parser.OFPMatch(eth_type=pkt_eth.ethertype, in_port=in_port, eth_dst=dst, ipv4_src=ip_src, ipv4_dst=ip_dst, ip_proto=ip_proto, tcp_src=port_src, tcp_dst=port_dst)   
            elif pkt_udp:
                #print("udp")
                match = parser.OFPMatch(eth_type=pkt_eth.ethertype, in_port=in_port, eth_dst=dst, ipv4_src=ip_src, ipv4_dst=ip_dst, ip_proto=ip_proto, udp_src=port_src, udp_dst=port_dst)
            elif pkt_arp:
                #print("arp")
                match = parser.OFPMatch(eth_type=pkt_eth.ethertype, in_port=in_port, eth_dst=dst, arp_spa=ip_src, arp_tpa=ip_dst)#ip_proto=0)
            #elif pkt_icmp :
                #print("icmp")
                
            if ip_src_last % 2 != ip_dst_last % 2 and not pkt_arp:
                self.drop_flow(datapath, 1, match)
                #if not pkt.get_protocols(ipv6.ipv6):
                #    self.logger.info("drop %s %s %s", dpid, ip_src, ip_dst)
                return
            else:
                self.add_flow(datapath, 1, match, actions)
                #if not pkt.get_protocols(ipv6.ipv6):
                #    self.logger.info("add  %s %s %s", dpid, ip_src, ip_dst)

        data = None
        if msg.buffer_id == ofproto.OFP_NO_BUFFER:
            data = msg.data

        out = parser.OFPPacketOut(datapath=datapath, buffer_id=msg.buffer_id,
                                  in_port=in_port, actions=actions, data=data)
        datapath.send_msg(out)
    
    def _handle_icmp(self, datapath, port, pkt_ethernet, pkt_ipv4, pkt_icmp):
        pkt = packet.Packet()
        pkt.add_protocol(ethernet.ethernet(ethertype=pkt_ethernet.ethertype,dst=pkt_ethernet.src,src=self.hw_addr))
        pkt.add_protocol(ipv4.ipv4(dst=pkt_ipv4.src,src=self.ip_addr,proto=pkt_ipv4.proto))
        pkt.add_protocol(icmp.icmp(type_=icmp.ICMP_DEST_UNREACH,code=icmp.ICMP_DEST_UNREACH_CODE,csum=0,data=pkt_icmp.data))
        self._send_packet(datapath, port, pkt)

    @set_ev_cls(stplib.EventTopologyChange, MAIN_DISPATCHER)
    def _topology_change_handler(self, ev):
        dp = ev.dp
        dpid_str = dpid_lib.dpid_to_str(dp.id)
        msg = 'Receive topology change event. Flush MAC table.'
        self.logger.debug("[dpid=%s] %s", dpid_str, msg)

        if dp.id in self.mac_to_port:
            self.delete_flow(dp)
            del self.mac_to_port[dp.id]

    @set_ev_cls(stplib.EventPortStateChange, MAIN_DISPATCHER)
    def _port_state_change_handler(self, ev):
        dpid_str = dpid_lib.dpid_to_str(ev.dp.id)
        of_state = {stplib.PORT_STATE_DISABLE: 'DISABLE',
                    stplib.PORT_STATE_BLOCK: 'BLOCK',
                    stplib.PORT_STATE_LISTEN: 'LISTEN',
                    stplib.PORT_STATE_LEARN: 'LEARN',
                    stplib.PORT_STATE_FORWARD: 'FORWARD'}
        self.logger.debug("[dpid=%s][port=%d] state=%s",
                          dpid_str, ev.port_no, of_state[ev.port_state])
                          
    @set_ev_cls(ofp_event.EventOFPStateChange,
                [MAIN_DISPATCHER, DEAD_DISPATCHER])
    def _state_change_handler(self, ev):
        datapath = ev.datapath
        if ev.state == MAIN_DISPATCHER:
            if datapath.id not in self.datapaths:
                self.logger.debug('register datapath: %016x', datapath.id)
                self.datapaths[datapath.id] = datapath
        elif ev.state == DEAD_DISPATCHER:
            if datapath.id in self.datapaths:
                self.logger.debug('unregister datapath: %016x', datapath.id)
                del self.datapaths[datapath.id]

    def _monitor(self):
        while True:
            self.count = self.count + 1
            for dp in self.datapaths.values():
                self._request_stats(dp)
            hub.sleep(1)
            if self.count == 10:
                self.count = 0
#            os.system('clear')

    def _request_stats(self, datapath):
        self.logger.debug('send stats request: %016x', datapath.id)
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        req = parser.OFPFlowStatsRequest(datapath)
        datapath.send_msg(req)

        req = parser.OFPPortStatsRequest(datapath, 0, ofproto.OFPP_ANY)
        datapath.send_msg(req)

    @set_ev_cls(ofp_event.EventOFPFlowStatsReply, MAIN_DISPATCHER)
    def _flow_stats_reply_handler(self, ev):
        body = ev.msg.body
        #print(vars(ev))
        sum_of_flow = defaultdict(int)
        largest_flow = defaultdict(lambda : (0, (' ', 0, ' ', 0, ' ')))
        num_of_flow = defaultdict(int)
        if self.count == 10:
            self.logger.info('Flow Statistical Information')
            self.logger.info('datapath         '
                             'in-port  src_ip            '
                             'src_port dst_ip            '
                             'dst_port protocol  '
                             'action   packets  bytes')
            self.logger.info('---------------- '
                             '-------- ----------------- '
                             '-------- ----------------- '
                             '-------- --------- '
                             '-------- -------- ----------')
        for stat in sorted([flow for flow in body if flow.priority == 1],
                           key=lambda flow: (flow.match.get('in_port', 0), 
                                             flow.match.get('eth_dst', 0))):
            if stat.match['eth_type'] == 0x0806:
                continue                                
            
            action = 'dropped'
            in_port = -1
            if stat.instructions[0].type != 5:
                action = 'port ' + hex(stat.instructions[0].actions[0].port)[2:]
                in_port = stat.match['in_port']
                if stat.match['ip_proto'] == 6:
                    diff = stat.byte_count - self.flow[(ev.msg.datapath.id, stat.instructions[0].actions[0].port)][(stat.match['ipv4_src'], stat.match['tcp_src'], 
                                                        stat.match['ipv4_dst'],stat.match['tcp_dst'], 'TCP')]
                    if diff > 0:
                        num_of_flow[stat.instructions[0].actions[0].port] = num_of_flow[stat.instructions[0].actions[0].port] + 1
                    sum_of_flow[stat.instructions[0].actions[0].port] = sum_of_flow[stat.instructions[0].actions[0].port] + diff
                    if diff > largest_flow[stat.instructions[0].actions[0].port][0]:
                        largest_flow[stat.instructions[0].actions[0].port] = (diff, (stat.match['ipv4_src'], stat.match['tcp_src'], stat.match['ipv4_dst'],stat.match['tcp_dst'], 'TCP'))
                    self.flow[(ev.msg.datapath.id, stat.instructions[0].actions[0].port)][(stat.match['ipv4_src'], stat.match['tcp_src'], stat.match['ipv4_dst'],stat.match['tcp_dst'], 'TCP')] = stat.byte_count
                elif stat.match['ip_proto'] == 17:
                    diff = stat.byte_count - self.flow[(ev.msg.datapath.id, stat.instructions[0].actions[0].port)][(stat.match['ipv4_src'], stat.match['udp_src'], 
                                                        stat.match['ipv4_dst'],stat.match['udp_dst'], 'UDP')]
                    if diff > 0:
                        num_of_flow[stat.instructions[0].actions[0].port] = num_of_flow[stat.instructions[0].actions[0].port] + 1
                    sum_of_flow[stat.instructions[0].actions[0].port] = sum_of_flow[stat.instructions[0].actions[0].port] + diff
                    if diff > largest_flow[stat.instructions[0].actions[0].port][0]:
                        largest_flow[stat.instructions[0].actions[0].port] = (diff, (stat.match['ipv4_src'], stat.match['udp_src'], stat.match['ipv4_dst'],stat.match['udp_dst'], 'UDP'))
                    self.flow[(ev.msg.datapath.id, stat.instructions[0].actions[0].port)][(stat.match['ipv4_src'], stat.match['udp_src'], stat.match['ipv4_dst'],stat.match['udp_dst'], 'UDP')] = stat.byte_count
                    
            if self.count == 10:   
                if stat.match['ip_proto'] == 6:
                    self.logger.info('%016x %8x %17s %8d %17s %8d %9s %8s %8d %10d',
                                 ev.msg.datapath.id,
                                 in_port, stat.match['ipv4_src'],
                                 stat.match['tcp_src'], stat.match['ipv4_dst'],
                                 stat.match['tcp_dst'], 'TCP',
                                 action, #stat.instructions[0].actions[0].port,#action
                                 stat.packet_count, stat.byte_count)
                elif stat.match['ip_proto'] == 17:
                    self.logger.info('%016x %8x %17s %8d %17s %8d %9s %8s %8d %10d',
                                 ev.msg.datapath.id,
                                 in_port, stat.match['ipv4_src'],
                                 stat.match['udp_src'], stat.match['ipv4_dst'],
                                 stat.match['udp_dst'], 'UDP',
                                 action, #stat.instructions[0].actions[0].port,#action
                                 stat.packet_count, stat.byte_count)
                elif stat.match['ip_proto'] == 1:
                    self.logger.info('%016x %8x %17s %8s %17s %8s %9s %8s %8d %10d',
                                 ev.msg.datapath.id,
                                 in_port, stat.match['ipv4_src'],
                                 ' ', stat.match['ipv4_dst'],
                                 ' ', 'ICMP',
                                 action, #stat.instructions[0].actions[0].port,#action
                                 stat.packet_count, stat.byte_count)

        for port in sum_of_flow.keys():
            #if sum_of_flow[port] > 0:
            #    print("dpid: ", ev.msg.datapath.id, "port: ",  port, "sum: ", sum_of_flow[port], "num: ", num_of_flow[port], "largest: ", largest_flow[port])
            if sum_of_flow[port] > 1000000 and num_of_flow[port] > 1:
                print("Congestion Alert!", "dpid: ", ev.msg.datapath.id, "port: ",  port, "sum: ", sum_of_flow[port], "num: ", num_of_flow[port], "largest: ", largest_flow[port])
                self.flow[(ev.msg.datapath.id, port)][largest_flow[port][1]] = 0
                
                for dp in self.datapaths.values():
                    parser = dp.ofproto_parser
                    match = parser.OFPMatch(eth_type=0x0800)
                    if largest_flow[port][1][4] == 'TCP':
                        match = parser.OFPMatch(eth_type=0x0800, ipv4_src=largest_flow[port][1][0], ipv4_dst=largest_flow[port][1][2], ip_proto=6, tcp_src=largest_flow[port][1][1], tcp_dst=largest_flow[port][1][3])   
                    elif largest_flow[port][1][4] == 'UDP':
                        match = parser.OFPMatch(eth_type=0x0800, ipv4_src=largest_flow[port][1][0], ipv4_dst=largest_flow[port][1][2], ip_proto=17, udp_src=largest_flow[port][1][1], udp_dst=largest_flow[port][1][3]) 
                    self.remove_flow(dp, match)
                    self.drop_flow(dp, 1, match)
                
                
                

    @set_ev_cls(ofp_event.EventOFPPortStatsReply, MAIN_DISPATCHER)
    def _port_stats_reply_handler(self, ev):
        body = ev.msg.body
        if self.count == 10:
            self.logger.info('Port Statistical Information')
            self.logger.info('datapath         port     '
                             'rx-pkts  rx-bytes rx-error '
                             'tx-pkts  tx-bytes tx-error')
            self.logger.info('---------------- -------- '
                             '-------- -------- -------- '
                             '-------- -------- --------')
            for stat in sorted(body, key=attrgetter('port_no')):
                self.logger.info('%016x %8x %8d %8d %8d %8d %8d %8d',
                                 ev.msg.datapath.id, stat.port_no,
                                 stat.rx_packets, stat.rx_bytes, stat.rx_errors,
                                 stat.tx_packets, stat.tx_bytes, stat.tx_errors)


