#include "server.h"

// ?
static int the_number = 0;

// Buffer
char buffer[BUFFER_SIZE];

// Job Queue
List_t* jobs;

// Valid users
List_t* valid_users;

// Auctions
List_t* auctions;

// Mutexes
sem_t user_mutex;
sem_t auction_mutex;
sem_t auction_id_mutex;
sem_t job_mutex;

// Global vars
unsigned int num_threads;
unsigned int timer;
char* file_name;
unsigned int auctionid;


void* client_thread(void* client_data) {
    pthread_detach(pthread_self());

    user_data* cd = (user_data*)client_data;
    int clientfd = cd->fd;

    // Buffer for client job message
    char client_buffer[BUFFER_SIZE];
    //petr_header* ph = malloc(sizeof(petr_header));

    while (clientfd >= 0) {
        petr_header* ph = malloc(sizeof(petr_header));
        int valid = rd_msgheader(clientfd, ph);
        if (valid < 0) {
            break;
        }

        // Message type
        uint8_t msg_type = ph->msg_type;

        bzero(client_buffer, BUFFER_SIZE);
        // wait and read the message from client, copy it in buffer
        int received_size = read(clientfd, client_buffer, ph->msg_len);
        if (received_size < 0){
            printf("Receiving failed\n");
            exit(EXIT_FAILURE);
        }
        
        // put job information into job data struct
        job_data* job = malloc(sizeof(job_data));
        job->msg_type = msg_type;
        job->buffer = (char*)malloc(sizeof(ph->msg_len));
        strcpy(job->buffer, client_buffer);

        // push job into job queue
        sem_wait(&job_mutex);
        insertRear(jobs, (void*)job);
        sem_post(&job_mutex);
        
        // handle client closing connection
        if (msg_type == LOGOUT) {
            ph->msg_type = OK;
            ph->msg_len = 0;
            int wr = wr_msg(clientfd, ph, 0);
            write(clientfd, ph, 0);
            close(clientfd);
            break;
        }
    }
    return NULL;
}

void* job_thread(void* arg) {
    pthread_detach(pthread_self());

    while(1) {
        // Take a job from job queue
        job_data* job = (job_data*)removeFront(jobs);
        printf("%d\n", job->msg_type);
        /*if (job->msg_type == ANCREATE) {
            sem_wait(&auction_mutex);
            
            // Create new auction object to pass into auction list
            auction_data* auction = (auction_data*)malloc(sizeof(auction_data));
            auction->auctionid = auctionid;
            auction->creator = job->sender;
            auction->item = strtok(job->buffer, "\r\n");
            auction->ticks = timer;
            auction->highest_bid = 0;
            auction->highest_bidder = NULL;
            auction->watchers = (List_t*)malloc(sizeof(List_t));

            // Put auction into auction list
            printf("i hate debugging\n");
            insertRear(auctions, (void*)auction);
            
            // Increment auctionid
            auctionid++;

            sem_post(&auction_mutex);
        }

        else if (job->msg_type == ANLIST) {
            sem_wait(&auction_mutex);
            
            //
            node_t* curr = auctions->head;
            while (curr != NULL) {
                auction_data* auction = (auction_data*)curr->value;
                printf("%d\n", auctionid);
                curr = curr->next;
            }
            printf("this function works!\n");
            sem_post(&auction_mutex);
        }*/

    }
    return NULL;
}

void run_server(int server_port, int num_job_threads){
    int sockfd, clientfd;
    int retval = 0;
    socklen_t len;
    struct sockaddr_in servaddr, cli;
    pthread_t tid;

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

    // Job threads
    int i;
    for (i = 0; i < num_job_threads; i++) {
        pthread_create(&tid, NULL, job_thread, NULL);
    }

    // Now server is ready to listen and verification

    while(1)
    { 
        if ((listen(sockfd, 333)) != 0) {
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
            printf("Client connection accepted\n");
            // Read message header from new client
            petr_header* ph = malloc(sizeof(petr_header));
            retval = rd_msgheader(clientfd, ph);
            if (retval < 0) {
                printf("Receiving failed\n");
                exit(EXIT_FAILURE);
            }

            // Read username from new client
            bzero(buffer, BUFFER_SIZE);
            retval = read(clientfd, buffer, ph->msg_len);
            if (retval < 0) {
                printf("Receiving failed\n");
                exit(EXIT_FAILURE);
            }

            // Check the username and password
            char* client_usr = strtok(buffer, "\r\n");
            char* client_pw = strtok(NULL, "\r\n");
            printf("%s\n", client_usr);
            printf("%s\n", client_pw);

            user_data* valid = searchUsers(valid_users, client_usr);
            // Check to see if user is new
            if (valid == NULL) {
                // Create user_data struct to pass to client thread
                user_data* user = (user_data*)malloc(sizeof(user_data));
                user->username = malloc(sizeof(strlen(client_usr)));
                user->password = malloc(sizeof(strlen(client_pw)));
                strcpy(user->username, client_usr);
                strcpy(user->password, client_pw);
                user->fd = clientfd;

                // Add user to valid_users list
                insertRear(valid_users, (void*)user);

                // Create client thread
                ph->msg_type = OK;
                ph->msg_len = 0;
                int wr = wr_msg(clientfd, ph, 0);
                write(clientfd, ph, 0);
                pthread_create(&tid, NULL, client_thread, (void*)user);
            }
            
            // If not new, user exists, so check password
            else {
                // Check login info
                user_data* validate = validateLogin(valid_users, client_usr, client_pw);
                if (validate != NULL) {
                    validate->fd = clientfd;
                    
                    // Create client thread
                    ph->msg_type = OK;
                    ph->msg_len = 0;
                    int wr = wr_msg(clientfd, ph, 0);
                    write(clientfd, ph, 0);
                    pthread_create(&tid, NULL, client_thread, (void*)validate);
                }
                else {
                    close(clientfd);
                }
            }
        }
    }
    return;
}

int main(int argc, char* argv[]) {
    int opt;
    unsigned int port = 0;
    num_threads = 2;
    timer = 0;
    file_name = NULL;
    auctionid = 0;
    jobs = (List_t*)malloc(sizeof(List_t));
    jobs->head = NULL;
    jobs->length = 0;
    valid_users = (List_t*)malloc(sizeof(List_t));
    valid_users->head = NULL;
    valid_users->length = 0;
    auctions = (List_t*)malloc(sizeof(List_t));
    auctions->head = NULL;
    auctions->length = 0;

    // Initialize semaphores
    sem_init(&user_mutex, 0, 1);
    sem_init(&auction_mutex, 0, 1);
    sem_init(&auction_id_mutex, 0, 1);
    sem_init(&job_mutex, 0, 1);

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
    run_server(port, num_threads);

    return 0;
}
