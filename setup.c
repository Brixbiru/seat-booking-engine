#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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

int main() {
    int u_file = creat("users.txt", 0644); // Create users file
    if (u_file == -1) { 
        perror("Error creating users.txt"); 
        return 1; 
    }

    struct user admin = {1, "admin", "admin123", 0};
    write(u_file, &admin, sizeof(struct user));

    struct user customer = {2, "john", "john123", 1};
    write(u_file, &customer, sizeof(struct user));

    printf("Successfully created users.txt.\n");
    close(u_file); 

    // Create seats file
    int s_file = creat("seats.txt", 0644);
    if (s_file == -1) { 
        perror("Error creating seats.txt"); 
        return 1; 
    }

    struct seat temp_seat;
    for (int i = 1; i <= 20; i++) {
        temp_seat.seat_id = i;
        temp_seat.is_booked = 0;      
        temp_seat.booked_by_id = -1;  
        write(s_file, &temp_seat, sizeof(struct seat));
    }
    
    printf("Successfully created seats.txt.\n");
    close(s_file);

    return 0;
}