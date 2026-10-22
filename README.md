# Concurrent Seat Booking Engine

A multi-client seat booking system written in C for Linux. Clients connect over TCP sockets, log in or register, and book seats from a 20-seat map. The server handles each client in a separate process and keeps all connected users up to date when seats change.

## Features

- Concurrent client handling with `fork()`
- Login and registration with separate customer and admin roles
- Seat booking with a 10-second confirmation window (using `select()`)
- Seat locking with `fcntl()` byte-range locks, so two users can't book the same seat
- Admin tools to view the live seat map and unbook seats
- Live updates to all connected clients using shared memory and `SIGUSR1`

## Files

| File        | Description                                              |
|-------------|----------------------------------------------------------|
| `server.c`  | Multi-client server that handles login, booking, and admin actions |
| `client.c`  | Terminal client that connects to the server              |
| `setup.c`   | Creates the initial `users.txt` and `seats.txt` data files |

## Requirements

- Linux
- GCC

## Build

```bash
gcc setup.c -o setup
gcc server.c -o server
gcc client.c -o client
```

## Usage

1. Create the data files (run once):

```bash
   ./setup
```

   This creates `users.txt` with two default accounts and `seats.txt` with 20 empty seats.

2. Start the server:

```bash
   ./server
```

   The server listens on port `8080`.

3. In another terminal, start a client:

```bash
   ./client
```

4. Choose **Login** or **Register** from the menu.

### Default accounts

| Username | Password  | Role     |
|----------|-----------|----------|
| admin    | admin123  | Admin    |
| john     | john123   | Customer |

Admins can view the live seat map and unbook seats. Customers can book available seats.

## Notes

- The server and client both run on `127.0.0.1` port `8080` by default.
- Running `./setup` again overwrites `users.txt` and `seats.txt`, which erases all bookings and registered users.
- `users.txt` and `seats.txt` are generated data files, so they aren't included in the repository.
