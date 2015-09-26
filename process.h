//setbase - cout << setbase (16); cout << 100 << endl; Prints 64
//setfill - cout << setfill ('x') << setw (5); cout << 77 << endl; prints xxx77
//setprecision - cout << setprecision (4) << f << endl; Prints x.xxxx
#include <iostream>
#include <string>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <utility>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <cstring>
#include <ctime>
#include "fstream"
#include "sstream"
#include "pthread.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;
#define PR(x) cout << #x " = " << x << "\n";
#define N 5
#define CONFIG_FILE "config.txt"
#define LISTEN_PORT0 "10666"  // the port on which p0 listens for incoming connections
#define LISTEN_PORT1 "11666"  // the port on which p1 listens for incoming connections
#define LISTEN_PORT2 "12666"  // the port on which p2 listens for incoming connections
#define LISTEN_PORT3 "13666"  // the port on which p3 listens for incoming connections
#define LISTEN_PORT4 "14666"  // the port on which p4 listens for incoming connections

#define SEND_PORT0 "10777"        // the port from which p0 sends messages
#define SEND_PORT1 "11777"        // the port from which p1 sends messages
#define SEND_PORT2 "12777"        // the port from which p2 sends messages
#define SEND_PORT3 "13777"        // the port from which p3 sends messages
#define SEND_PORT4 "14777"        // the port from which p4 sends messages

#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s);
void* initiate_connections(void*);

class Process
{
private:
    std::vector<std::vector<time_t> > vc;
    std::vector<time_t> delay;
    std::vector<time_t> br_time;
    // std::vector<int> parent_fd;
    std::vector<int> fd;
    std::vector<string> listen_port_no;
    std::vector<string> send_port_no;
    std::map<int, int> port_pid_map;

public:
    Process();
    void set_fd(int incoming_port, int new_fd);
    string get_listen_port_no(int _pid);

    int return_in_addr(struct sockaddr *sa);
    void read_config(string filename = CONFIG_FILE);
    // int server();
    void initiate_connections();
    int client(int);
    void print();

};

