#include "process.h"
#define PID 0

Process::Process(): vc(), delay(N, 0), fd(N, 0), send_port_no(N), listen_port_no(N)
{
    port_pid_map.insert(make_pair(0, atoi(SEND_PORT0)));
    port_pid_map.insert(make_pair(1, atoi(SEND_PORT1)));
    port_pid_map.insert(make_pair(2, atoi(SEND_PORT2)));
    port_pid_map.insert(make_pair(3, atoi(SEND_PORT3)));
    port_pid_map.insert(make_pair(4, atoi(SEND_PORT4)));

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

// get sockaddr, IPv4 or IPv6:
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

void Process::set_fd(int incoming_port, int new_fd)
{
    fd[port_pid_map[incoming_port]] = new_fd;
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

    while (1)
    {   // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        // inet_ntop(their_addr.ss_family,
        //           return_in_addr((struct sockaddr *)&their_addr),
        //           s, sizeof s);
        int incoming_port = ntohs(P->return_in_addr((struct sockaddr *)&their_addr));
        printf("server: got connection from %d\n", incoming_port);

        if (!fork())
        {   // this is the child process
            close(sockfd); // child doesn't need the listener

            // add the fd for this process to fd vector for future communication
            P->set_fd(incoming_port, new_fd);
            // close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
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
            perror("client: connect");
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
    printf("client: connecting to %d\n", outgoing_port);

    freeaddrinfo(servinfo); // all done with this structure
    fd[port_pid_map[outgoing_port]] = sockfd;

    // if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    //     perror("recv");
    //     exit(1);
    // }

    // buf[numbytes] = '\0';

    // printf("client: received '%s'\n", buf);

    // close(sockfd);
}

void Process::print()
{
    for (int i = 0; i < N; ++i)
    {
        cout << delay[i] << " ";
    }
    cout << endl;
    for (int i = 0; i < br_time.size(); ++i)
    {
        cout << br_time[i] << " ";
    }
}

void Process::initiate_connections()
{
    // every process initiates connect to every process with pid < self.pid, including self
    for (int i = 0; i <= PID; ++i)
    {
        client(i);
    }
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
    // Process P;
    Process *P = new Process;
    P->read_config(CONFIG_FILE);
    // p.print();

    pthread_t server_thread;
    int thread_s = pthread_create(&server_thread, NULL, server, (void *)P);

    if (thread_s)
    {
        cout << "Error:unable to create thread," << thread_s << endl;
        return 1;
    }

    P->initiate_connections();

    pthread_exit(NULL);
    // Pserver();
    // p.initiate_connections();
    return 0;
}