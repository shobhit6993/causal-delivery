#include "process.h"
#define PID 0
pthread_mutex_t fd_lock;
pthread_mutex_t msg_buf_lock;
pthread_mutex_t vc_lock;
pthread_mutex_t cd_lock;
pthread_mutex_t delv_buf_lock;

Process::Process(): vc(N, 0), cd(N, 0), delay(N, 0), fd(N, -1), send_port_no(N), listen_port_no(N)
{
    port_pid_map.insert(make_pair(atoi(SEND_PORT0), 0));
    port_pid_map.insert(make_pair(atoi(SEND_PORT1), 1));
    port_pid_map.insert(make_pair(atoi(SEND_PORT2), 2));
    // port_pid_map.insert(make_pair(atoi(SEND_PORT3), 3));
    // port_pid_map.insert(make_pair(atoi(SEND_PORT4), 4));

    send_port_no[0] = SEND_PORT0;
    send_port_no[1] = SEND_PORT1;
    send_port_no[2] = SEND_PORT2;
    // send_port_no[3] = SEND_PORT3;
    // send_port_no[4] = SEND_PORT4;

    listen_port_no[0] = LISTEN_PORT0;
    listen_port_no[1] = LISTEN_PORT1;
    listen_port_no[2] = LISTEN_PORT2;
    // listen_port_no[3] = LISTEN_PORT3;
    // listen_port_no[4] = LISTEN_PORT4;
}

// get port number
int Process::return_in_addr(struct sockaddr *sa)
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

    // cout<<"out_set_fd"<<incoming_port<<" "<<new_fd<<" "<<port_pid_map[incoming_port]<<" "<<fd[port_pid_map[incoming_port]]<<endl;
    pthread_mutex_lock(&fd_lock);
    if (fd[port_pid_map[incoming_port]] == -1)
    {
        // cout<<"in_set_fd"<<incoming_port<<" "<<new_fd<<endl;
        fd[port_pid_map[incoming_port]] = new_fd;
    }
    pthread_mutex_unlock(&fd_lock);
}

void Process::set_fd_by_pid(int _pid, int new_fd)
{

    // cout<<"out_set_fd"<<incoming_port<<" "<<new_fd<<" "<<port_pid_map[incoming_port]<<" "<<fd[port_pid_map[incoming_port]]<<endl;
    pthread_mutex_lock(&fd_lock);
    if (fd[_pid] == -1)
    {
        // cout<<"in_set_fd"<<incoming_port<<" "<<new_fd<<endl;
        fd[_pid] = new_fd;
    }
    pthread_mutex_unlock(&fd_lock);
}

time_t Process::get_br_time(int i)
{
    return br_time[i];
}

int Process::get_fd(int _pid)
{
    int ret;
    pthread_mutex_lock(&fd_lock);
    ret = fd[_pid];
    pthread_mutex_unlock(&fd_lock);
    return ret;
}

int Process::get_br_time_size()
{
    return br_time.size();
}

time_t Process::get_delay(int _pid)
{
    return delay[_pid];
}

