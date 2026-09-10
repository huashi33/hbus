#! /bin/bash
pub_interval_us=500
pub_count=1000
sub_node_num=$2
sub_time_s=$(echo "$pub_interval_us * $pub_count /1000000 + 2"|bc)
echo "pub_interval_us: $pub_interval_us"
echo "pub_count: $pub_count"
echo "sub_time_s: $sub_time_s"

work_dir=$1
$work_dir/hbroker &
pid_broker=$!
sleep 1
# sub
sub_pids=()

for i in $(seq 1 $sub_node_num);do
  $work_dir/test/tbase s $i $sub_time_s&
  pid=$!
  sub_pids+=($pid)
done
sleep 1
# pub
$work_dir/test/tbase p $pub_interval_us $pub_count
for i in ${sub_pids[@]};do
  wait $i
done

kill $pid_broker


