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
sem_t buffer_mutex;
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
        if (msg_type == LOGOUT) {
            ph->msg_type = OK;
            ph->msg_len = 0;
            int wr = wr_msg(clientfd, ph, 0);
            //write(clientfd, ph, 0);
            cd->active = 0;
            close(clientfd);
            free(ph);
            break;
        }
        job_data* job = malloc(sizeof(job_data));
        job->msg_type = msg_type;
        job->buffer = (char*)malloc(BUFFER_SIZE);
        strcpy(job->buffer, client_buffer);
        job->senderfd = clientfd;
        job->sender = (char*)malloc(BUFFER_SIZE);
        strcpy(job->sender, cd->username);
        printf("Adding job to queue\n");

        // push job into job queue
        sem_wait(&job_mutex);
        insertRear(jobs, (void*)job);
        sem_post(&job_mutex);
        
        /*if (jobs->length > 0) {
            printf("from client: %d\n", jobs->length);
        }*/

        // handle client closing connection
    }
    return NULL;
}

// NOTE: Calling these functions still results in a timed-out,
// but they correctly print back to our server
void* job_thread(void* arg) {
    pthread_detach(pthread_self());

    char msg_buf[BUFFER_SIZE];

    while(1) {
        // Create header to send back to client
        petr_header* ph = malloc(sizeof(petr_header));
        bzero(msg_buf, BUFFER_SIZE);

        // Take a job from job queue
        sem_wait(&job_mutex);
        if (jobs->length > 0) {
            job_data* job = (job_data*)removeFront(jobs);
            printf("Found job\n");
             
            // createauction
            if (job->msg_type == ANCREATE) {
                sem_wait(&auction_mutex);
                if (job->buffer[0] == '\r') {
                    ph->msg_type = EINVALIDARG;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    free(job->buffer);
                    free(job->sender);
                    free(job);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                // Create new auction object to pass into auction list
                auction_data* auction = (auction_data*)malloc(sizeof(auction_data));
                auction->auctionid = auctionid;
                auction->creator = malloc(BUFFER_SIZE);
                auction->creator = job->sender;
                auction->item = strtok(job->buffer, "\r\n");
                auction->ticks = atoi(strtok(NULL, "\r\n"));
                if (auction->ticks < 1) {
                    ph->msg_type = EINVALIDARG;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    free(auction);
                    free(job->buffer);
                    free(job->sender);
                    free(job);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                printf("ticks: %d\n", auction->ticks);
                auction->highest_bid = 0;
                auction->highest_bidder = malloc(BUFFER_SIZE);
                auction->highest_bidder = "";
                auction->bin = atoi(strtok(NULL, "\r\n"));
                if (auction->bin < 0) {
                    ph->msg_type = EINVALIDARG;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    free(auction);
                    free(job->buffer);
                    free(job->sender);
                    free(job);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                printf("bin: %d\n", auction->bin);
                auction->watchers = (List_t*)malloc(sizeof(List_t));
                auction->watchers->length = 0;
                printf("Finished making the auction\n");

                // Put auction into auction list
                insertRear(auctions, (void*)auction);
                
                // Send ANCREATE back to server
                ph->msg_type = ANCREATE;
                sprintf(msg_buf, "%u", auctionid);
                ph->msg_len = strlen(msg_buf) + 1;
                int wr = wr_msg(job->senderfd, ph, msg_buf);
                //write(job->senderfd, ph, sizeof(ph));

                // Increment auctionid
                auctionid++;

                bzero(msg_buf, BUFFER_SIZE);
                free(ph);
                free(job);
                // Free mutex and continue
                sem_post(&auction_mutex);
                sem_post(&job_mutex);
                continue;
            }
            
            // auctionlist
            else if (job->msg_type == ANLIST) {
                sem_wait(&auction_mutex);
                bzero(msg_buf, BUFFER_SIZE);
                
                // need to add to file;
                node_t* curr = auctions->head;
                char temp[500];
                if (curr == NULL) {
                    ph->msg_type = ANLIST;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, msg_buf);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                while (curr != NULL) {
                    auction_data* auction = (auction_data*)curr->value;
                    sprintf(temp, "%d", auction->auctionid);
                    strcat(msg_buf, temp);
                    strcat(msg_buf, ";");
                    bzero(temp, 500);

                    strcat(msg_buf, auction->item);
                    strcat(msg_buf, ";");
                    
                    sprintf(temp, "%d", auction->bin);
                    strcat(msg_buf, temp);
                    strcat(msg_buf, ";");
                    bzero(temp, 500);

                    sprintf(temp, "%d", auction->watchers->length);
                    strcat(msg_buf, temp);
                    strcat(msg_buf, ";");
                    bzero(temp, 500);

                    sprintf(temp, "%d", auction->highest_bid);
                    strcat(msg_buf, temp);
                    strcat(msg_buf, ";");
                    bzero(temp, 500);

                    sprintf(temp, "%d", auction->ticks);
                    strcat(msg_buf, temp);
                    strcat(msg_buf, "\n");
                    strcat(msg_buf, "\0");
                    bzero(temp, 500);

                    curr = curr->next;
                }
                
                // Write message back to client
                ph->msg_type = ANLIST;
                ph->msg_len = strlen(msg_buf) + 1;
                int wr = wr_msg(job->senderfd, ph, msg_buf);

                bzero(msg_buf, BUFFER_SIZE);
                free(ph);
                free(job);
                sem_post(&auction_mutex);
                sem_post(&job_mutex);
                continue;
            }

            // watchauction
            else if (job->msg_type == ANWATCH) {
                sem_wait(&auction_mutex);
                int id = atoi(job->buffer);
                auction_data* found = searchAuctions(auctions, id);
                if (found == NULL) {
                    ph->msg_type = EANNOTFOUND;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                char temp[500];
                user_data* user = searchUsers(valid_users, job->sender);
                insertRear(found->watchers, user);

                bzero(msg_buf, BUFFER_SIZE);
                strcat(msg_buf, found->item);
                strcat(msg_buf, "\r\n");

                sprintf(temp, "%d", found->bin);
                strcat(msg_buf, temp);
                bzero(temp, 500);

                ph->msg_type = ANWATCH;
                ph->msg_len = strlen(msg_buf) + 1;
                int wr = wr_msg(job->senderfd, ph, msg_buf);

                bzero(msg_buf, BUFFER_SIZE);
                free(ph);
                free(job);
                sem_post(&auction_mutex);
                sem_post(&job_mutex);
                continue;
            }

            // leave auction
            else if (job->msg_type == ANLEAVE) {
                sem_wait(&auction_mutex);
                int id = atoi(job->buffer);
                auction_data* found = searchAuctions(auctions, id);
                if (found == NULL) {
                    ph->msg_type = EANNOTFOUND;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }
                removeWatcher(found->watchers, job->senderfd);
                ph->msg_type = OK;
                ph->msg_len = 0;
                int wr = wr_msg(job->senderfd, ph, NULL);
                free(ph);
                free(job);
                sem_post(&auction_mutex);
                sem_post(&job_mutex);
            }


            else if (job->msg_type == ANBID) {
                // Check to see if auction exists
                sem_wait(&auction_mutex);
                int id = atoi(strtok(job->buffer, "\r\n"));
                auction_data* found = searchAuctions(auctions, id);
                if (found == NULL) {
                    ph->msg_type = EANNOTFOUND;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }

                // Check to see if user is watching
                user_data* user = searchUsers(found->watchers, job->sender);
                if (user == NULL) {
                    ph->msg_type = EANDENIED;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }

                // Check to see if user created auction
                if (strcmp(found->creator,user->username) == 0) {
                    ph->msg_type = EANDENIED;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }

                // Check to see if bid is a valid bid
                int bid = atoi(strtok(NULL, "\r\n"));
                if (bid < found->highest_bid) {
                    ph->msg_type = EBIDLOW;
                    ph->msg_len = 0;
                    int wr = wr_msg(job->senderfd, ph, NULL);
                    sem_post(&auction_mutex);
                    sem_post(&job_mutex);
                    continue;
                }

                // Edit info in auction to reflect highest bidder
                found->highest_bid = bid;
                found->highest_bidder = malloc(sizeof(user->username));
                strcpy(found->highest_bidder, job->sender);
                ph->msg_type = OK;
                ph->msg_len = 0;
                int wr = wr_msg(job->senderfd, ph, NULL);
                
                free(ph);
                ph = malloc(sizeof(petr_header));
                // Create ANUPDATE to send to watchers
                ph->msg_type = ANUPDATE;
                char temp[500];
                sprintf(temp, "%d\r\n", id);
                strcat(msg_buf, temp);
                strcat(msg_buf, found->item);
                strcat(msg_buf, "\r\n");
                strcat(msg_buf, job->sender);
                bzero(temp, 500);
                sprintf(temp, "\r\n%d", bid);
                strcat(msg_buf, temp);
                ph->msg_len = strlen(msg_buf) + 1;
                
                // Send ANUPDATE to all watchers of current auction
                node_t* curr = found->watchers->head;
                while (curr != NULL) {
                    user_data* user = (user_data*)curr->value;
                    int fd = user->fd;
                    wr_msg(fd, ph, msg_buf);
                    curr = curr->next;
                }

                free(ph);
                free(job);
                sem_post(&auction_mutex);
                sem_post(&job_mutex);
                continue;
            }
            else if (job->msg_type == USRLIST) {
                sem_wait(&user_mutex);
                bzero(msg_buf, BUFFER_SIZE);
                node_t* curr = valid_users->head;
                while (curr != NULL) {
                    user_data* user = curr->value;
                    if (user->active == 1) {
                        strcat(msg_buf, user->username);
                        strcat(msg_buf, "\n");
                    }
                    curr = curr->next;
                }
                ph->msg_type = USRLIST;
                ph->msg_len = strlen(msg_buf) + 1;
                int wr = wr_msg(job->senderfd, ph, msg_buf);
                sem_post(&user_mutex);
                sem_post(&job_mutex);
                continue;
            }
        }
        sem_post(&job_mutex);
    }
    return NULL;
}

void* tick_thread(void* secs) {
    pthread_detach(pthread_self());

    unsigned int timer = *(unsigned int*)secs;
    // DEBUG MODE: tick on every STDIN from the server
    if (timer == 0) {
        while (1) {
            // If enter key is pressed, then tick
            if (getchar()) {
                sem_wait(&auction_mutex);
                node_t* curr = auctions->head;
                while (curr != NULL) {
                    auction_data* auction = (auction_data*)curr->value;
                    auction->ticks -= 1;
                    if (auction->ticks == 0) {
                        removeAuction(auctions, auction->auctionid);
                    }
                    curr = curr->next;
                }
                sem_post(&auction_mutex);
            }
        }
    }
    else {
        // Create a timer to get seconds passed
        time_t start, end;
        while (1) {
            time(&end);
            time_t elapsed = end - start;
            // If the timer amount has passed, then tick
            if (elapsed >= timer) {
                //printf("Ticking right now right here right now oh yea baby\n");
                start = end;
                sem_wait(&auction_mutex);
                node_t* curr = auctions->head;
                while (curr != NULL) {
                    auction_data* auction = (auction_data*)curr->value;
                    auction->ticks -= 1;
                    if (auction->ticks == 0) {
                        removeAuction(auctions, auction->auctionid);
                    }
                    curr = curr->next;
                }
                sem_post(&auction_mutex);
            }
        }
    }
    return NULL;
}

void run_server(int server_port, int num_job_threads){
    int sockfd, clientfd;
    int retval = 0;
    socklen_t len;
    struct sockaddr_in servaddr, cli;
    pthread_t tid;

    jobs = (List_t*)malloc(sizeof(List_t));
    jobs->head = NULL;
    jobs->length = 0;

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

    // Create job threads
    int i;
    for (i = 0; i < num_job_threads; i++) {
        pthread_create(&tid, NULL, job_thread, NULL);
    }

    // Create tick thread
    pthread_create(&tid, NULL, tick_thread, &timer);

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
                user->active = 1;

                // Add user to valid_users list
                insertRear(valid_users, (void*)user);

                // Create client thread
                ph->msg_type = OK;
                ph->msg_len = 0;
                int wr = wr_msg(clientfd, ph, 0);
                //write(clientfd, ph, 0);
                pthread_create(&tid, NULL, client_thread, (void*)user);
            }
            
            // If not new, user exists, so check password
            else {
                // If account is logged in
                if (valid->active == 1) {
                    ph->msg_type = EUSRLGDIN;
                    ph->msg_len = 0;
                    int wr = wr_msg(clientfd, ph, NULL);
                    continue;
                }
                // Check login info
                user_data* validate = validateLogin(valid_users, client_usr, client_pw);
                // If login info is a match, log them in (create client thread)
                if (validate != NULL) {
                    validate->fd = clientfd;
                    
                    // Create client thread
                    ph->msg_type = OK;
                    ph->msg_len = 0;
                    int wr = wr_msg(clientfd, ph, 0);
                    //write(clientfd, ph, 0);
                    pthread_create(&tid, NULL, client_thread, (void*)validate);
                }
                // User pw is wrong
                else {
                    ph->msg_type = EWRNGPWD;
                    ph->msg_len = 0;
                    int wr = wr_msg(clientfd, ph, NULL);
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
    auctionid = 1;
    //jobs = (List_t*)malloc(sizeof(List_t));
    //jobs->head = NULL;
    //jobs->length = 0;
    valid_users = (List_t*)malloc(sizeof(List_t));
    valid_users->head = NULL;
    valid_users->length = 0;
    auctions = (List_t*)malloc(sizeof(List_t));
    auctions->head = NULL;
    auctions->length = 0;

    // Initialize semaphores
    sem_init(&user_mutex, 0, 1);
    sem_init(&auction_mutex, 0, 1);
    sem_init(&buffer_mutex, 0, 1);
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
    FILE* fd;
    fd = fopen(file_name, "r");
    char line[BUFFER_SIZE];
    if (fd != NULL) {
        sem_wait(&auction_mutex);
        while (fgets(line, BUFFER_SIZE, fd) != NULL) {
            auction_data* auction = (auction_data*)malloc(sizeof(auction_data));
            auction->auctionid = auctionid;
            auction->creator = malloc(BUFFER_SIZE);
            auction->creator = "";
            // Line contains name
            line[strlen(line) - 2] = '\0';
            auction->item = malloc(sizeof(line));
            strcpy(auction->item, line);
            
            // Get time duration
            fgets(line, BUFFER_SIZE, fd);
            auction->ticks = atoi(strtok(line, "\n"));
            
            auction->highest_bid = 0;
            auction->highest_bidder = malloc(BUFFER_SIZE);
            auction->highest_bidder = "";
            
            // Get bin
            fgets(line, BUFFER_SIZE, fd);
            auction->bin = atoi(strtok(line, "\n"));
            auction->watchers = (List_t*)malloc(sizeof(List_t));
            auction->watchers->length = 0;

            // Skip over empty line
            fgets(line, BUFFER_SIZE, fd);
            insertRear(auctions, auction);
            auctionid++;
        }
        sem_post(&auction_mutex);
    }

    if (port == 0){
        fprintf(stderr, "ERROR: Port number for server to listen is not given\n");
        fprintf(stderr, USAGE_MSG);
        exit(EXIT_FAILURE);
    }
    run_server(port, num_threads);

    return 0;
}
