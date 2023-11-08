import json
import socket

with open('/tmp/OmniWindow-config.json', 'r') as fp:
  conf = json.load(fp)
host_dp = conf['owhost-dp']
recv_dp = conf['recv-dp']
host_mac = conf['owhost-mac']
host_ip = conf['owhost-ip']

prog_name = bfrt.p4_programs_list[0]
print(f'Program: {prog_name}')
prog = eval(f'bfrt.{prog_name}')

prog.pipe.SwitchIngress.tab_forward.add_with_send(ingress_port=recv_dp, egress_port=recv_dp)
prog.pipe.SwitchIngress.tab_forward.add_with_send(ingress_port=host_dp, egress_port=recv_dp)

bfrt.mirror.cfg.add_with_normal(
  sid=2,
  direction='EGRESS',
  session_enable=True,
  ucast_egress_port=host_dp,
  ucast_egress_port_valid=1,
  max_pkt_len=17)
# In the interactive python environment on the switch

listener = socket.create_server(('0.0.0.0', 33668))
print('Waiting for the OmniWindows host...')
ns, hostAddr = listener.accept()
print(f'Accepted: {hostAddr}')
listener.close()

dst_qp = int.from_bytes(ns.recv(4, socket.MSG_WAITALL), "big", signed=False)
rkey = int.from_bytes(ns.recv(4, socket.MSG_WAITALL), "big", signed=False)
msg_vaddr_base = int.from_bytes(ns.recv(8, socket.MSG_WAITALL), "big", signed=False)
print(f'dst_qp={hex(dst_qp)}, rkey={hex(rkey)}, msg_vaddr_base={hex(msg_vaddr_base)}')
ns.sendall(bytes(1))

prog.pipe.SwitchEgress.tab_addr.clear()
nKeys = int.from_bytes(ns.recv(4, socket.MSG_WAITALL), "big", signed=True)
keysData = ns.recv(nKeys * 12, socket.MSG_WAITALL)
for i in range(nKeys):
  key = int.from_bytes(keysData[i*12:i*12+4], "big", signed=False)
  vaddr = int.from_bytes(keysData[i*12+4:i*12+12], "big", signed=False)
  prog.pipe.SwitchEgress.tab_addr.add_with_addr_tab_hit(dst_ip=key, vaddr=vaddr)
print(f'{nKeys} keys inserted')
ns.sendall(bytes(1))

import re

prog.pipe.SwitchEgress.tab_rdma.set_default_with_fill_rdma(
  src_mac=host_mac, src_ip=host_ip,
  dst_mac=host_mac, dst_ip=host_ip,
  dst_qp=dst_qp, rkey=rkey, msg_vaddr_base=msg_vaddr_base)

prog.pipe.SwitchEgress.reg_msg_addr.clear()
prog.pipe.SwitchEgress.reg_psn.clear()

prog.pipe.SwitchEgress.reg_epoch.clear()
prog.pipe.SwitchEgress.reg_reset_idx.mod(0, 65536)
prog.pipe.SwitchEgress.reg_bf_x.clear()
prog.pipe.SwitchEgress.reg_bf_y.clear()
prog.pipe.SwitchEgress.reg_key_idx.clear()
prog.pipe.SwitchEgress.reg_cm1_x.clear()
prog.pipe.SwitchEgress.reg_cm2_x.clear()

ns.close()
print('Initialization done.')

exit()
