#!/bin/bash
set -e
 
PORT_BASE=5000
 
cleanup() {
	pkill -9 dbserver || true
}
 
trap cleanup EXIT
 
run_test() {
	local test_name=$1
	local port=$2
	shift 2
	echo "test ${test_name} running"
 
	(./dbserver ${port}) &
	server_pid=$!
 
	sleep 1
 
	"$@"

	sleep 1

	process_id=$(pgrep -f "dbserver ${port}")

	kill "$process_id"

 
	echo "test ${test_name} finished"
	echo
}
 
make || { echo "error"; exit 1; }
 
run_test "basics" $((PORT_BASE+1)) \
	bash -c "
		./dbtest -p $((PORT_BASE+1)) -S key1 value1 && \
		./dbtest -p $((PORT_BASE+1)) -G key1 | grep 'value1' && \
		./dbtest -p $((PORT_BASE+1)) -D key1 && \
		./dbtest -p $((PORT_BASE+1)) -G key1 | grep 'X'
		"
run_test "mult" $((PORT_BASE+2)) \
		./dbtest -p $((PORT_BASE+2)) -n 100 -t 4
 
run_test "max" $((PORT_BASE+3)) \
	bash -c "
		for i in {0..199};
			do ./dbtest -p $((PORT_BASE+3)) -S key_\${i} value_\${i}
		done && \
		./dbtest -p $((PORT_BASE+3)) -S key_200 value_200 | grep 'X'
		"
 
run_test "concurrent" $((PORT_BASE+4)) \
	bash -c "
		for i in {1..20};
			do ./dbtest -p $((PORT_BASE+4)) -S ckey \${i} &
		done && \
		wait
		"
 
run_test "mixed" $((PORT_BASE+5)) \
	./dbtest -p $((PORT_BASE+5)) -n 500 -t 10
 
 
echo "tests done!"