void Process::read_config(string filename)
{
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try
    {
        fin.open(filename.c_str());

        int i = 0, d;
        while (i < N && !fin.eof())
        {
            for (int j = 0; j < N; ++j)
            {
                fin >> d;
                if (i == PID)
                {
                    delay[j] = d;
                }
            }

            i++;
        }

        i = 0;
        int p, t;
        string garbage, line;

        while (i < N && !fin.eof())
        {
            fin >> p;
            fin >> garbage >> garbage;
            std::getline(fin, line);

            if (p == PID)
            {
                std::istringstream iss(line);
                while (iss >> t)
                {
                    br_time.push_back(t);
                }
                break;
            }

            i++;
        }

        sort(br_time.begin(), br_time.end());

    }
    catch (ifstream::failure e)
    {
        cerr << e.what();
    }

    fin.close();
}

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
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
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
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    cout << "p" << PID << " server: waiting for connections...\n";
    cout << "p" << PID << "server sockfd=" << sockfd << endl;
    while (1)
    {   // main accept() loop
        sin_size = sizeof their_addr;

        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        int incoming_port = ntohs(P->return_in_addr((struct sockaddr *)&their_addr));
        printf("server %d: got connection from %d\n", PID, incoming_port);

        //update fd only if msg recvd from some other process
        if (incoming_port != atoi((P->get_send_port_no(PID)).c_str()))
        {
            P->set_fd(incoming_port, new_fd);
        }

        cout << "P" << PID << "accepting connection from P" << P->get_port_pid_map(incoming_port) << "using sockfd=" << new_fd << " but using:" << P->get_fd(P->get_port_pid_map(incoming_port)) << endl;

        int child_pid = fork();
        if (child_pid == 0)
        {   // this is the child process
            // close(sockfd); // child doesn't need the listener
            // cout << "FD:A" << new_fd << endl;
            // if (send(new_fd, "Hello", ST_BR_MSG.size(), 0) == -1)
            //     perror("send");

            // cout << "FD:B" << new_fd << endl;

            // P->set_fd(incoming_port, new_fd);

            // cout << "FD:C" << P->get_fd(P->get_port_pid_map(incoming_port)) << endl;
            // usleep(100000 * 1000);

            // close(new_fd);
            exit(0);
        }
    }

    pthread_exit(NULL);
}

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
            perror("client: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind");
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
            cerr << "P" << PID << "client: connect error\n";
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    // inet_ntop(p->ai_family, return_in_addr((struct sockaddr *)p->ai_addr),
    //           s, sizeof s);
    int outgoing_port = ntohs(return_in_addr((struct sockaddr *)p->ai_addr));
    printf("client %d: connecting to %d\n", PID, outgoing_port);
    freeaddrinfo(servinfo); // all done with this structure

    // if (_pid != PID)
    // {
    set_fd_by_pid(_pid, sockfd);
    // }

    cout << "P" << PID << "initiating connection to P" << _pid << "using sockfd=" << sockfd << " but using:" << get_fd(_pid) << endl;

}

void Process::print()
{
    cout << PID << "..";
    for (int i = 0; i < N; ++i)
    {
        cout << fd[i] << " ";
    }
    cout << endl;
    // for (int i = 0; i < N; ++i)
    // {
    //     cout << delay[i] << " ";
    // }
    // cout << endl;
    // for (int i = 0; i < br_time.size(); ++i)
    // {
    //     cout << br_time[i] << " ";
    // }
}

void Process::initiate_connections()
{
    // every process initiates connect to every process with pid < self.pid, including self
    for (int i = 0; i <= PID; ++i)
    {
        client(i);
    }
}

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
        ss.str("");
        ss << vc[i];
        msg = msg + " " + ss.str();
    }

    return msg;
}

void self_send(const char buf[MAXDATASIZE], int pid, Process* P)
{
    cout << PID << "####" << "Msg rcvd from P" << pid << "-" << buf << "on sockfd=" << P->get_fd(pid) << endl;

    int rcv_after_delay = time(NULL) - (P->start_time) + P->get_delay(pid);
    string msg_body;
    std::vector<int> vc_msg;

    P->extract_vc(string(buf), msg_body, vc_msg);   //extract VC stamped with the msg

    // add receive event to msg log buf
    P->msg_handler(msg_body, RECEIVE, pid, PID, -1, rcv_after_delay, -1);
    P->vc_update_recv(vc_msg, PID); // update VC on receive event

    // check if this message can be delivered immediately
    // if yes, then deliver, add deliver event to msg log buffer
    // else add msg to the delivery buf associated with causal delivery protocol
    P->add_to_delv_buf(msg_body, DELIVER, pid, PID, -1, rcv_after_delay, -1, vc_msg);
    // P->log_rcv(string(buf), PID, time(NULL) - (P->start_time));
}

