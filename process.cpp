#include "process.h"
int PID;

pthread_mutex_t fd_lock;
pthread_mutex_t log_buf_lock;
pthread_mutex_t sent_trace_lock;
pthread_mutex_t recv_trace_lock;
pthread_mutex_t delv_buf_lock;
pthread_mutex_t recv_buf_lock;

Process::Process(): sent_trace(N, std::vector<int> (N, 0)), recv_trace(N, 0), delay(N, 0), fd(N, -1), send_port_no(N), listen_port_no(N), unicast_time(N, std::vector<time_t>())
{
    port_pid_map.insert(make_pair(atoi(SEND_PORT0), 0));
    port_pid_map.insert(make_pair(atoi(SEND_PORT1), 1));
    port_pid_map.insert(make_pair(atoi(SEND_PORT2), 2));
    port_pid_map.insert(make_pair(atoi(SEND_PORT3), 3));
    port_pid_map.insert(make_pair(atoi(SEND_PORT4), 4));

    send_port_no[0] = SEND_PORT0;
    send_port_no[1] = SEND_PORT1;
    send_port_no[2] = SEND_PORT2;
    send_port_no[3] = SEND_PORT3;
    send_port_no[4] = SEND_PORT4;

    listen_port_no[0] = LISTEN_PORT0;
    listen_port_no[1] = LISTEN_PORT1;
    listen_port_no[2] = LISTEN_PORT2;
    listen_port_no[3] = LISTEN_PORT3;
    listen_port_no[4] = LISTEN_PORT4;
}

// get port number
int Process::return_port_no(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

string Process::get_listen_port_no(int _pid)
{
    return listen_port_no[_pid];
}

int Process::get_port_pid_map(int port)
{
    return port_pid_map[port];
}

string Process::get_send_port_no(int _pid)
{
    return send_port_no[_pid];
}

void Process::set_fd(int incoming_port, int new_fd)
{
    pthread_mutex_lock(&fd_lock);
    if (fd[port_pid_map[incoming_port]] == -1)
    {
        fd[port_pid_map[incoming_port]] = new_fd;
    }
    pthread_mutex_unlock(&fd_lock);
}

void Process::set_fd_by_pid(int _pid, int new_fd)
{
    if (fd[_pid] == -1)
    {
        fd[_pid] = new_fd;
    }
    pthread_mutex_unlock(&fd_lock);
}

time_t Process::get_unicast_time(int i, int j)
{
    return unicast_time[i][j];
}

int Process::get_fd(int _pid)
{
    int ret;
    pthread_mutex_lock(&fd_lock);
    ret = fd[_pid];
    pthread_mutex_unlock(&fd_lock);
    return ret;
}

int Process::get_unicast_size(int i)
{
    return unicast_time[i].size();
}

time_t Process::get_delay(int _pid)
{
    return delay[_pid];
}

// reads config file
// fills delay and unicast_time vectors
void Process::read_config(string filename)
{
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try
    {
        fin.open(filename.c_str());

        time_t m[N][N];
        int i = 0, d;
        while (i < N && !fin.eof())
        {
            for (int j = 0; j < N; ++j)
            {
                fin >> d;
                m[i][j] = d;
            }
            i++;
        }

        // all this crap because delay matrix is upper triagular
        for ( i = 0; i < N; ++i)
        {
            if (i <= PID)
                delay[i] = m[i][PID];
            else
                delay[i] = m[PID][i];
        }

        int p, source, dest, t;
        string line;
        while (!fin.eof())
        {
            std::getline(fin, line);

            std::istringstream iss(line);

            iss >> source;
            iss >> dest;
            if (source == PID)
            {
                while (iss >> t)
                {
                    unicast_time[dest].push_back(t);
                }
            }

        }

        for (int i = 0; i < N; ++i)
        {
            if (!unicast_time[i].empty())
                sort(unicast_time[i].begin(), unicast_time[i].end());
        }

    }
    catch (ifstream::failure e)
    {
        cerr << e.what();
    }

    fin.close();
}

// function where process acts as server
// listens to incoming connect() requests
void* server(void* _P)
{
    Process *P = (Process *)_P;

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, (P->get_listen_port_no(PID)).c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind ERROR");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen ERROR");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    cout << "pid=" << PID << " server: waiting for connections...\n";
    while (1)
    {   // main accept() loop
        sin_size = sizeof their_addr;

        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept ERROR");
            continue;
        }

        int incoming_port = ntohs(P->return_port_no((struct sockaddr *)&their_addr));
        cout << "pid=" << PID << " server: got connection from " << PID << endl;

        //update fd only if msg recvd from some other process
        if (incoming_port != atoi((P->get_send_port_no(PID)).c_str()))
        {
            P->set_fd(incoming_port, new_fd);
        }

        cout << "pid=" << PID << " accepting connection from P" << P->get_port_pid_map(incoming_port) << endl;

        int child_pid = fork();
        if (child_pid == 0)
        {   // this is the child process
            exit(0);
        }
    }

    pthread_exit(NULL);
}

