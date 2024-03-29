Causal broadcast
==============


```NOTE:``` The master branch of this repository implements causal broadcast. The unicast branch of this repository implements causal unicast

This program executes causal broadcast between a set of N (=5) processes. Causal broadcast respects causal delivery of messages in a system where processes communicate with each other only through broadcasts.

### Build instructions:
```cd``` to the project directory, and type ```make```. It should create 5 executables named ``process0, process1, ..., process4``. Type the following command to delete all executables and logfiles
```sh
make clean
``` 

### Running instructions:
Give execute permission to ```run.sh``` script using
```sh
chmod +x ./run.sh
```
Run the script using
```sh
./run.sh
```
This runs 5 processes (one for each "process"). You should see debugging/status messages on screen. When the messages stop getting printed (along with the knowledge of delay values set in the config file), kill the process(es) using ```Ctrl+C```.

The processes do not kill themselves since they are always listening to incoming messages, logging parallely, with unknown (simulated) delay in message transmission.

### Logs:
Each process generates its own log file, namely, ``log0, log1,...``

### Configuration:
The configurations can be provided in config file. Following is a sample config file

>0 0 3 3 3 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 br at 1 <br>
1 br at 2 3

If there are N processes (N=5 at present), then config file contains a N*N upper triangular matrix of whole numbers. Value in cell (i,j) of the upper triangle represents the delay between process i and process j in seconds. For example, in above case, the delay between 0 and 2 is 4 sec.

This is followed by at max N number of lines, where each line starts with process ID, followed by " br at " folllwed by a space separated list of whole numbers.

>0 br at 1       // Process 0 broadcasts at t=1 <br>
1 br at 2 3     // Process 1 braodcasts at t=2 and t=3 <br>

To change the number of processes (N), in addition to making appropriate changes to the config file, add ```#define``` for each process' LISTEN_PORT and SEND_PORT in process.h, and add statements for ```port_pid_map.insert()```, ```send_port_no[i]```, and ```listen_port_no[i]``` in the constructor of ```Process``` class at the beginning of process.cpp. In hindsight, I should have read these values from the config file, rather than hardcoding it in the code.

### Notes:
1. Always run ```make clean``` before each execution so that previous logs are deleted. Otherwise, the old log files will be appended with new log data.
2. The threads sleep for about 5 seconds in the beginning; so, please be patient.
3. run.sh has a trap for ```Ctrl+C``` which ensures that process are terminated cleaning and that they do not continue to hog ports after termination. Press ```Ctrl+C``` TWICE to terminate execution, and wait for few seconds for the interupt to clean up the mess.
4. Repeated execution might be hindered by ports being left open. Use ```lsof -i``` command to see if previous processes are still hogging the ports, and kill them using ```kill -9 PID```
5. TCP sockets are used for interprocess communication.
6. All delays are whole numbers, measured in seconds.
7. If you see error messages on screen, then kill the program and make sure that no other (or previous instances of this program) is using the required ports.
8. Value of N can be set in the process.h file. If you change N, you have to add LISTEN_PORTi and SEND_PORTi for each new process (beyond the current 5) in process.h file, and add corresponding ports to constuctor of class Process at the beginning of process.cpp