void* start_broadcast(void* _P)
{
    Process *P = (Process *)_P;

    P->start_time = time(NULL);
    // cout<<"P"<<PID<<" start_time="<<P->start_time<<endl;
    int i = 0, msg_counter = 1;
    int n = P->get_br_time_size();
    while (i < n)
    {
        // cout<<"t="<<time(NULL) - (P->start_time)<<endl;
        if ((time(NULL) - (P->start_time)) == (P->get_br_time(i)))
        {
            // update vc before send so that updated vc can be timestamped with sent msg
            // this is not the ideal way, but we assume that all sends are perfect
            // if send command below fails, we exit anyway.
            P->vc_update_send(PID);
            string msg_body;
            string msg = P->construct_msg(PID, msg_counter, msg_body);

            for (int j = 0; j < N; ++j)
            {
                if (j == PID)   //special hack for sending message to self
                {
                    self_send(msg.c_str(), PID, P);
                    continue;
                }
                cout << PID << "#" << "P" << j << P->get_fd(j) << endl;
                if (send(P->get_fd(j), msg.c_str(), msg.size(), 0) == -1)
                {
                    cerr << "error sending to process P" << j << endl;
                    exit(1);
                }
                else
                {
                    cout << PID << "#" << "Sent msg from P" << PID << " to P" << j << "-" << msg << "using sockfd=" << P->get_fd(j) << endl;
                }
            }
            //should ideally use time(NULL)-(P->start_time)
            P->msg_handler(msg_body, SEND, PID, -1, P->get_br_time(i), -1, -1);
            // P->log_br(msg, PID, P->get_br_time(i));
            msg_counter++;
            i++;
        }
        usleep(100 * 1000);
    }
    cout << PID << "#" << "start_broadcast thread exiting" << endl;
    // usleep(100000 * 1000);

    pthread_exit(NULL);
}

void Process::vc_update_send(int _pid)
{
    pthread_mutex_lock(&vc_lock);
    vc[_pid]++;
    pthread_mutex_unlock(&vc_lock);
}

void Process::vc_update_recv(std::vector<int> &vc_msg, int _pid)
{
    pthread_mutex_lock(&vc_lock);
    for (int i = 0; i < N; ++i)
    {
        vc[i] = max(vc[i], vc_msg[i]);
    }
    vc[_pid]++;
    pthread_mutex_unlock(&vc_lock);
}

void Process::extract_vc(string msg, string &body, std::vector<int> &vc_msg)
{
    cout << msg << endl;
    int n = msg.size();
    istringstream iss(msg);
    iss >> body;
    cout << body << endl;
    int temp;
    while (iss >> temp)
    {
        vc_msg.push_back(temp);
    }
    cout << endl;
}

void* receive(void* argument)
{
    Arg *A = (Arg*) argument;
    Process *P = A->P;
    int pid = A->pid;

    int numbytes;
    while (true)
    {
        char buf[MAXDATASIZE];
        cout << PID << "#" << "Stuck at" << pid << "looking at " << P->get_fd(pid) << endl;
        // memset(buf, '\0', MAXDATASIZE);
        if ((numbytes = recv(P->get_fd(pid), buf, MAXDATASIZE - 1, 0)) == -1)
        {
            cerr << PID << "#" << "error in receiving for P" << pid << endl;
            exit(1);
        }
        else if (numbytes == 0)     //connection closed
        {
            cerr << PID << "#" << "Connection closed by P" << pid << endl;
            break;
        }
        else
        {
            buf[numbytes] = '\0';
            cout << PID << "#" << "Msg rcvd from P" << pid << "-" << buf << "on sockfd=" << P->get_fd(pid) << endl;

            int rcv_after_delay = time(NULL) - (P->start_time) + P->get_delay(pid);
            string msg_body;
            std::vector<int> vc_msg;

            P->extract_vc(string(buf), msg_body, vc_msg);   //extract VC stamped with the msg

            // add receive event to msg log buf
            P->msg_handler(msg_body, RECEIVE, pid, PID, -1, rcv_after_delay, -1);
            P->vc_update_recv(vc_msg, PID); // update VC on receive event

            // check if this message can be delivered immediately
            // if yes, then deliver, add deliver event to msg log buffer
            // else add msg to the delivery buf associated with causal delivery protocol
            P->add_to_delv_buf(msg_body, DELIVER, pid, PID, -1, rcv_after_delay, -1, vc_msg);
            // P->log_rcv(string(buf), PID, time(NULL) - (P->start_time));
        }
        usleep(100 * 1000);
    }
    cout << PID << "#" << "receive thread exiting for P" << pid << endl;
    pthread_exit(NULL);
}

