#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h> 
#include <signal.h>
#include <errno.h>
#include <sys/select.h> 

#define PORT 8080
#define MAX_CLIENTS 10

int active_socket;
int *client_pids; 

#if defined(__linux__)
union semun { 
    int val; 
    struct semid_ds *buf; 
    unsigned short *array; 
};
#endif

struct user { 
    int id; 
    char username[50]; 
    char password[50]; 
    int role; 
};

struct seat { 
    int seat_id; 
    int is_booked; 
    int booked_by_id; 
};

void send_layout(int conn_fd) {
    int map_file = open("seats.txt", O_RDONLY);
    struct seat curr_seat;
    char buffer[2048] = "\n--- LIVE SEAT MAP ---\n";
    char seat_str[64];
    
    for (int i = 1; i <= 20; i++) {
        read(map_file, &curr_seat, sizeof(struct seat));
        sprintf(seat_str, "[%s] %02d  ", curr_seat.is_booked ? "X" : " ", i);
        strcat(buffer, seat_str);
        
        if (i % 5 == 0) {
            strcat(buffer, "\n");
        }
    }
    
    strcat(buffer, "----------------------\n");
    write(conn_fd, buffer, strlen(buffer));
    close(map_file);
}

void broadcast_handler(int sig) {
    // to interrupt the read/select calls
}

void register_user(int conn_fd) {
    char new_uname[50] = {0};
    char new_upass[50] = {0};
    struct user fresh_user;
    
    write(conn_fd, "Enter New Username: ", 20);
    read(conn_fd, new_uname, sizeof(new_uname));
    new_uname[strcspn(new_uname, "\n")] = 0; 
    
    write(conn_fd, "Enter New Password: ", 20);
    read(conn_fd, new_upass, sizeof(new_upass));
    new_upass[strcspn(new_upass, "\n")] = 0; 

    int u_file = open("users.txt", O_RDWR | O_APPEND);
    
    // Setting up write lock for safe registration
    struct flock lock; 
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK; 
    lock.l_whence = SEEK_SET; 
    lock.l_start = 0; 
    lock.l_len = 0; 
    
    fcntl(u_file, F_SETLKW, &lock);
    off_t filesize = lseek(u_file, 0, SEEK_END);
    
    fresh_user.id = (filesize / sizeof(struct user)) + 1;
    strcpy(fresh_user.username, new_uname); 
    strcpy(fresh_user.password, new_upass); 
    fresh_user.role = 1;
    
    write(u_file, &fresh_user, sizeof(struct user));
    
    // Unlock and close
    lock.l_type = F_UNLCK; 
    fcntl(u_file, F_SETLK, &lock); 
    close(u_file);
    
    write(conn_fd, "Registration Successful! Reconnect to login.\n", 45);
}

