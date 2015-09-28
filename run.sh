#!/bin/bash
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT


}
./process0 0 &
./process1 1 &
./process2 2 &
./process2 3 &
./process2 4

wait