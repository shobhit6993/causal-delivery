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
#include <algorithm>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <cstring>
#include <ctime>
#include "fstream"
#include "sstream"
#include "pthread.h"
#include "time.h"

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
#define N 3
#define CONFIG_FILE "config.txt"
const string ST_BR_MSG = "ST_BR";

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
#define LOG_FILE "log"

void sigchld_handler(int s);
void* start_broadcast(void*);
void* server(void*);
void* receive(void* _P);
void* logger(void* _P);

typedef enum
{
    SEND, RECEIVE, DELIVER
} MsgObjType;

struct MsgObj
{
    string msg;
    std::vector<int> vc;
    int source_pid;
    int dest_pid;
    time_t send_time;
    time_t recv_time;
    time_t delv_time;

    MsgObjType type;

    MsgObj(string msg, MsgObjType type, int source_pid = -1, int dest_pid = -1, time_t send_time = -1, time_t recv_time = -1, time_t delv_time = -1, std::vector<int> vc = std::vector<int> (N, -1)) : msg(msg), type(type), source_pid(source_pid), dest_pid(dest_pid), send_time(send_time), recv_time(recv_time), delv_time(delv_time), vc(vc) {};
};

class Process
{
private:
    std::vector<time_t> delay;
    std::vector<time_t> br_time;
    std::vector<int> fd;
    std::vector<string> listen_port_no;
    std::vector<string> send_port_no;
    std::map<int, int> port_pid_map;
    std::vector<int> vc;


public:
    std::map<time_t, std::vector<MsgObj> > msg_buf;
    time_t start_time;

    Process();
    void set_fd(int incoming_port, int new_fd);
    void set_fd_by_pid(int _pid, int new_fd);

    string get_listen_port_no(int _pid);
    time_t get_br_time(int);
    int get_fd(int _pid);
    int get_br_time_size();
    int get_port_pid_map(int port);
    string get_send_port_no(int _pid);
    time_t get_delay(int _pid);


    int return_in_addr(struct sockaddr * sa);
    void read_config(string filename = CONFIG_FILE);
    void initiate_connections();
    int client(int);
    void print();

    void msg_handler(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time);
    void extract_vc(string msg, string &body, std::vector<int> &vc_msg);
    string construct_msg(int _pid, int msg_counter, string &msg_body);

    void vc_update_send(int _pid);
    void vc_update_recv(std::vector<int> &vc_msg, int _pid);





    void log_br(string msg, int pid, time_t t);
    void log_rcv(string msg, int pid, time_t t);


};

struct Arg
{
    Process * P;
    int pid;
};

// msg are of the following format (without quotes)
// "P5:21 12 1 0 100 5"
// message body, followed by a space, followed by VC of the send event of the message.
// message body format -- 'P' followed by processID followed by ':' followed by message index (local to each process)
// VC is a space separated list of whole numbers