// function where process acts as client
// connect()ing to other processes
int Process::client(int _pid)
{
    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *clientinfo, *p;

    struct sigaction sa;
    int yes = 1;
    int rv;

    // set up addrinfo for client (i.e. self)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, send_port_no[PID].c_str(), &hints, &clientinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (p = clientinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind ERROR");
            continue;
        }

        break;
    }

    freeaddrinfo(clientinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    // set up addrinfo for server
    int numbytes;
    // char buf[MAXDATASIZE];
    struct addrinfo *servinfo;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, listen_port_no[_pid].c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            cerr << "pid=" << PID << " client: connect ERROR\n";
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    int outgoing_port = ntohs(return_port_no((struct sockaddr *)p->ai_addr));
    cout << "pid=" << PID << " Client: connecting to " << outgoing_port << endl ;
    freeaddrinfo(servinfo); // all done with this structure

    // if (_pid != PID)
    // {
    set_fd_by_pid(_pid, sockfd);
    // }

    cout << "pid=" <<  PID << " initiating connection to P" << _pid << endl;

}

// print function
void Process::print()
{
    for (int i = 0; i < N; ++i)
    {
        cout << delay[i] << " ";
    }
    cout << endl;
    for (int i = 0; i < N; ++i)
    {
        cout << PID << "->" << i << " ";
        for (int j = 0; j < unicast_time[i].size(); ++j)
        {
            cout << unicast_time[i][j] << " ";
        }
        cout << endl;
    }
}

// call connect() to all process with pid <= self.PID
void Process::initiate_connections()
{
    // every process initiates connect to every process with pid < self.pid, including self
    for (int i = 0; i <= PID; ++i)
    {
        client(i);
    }
}

// constructs msg
// appends sent_trace of parent process to the msg body
// msg are of the following format (without quotes)
// "P0:21 10 12 1 0"
// message body, followed by a space, followed by sent_trace of the send event of the message serialized
// message body format -- 'P' followed by processID followed by ':' followed by message index (local to each process)
// sent_trace is a space separated list of whole numbers (row0 row1), where each row is space separated list of elems
string Process::construct_msg(int _pid, int msg_counter, string &msg_body)
{
    stringstream ss;
    ss << PID;
    string msg = "P" + ss.str() + ":";
    ss.str("");
    ss << msg_counter;
    msg = msg + ss.str();

    msg_body = msg;

    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            ss.str("");
            ss << sent_trace[i][j];
            msg = msg + " " + ss.str();
        }
    }

    return msg;
}