void Process::add_to_delv_buf(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time, std::vector<int> &vc_msg)
{
    MsgObj M(msg, type, source_pid, dest_pid, send_time, recv_time, delv_time, vc_msg);

    //check if msg can be delivered
    if (can_deliver(vc_msg, source_pid))
    {
        cout << "CAN DELV:" << M.msg << "went right through" << endl;
        deliver(M);

        // since cd was updated
        // call causal delivery handler function to check if some msg
        // in delivery buffer can now be delivered
        causal_delv_handler();
    }
    else
    {
        cout << "DELV BUF:" << M.msg << "pushed in" << endl;
        // can't deliver at this moment
        // add message to the delivery buffer associated with causal delivery protocol
        pthread_mutex_lock(&delv_buf_lock);
        delv_buf.push_back(M);
        pthread_mutex_unlock(&delv_buf_lock);
    }
}

// takes a message's VC and it's source pid
// returns true if the message can be delivered
// otherwise returns false
bool Process::can_deliver(std::vector<int> &vc_msg, int source_pid)
{
    bool ans = true;

    cout << "CAN DELIVER" << PID << endl;
    PR(cd[source_pid])
    PR(vc_msg[source_pid])

    pthread_mutex_lock(&cd_lock);
    if (cd[source_pid] != vc_msg[source_pid] - 1)
        ans = false;

    if (ans)
    {
        for (int k = 0; k < N; ++k)
        {
            if (k == source_pid)
                continue;
            PR(k)
            PR(cd[k])
            PR(vc_msg[k])
            if (cd[k] < vc_msg[k])
            {
                ans = false;
                break;
            }
        }
    }
    pthread_mutex_unlock(&cd_lock);

    return ans;
}

// deliver message M
// adds deliver event to log buffer
// updates cd
void Process::deliver(MsgObj& M)
{
    cout << "DELIVER:" << M.msg << endl;
    M.delv_time = time(NULL) - start_time;

    // add deliver event to log buffer
    msg_handler(M.msg, M.type, M.source_pid, M.dest_pid, M.send_time, M.recv_time, M.delv_time);

    //update cd
    pthread_mutex_lock(&cd_lock);
    cd[M.source_pid] = M.vc[M.source_pid];
    pthread_mutex_unlock(&cd_lock);
}

// iterates over all messages in delivery buffer
// and checks which messages can be delivered now
void Process::causal_delv_handler()
{
    pthread_mutex_lock(&delv_buf_lock);

    std::list<MsgObj>::iterator it = delv_buf.begin();
    while (it != delv_buf.end())
    {
        if (can_deliver(it->vc, it->source_pid))
        {
            deliver(*it);

            // remove this message from the deliver buffer
            delv_buf.erase(it);

            // need to start from the beginning of the list
            // because delivery of this message render some prev message deliverable
            it = delv_buf.begin();
        }
        else
        {
            it++;
        }
    }

    pthread_mutex_lock(&delv_buf_lock);
}

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

    pthread_mutex_lock(&msg_buf_lock);

    if (msg_buf.find(indexing_time) != msg_buf.end()) // MsgObj already exists with this timestamp
    {
        msg_buf[indexing_time].push_back(M);
    }
    else
    {
        std::vector<MsgObj> v;
        v.push_back(M);
        msg_buf.insert(make_pair(indexing_time, v));
    }

    pthread_mutex_unlock(&msg_buf_lock);

}

