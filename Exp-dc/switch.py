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

reg_data = []
try:
  reg_data.append(prog.pipe.SwitchEgress.reg_data_0)
  reg_data.append(prog.pipe.SwitchEgress.reg_data_1)
  reg_data.append(prog.pipe.SwitchEgress.reg_data_2)
  reg_data.append(prog.pipe.SwitchEgress.reg_data_3)
except:
  pass

print('CM_NUM:', len(reg_data))

if 'reset_os_switch' in prog_name:
  import time
  while True:
    try:
      print('Inject the trace, and then press Enter... (Ctrl-D to quit)')
      input()
    except EOFError:
      break
    t_start = time.perf_counter()
    for r in reg_data:
      r.clear()
    t_end = time.perf_counter()
    print('Time:', t_end - t_start)
  exit()

if 'dc_os_switch' in prog_name:
  import time
  while True:
    for r in reg_data:
      r.clear()
    try:
      print('Inject the trace, and then press Enter... (Ctrl-D to quit)')
      input()
    except EOFError:
      break
    t_start = time.perf_counter()
    for r in reg_data:
      r.operation_register_sync()
      r.get(regex=True, print_ents=False)
    t_end = time.perf_counter()
    print('Time:', t_end - t_start)
  exit()

reg_clear = reg_data

if 'dc_cpc_' not in prog_name:
  if '_rdma_switch' in prog_name:
    bfrt.mirror.cfg.add_with_normal(
      sid=2,
      direction='EGRESS',
      session_enable=True,
      ucast_egress_port=4,
      ucast_egress_port_valid=1,
      max_pkt_len=17)
    if 'dc_ow_' in prog_name:
      reg_clear.append(prog.pipe.SwitchEgress.reg_msn)
  else:
    bfrt.mirror.cfg.add_with_normal(
      sid=2,
      direction='EGRESS',
      session_enable=True,
      ucast_egress_port=4,
      ucast_egress_port_valid=1,
      max_pkt_len=100)

if 'reset_ow_switch' in prog_name:
  prog.pipe.SwitchIngress.tab_reset.add_with_send(valid=True, egress_port=4)

if '_rdma_switch' in prog_name:
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
  prog.pipe.SwitchEgress.reg_psn.clear()
  try:
    prog.pipe.SwitchEgress.tab_rdma.set_default_with_fill_rdma(
      src_mac='0c:42:a1:67:8d:2e', src_ip='10.64.100.37',
      dst_mac='0c:42:a1:67:8d:2e', dst_ip='10.64.100.37',
      dst_qp=dst_qp, rkey=rkey, vaddr_base=msg_vaddr_base)
  except TypeError:
    prog.pipe.SwitchEgress.tab_rdma.set_default_with_fill_rdma(
      src_mac='0c:42:a1:67:8d:2e', src_ip='10.64.100.37',
      dst_mac='0c:42:a1:67:8d:2e', dst_ip='10.64.100.37',
      dst_qp=dst_qp, rkey=rkey)

  ns.close()

for r in reg_clear:
  r.clear()
print('Initialization done.')

if 'reset_ow_switch' in prog_name:
  exit()

while True:
  print('Press Enter to reset. Ctrl-D to quit.')
  try:
    input()
  except:
    break

  for r in reg_clear:
    r.clear()

  print('Done.')

exit()