// send message to self
void self_send(const char buf[MAXDATASIZE], int pid, Process* P)
{
    cout << "pid=" << PID << " Msg PSEUDO-RECV from P" << pid << "-" << buf <<  endl;

    int rcv_after_delay = time(NULL) - (P->start_time) + P->get_delay(pid);
    string msg_body;
    std::vector<std::vector<int> > sent_trace_msg(N, std::vector<int> (N));

    P->extract_sent_trace(string(buf), msg_body, sent_trace_msg);   //extract sent_trace stamped with the msg

    // no need to update recv_trace on receive at this moment.
    // recv_trace will be updated on delivery
    // P->vc_update_recv(vc_msg, PID); // update VC on receive event

    P->delay_receipt(msg_body, RECEIVE, pid, PID, -1, rcv_after_delay, -1, sent_trace_msg);
}

// thread to unicast messages according to unicast_time vector
void* start_unicast(void* _P)
{
    Process *P = (Process *)_P;

    P->start_time = time(NULL);
    // cout<<"P"<<PID<<" start_time="<<P->start_time<<endl;
    int msg_counter = 1;

    while (true)
    {
        for (int r = 0; r < N; ++r)
        {
            int n =  P->get_unicast_size(r);
            for (int j = 0; j < n; ++j)
            {
                if ((time(NULL) - (P->start_time)) == (P->get_unicast_time(r, j)))
                {
                    P->sent_trace_update_send(PID, r);

                    string msg_body;
                    string msg = P->construct_msg(PID, msg_counter, msg_body);

                    if (r == PID)   //special hack for sending message to self
                    {
                        self_send(msg.c_str(), PID, P);
                        continue;
                    }
                    if (send(P->get_fd(r), msg.c_str(), msg.size(), 0) == -1)
                    {
                        cerr << "pid=" << PID << " ERROR sending to process P" << j << endl;
                        exit(1);
                    }
                    else
                    {
                        cout << "pid=" << PID << " Sent msg to P" << r << "-" << msg << " at t=" << P->get_unicast_time(r, j) << endl;
                    }

                    //should ideally use time(NULL)-(P->start_time)
                    P->msg_handler(msg_body, SEND, PID, r, P->get_unicast_time(r, j), -1, -1);
                    // P->log_br(msg, PID, P->get_br_time(i));
                    msg_counter++;
                }
            }
        }
        usleep(1000 * 1000);
    }
    cout << "pid=" << PID << " start_unicast thread exiting..." << endl;
    pthread_exit(NULL);
}

// sent_trace update on send event
void Process::sent_trace_update_send(int s_pid, int r_pid)
{
    pthread_mutex_lock(&sent_trace_lock);
    sent_trace[s_pid][r_pid]++;
    pthread_mutex_unlock(&sent_trace_lock);
}

// sent_trace update on receive event
void Process::sent_trace_update_recv(std::vector<std::vector<int> > &sent_trace_msg)
{
    pthread_mutex_lock(&sent_trace_lock);
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            sent_trace[i][j] = max(sent_trace[i][j], sent_trace_msg[i][j]);
        }
    }
    pthread_mutex_unlock(&sent_trace_lock);
}

// extracts sent_trace vector and msg body from message
void Process::extract_sent_trace(string msg, string &body, std::vector<std::vector<int> > &sent_trace_msg)
{
    int n = msg.size();
    istringstream iss(msg);
    iss >> body;
    int temp;

    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            iss >> temp;
            sent_trace_msg[i][j] = temp;
        }
    }
}