// void Process::log_br(string msg, int pid, time_t t)
// {
//     ofstream fout;
//     stringstream ss;
//     ss << pid;
//     string logfile = LOG_FILE + ss.str() + ".txt";

//     fout.open(logfile.c_str(), ios::app);
//     fout << t << "\t";
//     fout << "p" << pid << " BRC " << msg << endl;
//     fout.close();
// }

// void Process::log_rcv(string msg, int pid, time_t t)
// {
//     ofstream fout;
//     stringstream ss;
//     ss << pid;
//     string logfile = LOG_FILE + ss.str() + ".txt";

//     fout.open(logfile.c_str(), ios::app);
//     fout << t << "\t";
//     fout << "p" << pid << " REC " << msg << endl;
//     fout.close();
// }

void write_to_log(string msg, int pid, time_t t, MsgObjType type)
{
    ofstream fout;
    stringstream ss;
    ss << pid;
    string logfile = LOG_FILE + ss.str() + ".txt";

    string temp;
    if (type == SEND)
        temp = " BRC ";
    if (type == RECEIVE)
        temp = " REC ";
    if (type == DELIVER)
        temp = " DLR ";

    fout.open(logfile.c_str(), ios::app);
    fout << t << "\t";
    fout << "p" << pid << temp << msg << endl;
    fout.close();
}

void* logger(void* _P)
{
    Process *P = (Process *)_P;
    while (true)
    {
        cout << PID << "LOGGER:size" << P->msg_buf.size() << endl;
        pthread_mutex_lock(&msg_buf_lock);

        if (!((P->msg_buf).empty()))
        {
            std::map<time_t, vector<MsgObj> >::iterator mit = (P->msg_buf).begin();

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
                        cout << "LOGGER: deliver case" << endl;
                    }
                    write_to_log(it->msg, pid, mit->first, it->type);

                    it++;
                }
                (P->msg_buf).erase((P->msg_buf).begin());
            }
        }
        pthread_mutex_unlock(&msg_buf_lock);
        usleep(500 * 1000);
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

int main(int argc, char const *argv[])
{
    Process *P = new Process;
    P->read_config(CONFIG_FILE);

    if (pthread_mutex_init(&fd_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&msg_buf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&vc_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&cd_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&delv_buf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }


    pthread_t server_thread;
    int rv = pthread_create(&server_thread, NULL, server, (void *)P);

    if (rv)
    {
        cout << "Error:unable to create thread for server:" << rv << endl;
        return 1;
    }

    cout << "Sleeping for 3 secs" << endl;
    usleep(3000 * 1000);
    cout << "Up and running once again" << endl;

    void *status;

    P->initiate_connections();
    usleep(5000 * 1000);
    P->print();

    pthread_t broadcast_thread;
    rv = pthread_create(&broadcast_thread, NULL, start_broadcast, (void *)P);

    if (rv)
    {
        cout << "Error:unable to create thread for broadcast:" << rv << endl;
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

        // if (i == PID)
        // {
        //     // cout << "Not creating thread for receving from self..." << endl;
        //     continue;
        // }
        A[i]->pid = i;
        rv = pthread_create(&receive_thread[i], NULL, receive, (void *)A[i]);

        if (rv)
        {
            cout << "Error:unable to create thread for receive for " << i << ":" << rv << endl;
            return 1;
        }
    }

    pthread_t logger_thread;
    rv = pthread_create(&logger_thread, NULL, logger, (void *)P);

    if (rv)
    {
        cout << "Error:unable to create thread for logging" << endl;
        return 1;
    }

    for (int i = 0; i < N; ++i)
    {
        pthread_join(receive_thread[i], &status);
    }
    pthread_join(broadcast_thread, &status);
    pthread_join(logger_thread, &status);
    pthread_join(server_thread, &status);
    pthread_exit(NULL);
    return 0;
}