void handle_client(int conn_fd, int semid) {
    active_socket = conn_fd;
    signal(SIGUSR1, broadcast_handler);

    // Tracking active PID in shared memory
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_pids[i] == 0) { 
            client_pids[i] = getpid(); 
            break; 
        }
    }

    char menu_opt[10] = {0};
    write(conn_fd, "Welcome! [1] Login [2] Register\nEnter Choice: ", 46);
    read(conn_fd, menu_opt, sizeof(menu_opt));
    
    if (atoi(menu_opt) == 2) { 
        register_user(conn_fd); 
        
        // Cleaning up PID before exiting
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_pids[i] == getpid()) {
                client_pids[i] = 0;
            }
        }
        return; 
    }
    
    char input_user[50] = {0};
    char input_pass[50] = {0};
    struct user db_user; 
    int is_auth = 0;

    write(conn_fd, "Enter Username: ", 16); 
    read(conn_fd, input_user, 50); 
    input_user[strcspn(input_user, "\n")] = 0;
    
    write(conn_fd, "Enter Password: ", 16); 
    read(conn_fd, input_pass, 50); 
    input_pass[strcspn(input_pass, "\n")] = 0;

    int u_file = open("users.txt", O_RDONLY);
    if (u_file != -1) {
        while (read(u_file, &db_user, sizeof(struct user)) > 0) {
            if (strcmp(db_user.username, input_user) == 0 && strcmp(db_user.password, input_pass) == 0) { 
                is_auth = 1; 
                break; 
            }
        }
        close(u_file);
    }

    if (is_auth) {
        // ADMIN LOGIC
        if (db_user.role == 0) {
            write(conn_fd, "Admin Login Successful\n", strlen("Admin Login Successful\n"));
            
            while(1) {
                char admin_menu[] = "\n--- ADMIN MENU ---\n[1] View Live Map\n[2] Unbook a Seat\n[0] Exit\nEnter Choice: ";
                write(conn_fd, admin_menu, strlen(admin_menu));

                char admin_opt[10] = {0};
                int bytes_read = read(conn_fd, admin_opt, sizeof(admin_opt));
                
                if (bytes_read < 0) { 
                    if (errno == EINTR) continue; 
                    break; 
                }
                if (bytes_read == 0 || atoi(admin_opt) == 0) {
                    break;
                }

                int choice = atoi(admin_opt);

                if (choice == 1) {
                    send_layout(conn_fd);
                } else if (choice == 2) {
                    write(conn_fd, "Enter seat number to unbook (1-20): ", 36);
                    char seat_input[10] = {0};
                    int b = read(conn_fd, seat_input, sizeof(seat_input));
                    
                    if (b < 0 && errno == EINTR) continue;

                    int seat_num = atoi(seat_input);
                    if (seat_num < 1 || seat_num > 20) {
                        write(conn_fd, "Invalid seat.\n", 14);
                        continue;
                    }

                    int s_file = open("seats.txt", O_RDWR);
                    struct seat curr_seat;
                    int offset = (seat_num - 1) * sizeof(struct seat);
                    
                    struct flock lock; 
                    memset(&lock, 0, sizeof(lock));
                    lock.l_type = F_WRLCK; 
                    lock.l_whence = SEEK_SET; 
                    lock.l_start = offset; 
                    lock.l_len = sizeof(struct seat);
                    
                    // Admin forces the lock (waits if someone is viewing it)
                    fcntl(s_file, F_SETLKW, &lock); 
                    
                    lseek(s_file, offset, SEEK_SET); 
                    read(s_file, &curr_seat, sizeof(struct seat));
                    
                    if (curr_seat.is_booked == 0) {
                        write(conn_fd, "\nSeat is already empty.\n", 24);
                    } else {
                        curr_seat.is_booked = 0; 
                        curr_seat.booked_by_id = -1; // Clear the seat
                        
                        lseek(s_file, offset, SEEK_SET); 
                        write(s_file, &curr_seat, sizeof(struct seat));
                        write(conn_fd, "\nSeat successfully unbooked\n", strlen("\nSeat successfully unbooked\n"));
                        
                        // Send live update to everyone
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (client_pids[i] != 0 && client_pids[i] != getpid()) {
                                kill(client_pids[i], SIGUSR1);
                            }
                        }
                    }
                    
                    lock.l_type = F_UNLCK; 
                    fcntl(s_file, F_SETLK, &lock); 
                    close(s_file);
                }
            }
        } 
        // CUSTOMER ROLE LOGIC
        else {
            write(conn_fd, "Login Successful\n", strlen("Login Successful\n"));
            
            while(1) { 
                send_layout(conn_fd);
                write(conn_fd, "Enter seat number (1-20) or 0 to exit: ", 39);

                char seat_input[10] = {0};
                int bytes_read = read(conn_fd, seat_input, sizeof(seat_input));
                
                if (bytes_read < 0) { 
                    if (errno == EINTR) continue; 
                    break; 
                }
                if (bytes_read == 0 || atoi(seat_input) == 0) {
                    break;
                }

                int seat_num = atoi(seat_input);
                if (seat_num < 1 || seat_num > 20) {
                    write(conn_fd, "Invalid seat.\n", 14);
                    continue;
                }

                int s_file = open("seats.txt", O_RDWR);
                struct seat curr_seat;
                int offset = (seat_num - 1) * sizeof(struct seat);
                
                struct flock lock; 
                memset(&lock, 0, sizeof(lock));
                lock.l_type = F_WRLCK; 
                lock.l_whence = SEEK_SET; 
                lock.l_start = offset; 
                lock.l_len = sizeof(struct seat);
                
                // Try to lock, but do not wait if it's already locked
                if (fcntl(s_file, F_SETLK, &lock) == -1) {
                    char locked_msg[] = "\n[!] SEAT LOCKED: Another user is looking at this seat right now!\n";
                    write(conn_fd, locked_msg, strlen(locked_msg));
                    close(s_file);
                    continue;
                }

                lseek(s_file, offset, SEEK_SET); 
                read(s_file, &curr_seat, sizeof(struct seat));

                if (curr_seat.is_booked == 1) {
                    write(conn_fd, "\nAlready booked.\n", 17);
                    lock.l_type = F_UNLCK; 
                    fcntl(s_file, F_SETLK, &lock); 
                    close(s_file);
                    continue;
                }

                char confirm_prompt[] = "\nSeat is available. Enter 'yes' to confirm (You have 10 seconds): ";
                write(conn_fd, confirm_prompt, strlen(confirm_prompt));

                fd_set readfds;
                struct timeval tv;
                tv.tv_sec = 10; 
                tv.tv_usec = 0;
                
                int retval;
                do {
                    FD_ZERO(&readfds);
                    FD_SET(conn_fd, &readfds);
                    retval = select(conn_fd + 1, &readfds, NULL, NULL, &tv);
                } while (retval == -1 && errno == EINTR); 

                if (retval == -1) {
                    lock.l_type = F_UNLCK; 
                    fcntl(s_file, F_SETLK, &lock); 
                    close(s_file);
                    continue;
                } else if (retval == 0) {
                    char timeout_msg[] = "\nTimeout! You took too long. Seat reservation cancelled.\n";
                    write(conn_fd, timeout_msg, strlen(timeout_msg));
                    lock.l_type = F_UNLCK; 
                    fcntl(s_file, F_SETLK, &lock); 
                    close(s_file);
                    continue;
                } else {
                    char confirm_buf[10] = {0};
                    read(conn_fd, confirm_buf, sizeof(confirm_buf));
                    confirm_buf[strcspn(confirm_buf, "\n")] = 0;

                    if (strcmp(confirm_buf, "yes") == 0 || strcmp(confirm_buf, "YES") == 0) {
                        curr_seat.is_booked = 1; 
                        curr_seat.booked_by_id = db_user.id;
                        
                        lseek(s_file, offset, SEEK_SET);
                        write(s_file, &curr_seat, sizeof(struct seat));
                        
                        write(conn_fd, "\nBooking Successful\n", strlen("\nBooking Successful\n"));

                        // Broadcast updates
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (client_pids[i] != 0 && client_pids[i] != getpid()) {
                                kill(client_pids[i], SIGUSR1);
                            }
                        }
                    } else {
                        write(conn_fd, "\nBooking Cancelled.\n", strlen("\nBooking Cancelled.\n"));
                    }
                }
                
                lock.l_type = F_UNLCK; 
                fcntl(s_file, F_SETLK, &lock); 
                close(s_file);
            }
        } 
    } else { 
        write(conn_fd, "Invalid credentials.\n", 21);
    }
    
    // Cleaning up PID on logout/exit
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_pids[i] == getpid()) {
            client_pids[i] = 0;
        }
    }
}

int main() {
    // Setup Shared Memory for Live Updates
    int shm_id = shmget(IPC_PRIVATE, sizeof(int) * MAX_CLIENTS, IPC_CREAT | 0666);
    client_pids = (int *)shmat(shm_id, NULL, 0);
    memset(client_pids, 0, sizeof(int) * MAX_CLIENTS);

    // Setup Semaphore
    key_t sem_key = ftok("seats.txt", 'S'); 
    int semid = semget(sem_key, 1, IPC_CREAT | 0666);
    union semun sem_opts; 
    sem_opts.val = 1; 
    semctl(semid, 0, SETVAL, sem_opts);

    // Setup Server Socket - Concurrent server
    int main_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    bind(main_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(main_server_fd, 5);
    
    printf("Server live on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_conn = accept(main_server_fd, (struct sockaddr *)&client_addr, &addr_len);
        
        if (client_conn < 0) {
            continue;
        }
        
        if (fork() == 0) { 
            // Child process handles the client
            close(main_server_fd); 
            handle_client(client_conn, semid); 
            close(client_conn); 
            exit(0); 
        }
        
        // Parent process closes the connected socket and keeps listening
        close(client_conn);
    }
    return 0;
}