// one thread for receiving messages from each process
void* receive(void* argument)
{
    Arg *A = (Arg*) argument;
    Process *P = A->P;
    int pid = A->pid;

    int numbytes;
    while (true)
    {
        char buf[MAXDATASIZE];
        if ((numbytes = recv(P->get_fd(pid), buf, MAXDATASIZE - 1, 0)) == -1)
        {
            cerr << "pid=" << PID << " ERROR in receiving for P" << pid << endl;
            exit(1);
        }
        else if (numbytes == 0)     //connection closed
        {
            cerr << "pid=" << PID << " Connection closed by P" << pid << endl;
            break;
        }
        else
        {
            buf[numbytes] = '\0';
            cout << "pid=" << PID << " Msg PSEUDO-RECV from P" << pid << "-" << buf <<  endl;

            int rcv_after_delay = time(NULL) - (P->start_time) + P->get_delay(pid);
            string msg_body;
            std::vector<std::vector<int> > sent_trace_msg(N, std::vector<int> (N));

            P->extract_sent_trace(string(buf), msg_body, sent_trace_msg);   //extract sent_trace stamped with the msg

            // no need to update recv_trace on receive at this moment.
            // recv_trace will be updated on delivery
            // P->vc_update_recv(vc_msg, PID); // update VC on receive event

            P->delay_receipt(msg_body, RECEIVE, pid, PID, -1, rcv_after_delay, -1, sent_trace_msg);
        }
        usleep(100 * 1000);
    }
    cout << "pid=" << PID << " Receive thread exiting for P" << pid << endl;
    pthread_exit(NULL);
}

// constructs MsgObj
// adds it to recv buffer
void Process::delay_receipt(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time, std::vector<std::vector<int> > &sent_trace_msg)
{
    MsgObj M(msg, type, source_pid, dest_pid, send_time, recv_time, delv_time, sent_trace_msg);

    pthread_mutex_lock(&recv_buf_lock);

    if (recv_buf.find(recv_time) != recv_buf.end()) // MsgObj already exists with this timestamp
    {
        recv_buf[recv_time].push_back(M);
    }
    else
    {
        std::vector<MsgObj> v;
        v.push_back(M);
        recv_buf.insert(make_pair(recv_time, v));
    }

    pthread_mutex_unlock(&recv_buf_lock);
}


// check if this message can be delivered immediately
// if yes, then deliver, add deliver event to msg log buffer
// else add msg to the delivery buf associated with causal delivery protocol
void Process::add_to_delv_buf(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time, std::vector<std::vector<int> > &sent_trace_msg)
{
    MsgObj M(msg, type, source_pid, dest_pid, send_time, recv_time, delv_time, sent_trace_msg);

    //check if msg can be delivered
    if (can_deliver(sent_trace_msg, source_pid))
    {
        cout << "pid=" << PID << " CAN DELV:" << M.msg << " went right through" << endl;
        deliver(M);

        // since cd was updated
        // call causal delivery handler function to check if some msg
        // in delivery buffer can now be delivered
        causal_delv_handler();
    }
    else
    {
        cout << "pid=" << PID << " DELV BUF:" << M.msg << " pushed in" << endl;
        // can't deliver at this moment
        // add message to the delivery buffer associated with causal delivery protocol
        pthread_mutex_lock(&delv_buf_lock);
        delv_buf.push_back(M);
        pthread_mutex_unlock(&delv_buf_lock);
    }
}

// takes a message's sent_trace and its source pid
// returns true if the message can be delivered
// otherwise returns false
bool Process::can_deliver(std::vector<std::vector<int> > &sent_trace_msg, int source_pid)
{
    bool ans = true;

    pthread_mutex_lock(&recv_trace_lock);
    if (recv_trace[source_pid] != sent_trace_msg[source_pid][PID] - 1)
        ans = false;

    if (ans)
    {
        for (int k = 0; k < N; ++k)
        {
            if (k == source_pid)
                continue;
            if (recv_trace[k] < sent_trace_msg[k][PID])
            {
                ans = false;
                break;
            }
        }
    }
    pthread_mutex_unlock(&recv_trace_lock);

    return ans;
}

// deliver message M
// adds deliver event to log buffer
// updates receive trace
// updates sent trace
void Process::deliver(MsgObj& M)
{
    cout << "pid=" << PID << " DELIVER-" << M.msg << endl;
    M.delv_time = time(NULL) - start_time;

    // add deliver event to log buffer
    msg_handler(M.msg, M.type, M.source_pid, M.dest_pid, M.send_time, M.recv_time, M.delv_time);

    //update recv trace
    pthread_mutex_lock(&recv_trace_lock);
    recv_trace[M.source_pid] = M.sent_trace[M.source_pid][M.dest_pid];
    pthread_mutex_unlock(&recv_trace_lock);

    sent_trace_update_recv(M.sent_trace); // update VC on deliver event (equiv to receive of VC protocol)

}

