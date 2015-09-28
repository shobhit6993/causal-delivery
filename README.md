Causal unicast
==============

This program executes causal unicast between a set of N (=5) processes. Causal unicast respects causal delivery of messages in a system where processes communicate with each other through unicasts.

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

>0 0 4 0 0 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 0 0 0 0 <br>
0 2 1 <br>
0 1 2 <br>
1 2 3 <br>
3 4 1 2

If there are N processes (N=5 at present), then config file contains a N*N upper triangular matrix of whole numbers. Value in cell (i,j) of the upper triangle represents the delay between process i and process j in seconds. For example, in above case, the delay between 0 and 2 is 4 sec.

This is followed by any number of lines, where each line is a space separated list of whole numbers.
>0 2 1   // It is a triple (sender, reciever, times[]). Process 0 send messages to Process 2 at 1s <br>
0 1 2   // Process 0 send messages to Process 1 at 2s <br>
1 2 3   // Process 1 send messages to Process 2 at 3s <br>
3 4 1 2 // Process 3 send messages to Process 4 at 1s, 2s <br>

### Notes:
1. Always run ```make clean``` before each execution so that previous logs are deleted. Otherwise, the old log files will be appended with new log data.
2. Repeated execution might be hindered by ports being left open. Use ```lsof -i``` command to see if previous processes are still hogging the ports, and kill them using ```kill -9 PID```
3. TCP sockets are used for interprocess communication.
4. All delays are whole numbers, measured in seconds.
5. If you see error messages on screen, then kill the program and make sure that no other (or previous instances of this program) is using the required ports.
6. Value of N can be set in the process.h file. If you change N, you have to add LISTEN_PORTi and SEND_PORTi for each new process (beyond the current 5) in process.h file, and add corresponding ports to constuctor of class Process at the beginning of process.cpp