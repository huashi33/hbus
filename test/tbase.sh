#! /bin/bash
work_dir=$1
$work_dir/hbusbroker &
pid_broker=$!
sleep 1
# sub
sub_pids=()
sub_num=$2
for i in $(seq 1 $sub_num);do
  $work_dir/test/tbase s $i &
  pid=$!
  sub_pids+=($pid)
done
sleep 1
# pub
$work_dir/test/tbase p 1000 5000
for i in ${sub_pids[@]};do
  wait $i
done

kill $pid_broker