// iterates over all messages in delivery buffer
// and checks which messages can be delivered now
void Process::causal_delv_handler()
{

    std::list<MsgObj>::iterator it = delv_buf.begin();
    while (it != delv_buf.end())
    {
        if (can_deliver(it->sent_trace, it->source_pid))
        {
            deliver(*it);

            // remove this message from the deliver buffer
            pthread_mutex_lock(&delv_buf_lock);
            delv_buf.erase(it);
            pthread_mutex_unlock(&delv_buf_lock);

            // need to start from the beginning of the list
            // because delivery of this message render some prev message deliverable
            it = delv_buf.begin();
        }
        else
        {
            it++;
        }
    }

}

// constructs a MsgObj
// adds it to the log_buf
void Process::msg_handler(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time)
{
    MsgObj M(msg, type, source_pid, dest_pid, send_time, recv_time, delv_time);

    time_t indexing_time;
    if (type == SEND)
        indexing_time = send_time;
    if (type == RECEIVE)
        indexing_time = recv_time;
    if (type == DELIVER)
        indexing_time = delv_time;

    pthread_mutex_lock(&log_buf_lock);

    if (log_buf.find(indexing_time) != log_buf.end()) // MsgObj already exists with this timestamp
    {
        log_buf[indexing_time].push_back(M);
    }
    else
    {
        std::vector<MsgObj> v;
        v.push_back(M);
        log_buf.insert(make_pair(indexing_time, v));
    }

    pthread_mutex_unlock(&log_buf_lock);

}

// write event info to logfile
void write_to_log(string msg, int pid, time_t t, MsgObjType type)
{
    ofstream fout;
    stringstream ss;
    ss << pid;
    string logfile = LOG_FILE + ss.str() + ".txt";

    string temp;
    if (type == RECEIVE)
        temp = " REC ";
    if (type == DELIVER)
        temp = " DLR ";

    fout.open(logfile.c_str(), ios::app);
    fout << t << "\t";
    if (type == SEND)
    {
        fout << "p" << pid << "->p" << PID << " " << msg << endl;
    }
    else
    {
        fout << "p" << pid << temp << msg << endl;
    }
    fout.close();
}

// thread to poll log buffer periodically
// and write events to logfile according to their timestamp
void* logger(void* _P)
{
    Process *P = (Process *)_P;
    while (true)
    {
        // cout << PID << "LOGGER:size" << P->log_buf.size() << endl;
        pthread_mutex_lock(&log_buf_lock);

        if (!((P->log_buf).empty()))
        {
            std::map<time_t, vector<MsgObj> >::iterator mit = (P->log_buf).begin();

            if ((mit->first) == time(NULL) - (P->start_time))
            {
                int pid;
                time_t t;
                vector<MsgObj>::iterator it = (mit->second).begin();    //iterating over msgobjs
                while (it != (mit->second).end())
                {
                    if (it->type == SEND)
                    {
                        pid = it->source_pid;
                    }
                    if (it->type == RECEIVE)
                    {
                        pid = it->dest_pid;
                    }
                    if (it->type == DELIVER)
                    {
                        pid = it->dest_pid;
                    }
                    write_to_log(it->msg, pid, mit->first, it->type);

                    it++;
                }
                (P->log_buf).erase((P->log_buf).begin());
            }
        }
        pthread_mutex_unlock(&log_buf_lock);
        usleep(100 * 1000);
    }
    pthread_exit(NULL);
}

