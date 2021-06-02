#include "server.h"

// ?
static int the_number = 0;

// Buffer
char buffer[BUFFER_SIZE];

// Job Queue
List_t* jobs;

// Mutexes
sem_t user_mutex;
sem_t auction_mutex;
sem_t auction_id_mutex;
sem_t job_mutex;

void client_thread(user_data* client_data) {
    int clientfd = client_data->fd;

    while (clientfd >= 0) {
        petr_header* ph = malloc(sizeof(petr_header));
        int valid = rd_msgheader(clientfd, ph);
        if (valid < 0) {
            exit(EXIT_FAILURE);
        }

        // Message type
        uint8_t msg_type = ph->msg_type;

        bzero(buffer, BUFFER_SIZE);
        // wait and read the message from client, copy it in buffer
        int received_size = read(clientfd, buffer, ph->msg_len);
        if (received_size < 0){
            printf("Receiving failed\n");
            exit(EXIT_FAILURE);
        }
        
        // put job information into job data struct
        job_data* job = malloc(sizeof(job_data));
        job->msg_type = msg_type;
        job->buffer = buffer;

        // push job into job queue
        insertRear(jobs, (void*)job);
        printf("reached this line\n");
        
        // handle client closing connection
        if (msg_type == LOGOUT) {
            close(clientfd);
        }
    }

}

void run_server(int server_port){
    int sockfd, clientfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully created\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully binded\n");

    // Now server is ready to listen and verification

    while(1)
    { 
        if ((listen(sockfd, 1)) != 0) {
            printf("Listen failed\n");
            exit(EXIT_FAILURE);
        }
        else {
            printf("Server listening on port: %d.. Waiting for connection\n", server_port);
        }
        len = sizeof(cli);
        clientfd = accept(sockfd, (SA*)&cli, &len);
        // Wait and Accept the connection from client
        if (clientfd < 0) {
            printf("server acccept failed\n");
            close(clientfd);
            exit(EXIT_FAILURE);
        }
        else {
            printf("Client connetion accepted\n");
        }

        // Create user_data struct to pass to client thread
        user_data* user = malloc(sizeof(user_data*));
        user->fd = clientfd;

        // Client thread
        client_thread(user);

        // Close the socket at the end
        close(clientfd);
        return;
    }
}

int main(int argc, char* argv[]) {
    int opt;
    unsigned int port = 0;
    unsigned int num_threads = 2;
    unsigned int timer = 0;
    char* file_name = NULL;
    jobs = (List_t*)malloc(sizeof(List_t));

    while ((opt = getopt(argc, argv, ":h::j:N::t:M:")) != -1) {
        switch (opt) {
        case 'h':
            printf(USAGE_MSG);
            return EXIT_SUCCESS;
        case 'j':
            fprintf(stderr, "Case j arg is: %s", optarg);
            num_threads = atoi(optarg);
            break;
        case 't':
            fprintf(stderr, "Case t arg is: %s", optarg);
            timer = atoi(optarg);
            break;
        default: /* '?' */
            fprintf(stderr, "Server Application Usage: %s -p <port_number>\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    port = atoi(argv[argc - 2]);
    file_name = argv[argc - 1];

    if (port == 0){
        fprintf(stderr, "ERROR: Port number for server to listen is not given\n");
        fprintf(stderr, USAGE_MSG);
        exit(EXIT_FAILURE);
    }
    run_server(port);

    return 0;
}
