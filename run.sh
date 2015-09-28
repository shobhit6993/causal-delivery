#!/bin/bash
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT


}
./process 0 &
./process 1 &
./process 2 &
./process 3 &
./process 4