void* recv_buf_poller(void* _P)
{
    Process *P = (Process *)_P;
    while (true)
    {
        // cout << PID << "RECV_BUF:size" << P->recv_buf.size() << endl;
        pthread_mutex_lock(&recv_buf_lock);

        if (!((P->recv_buf).empty()))
        {
            std::map<time_t, vector<MsgObj> >::iterator mit = (P->recv_buf).begin();

            if ((mit->first) == time(NULL) - (P->start_time))
            {
                int pid;
                time_t t;
                vector<MsgObj>::iterator it = (mit->second).begin();    //iterating over msgobjs
                while (it != (mit->second).end())
                {
                    P->msg_handler(it->msg, RECEIVE, it->source_pid, it->dest_pid, -1, it->recv_time, -1);
                    // no need to update VC on receive at this moment.
                    // VC will be updated on delivery, because for VC, recv means actually delivered
                    // P->vc_update_recv(vc_msg, PID); // update VC on receive event

                    P->add_to_delv_buf(it->msg, DELIVER, it->source_pid, it->dest_pid, -1,
                                       it->recv_time, -1, it->sent_trace);

                    it++;
                }
                (P->recv_buf).erase((P->recv_buf).begin());
            }
        }
        pthread_mutex_unlock(&recv_buf_lock);
        usleep(100 * 1000);
    }
    pthread_exit(NULL);
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void usage()
{
    cout << "Bad command line arguments" << endl;
    cout << "USAGE: ./process processID" << endl;
    cout << "EXAMPLE USAGE: ./process 10" << endl;
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        return 1;
    }
    else
    {
        int pid = atoi(argv[1]);
        PID = pid;
    }

    Process *P = new Process;
    P->read_config(CONFIG_FILE);
    P->print();
    if (pthread_mutex_init(&fd_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&log_buf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&sent_trace_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&recv_trace_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&delv_buf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&recv_buf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_t server_thread;
    int rv = pthread_create(&server_thread, NULL, server, (void *)P);

    if (rv)
    {
        cout << "ERROR:unable to create thread for server:" << rv << endl;
        return 1;
    }

    cout << "pid=" << PID << " SLEEPING for 3 secs..." << endl;
    usleep(3000 * 1000);
    cout << "pid=" << PID << " Up and running once again..." << endl;


    P->initiate_connections();

    cout << "pid=" << PID << " SLEEPING for 5 secs to ensure all connections are established..." << endl;
    usleep(5000 * 1000);
    cout << "pid=" << PID << " Up and running once again..." << endl;

    pthread_t unicast_thread;
    rv = pthread_create(&unicast_thread, NULL, start_unicast, (void *)P);

    if (rv)
    {
        cout << "ERROR:unable to create thread for broadcast:" << rv << endl;
        return 1;
    }


    pthread_t receive_thread[N];
    Arg **A = new Arg*[N];
    for (int i = 0; i < N; ++i)
    {
        A[i] = new Arg;
        A[i]->P = P;
    }
    for (int i = 0; i < N; ++i)
    {
        A[i]->pid = i;
        rv = pthread_create(&receive_thread[i], NULL, receive, (void *)A[i]);

        if (rv)
        {
            cout << "ERROR:unable to create thread for receive for " << i << ":" << rv << endl;
            return 1;
        }
    }

    pthread_t logger_thread;
    rv = pthread_create(&logger_thread, NULL, logger, (void *)P);

    if (rv)
    {
        cout << "ERROR:unable to create thread for logging" << endl;
        return 1;
    }

    pthread_t recv_buf_thread;
    rv = pthread_create(&recv_buf_thread, NULL, recv_buf_poller, (void *)P);

    if (rv)
    {
        cout << "ERROR:unable to create thread for logging" << endl;
        return 1;
    }

    void *status;
    pthread_join(unicast_thread, &status);

    for (int i = 0; i < N; ++i)
    {
        pthread_join(receive_thread[i], &status);
    }

    pthread_join(recv_buf_thread, &status);
    pthread_join(logger_thread, &status);
    pthread_join(server_thread, &status);
    pthread_exit(NULL);
    return 0;
}