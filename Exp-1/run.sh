RESULT_DIR=result
TRACE_PATH=../data/trace.bin

run() {
  ./build/apps/bin/$1 $TRACE_PATH ${@:2} >$RESULT_DIR/$1.txt &
}

run new_tcp_connections 87
run ssh_brute           10
run superspreader       249
run port_scan           55
run ddos                174
run syn_flood           49
run completed_flow      60
run slowloris_attack    7500 0.004000
