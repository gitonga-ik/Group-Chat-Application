#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "server.h"
#include "data_transfer.h"

volatile sig_atomic_t server_running = 1;
int s_socket;

void handle_shutdown(int signal) {
    server_running = 0;
    close(s_socket);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, handle_shutdown);

    const ServerConfig server = server_init(9000);
    thread_args args;
    s_socket = server.server_socket;
    args.s_socket = server.server_socket;

    printf("Server running... Press Ctrl+C to stop.\n");
    printf("Mode: iterative, connectionless (UDP).\n");

    while(server_running) {
        struct sockaddr_in client_addr;
        char *msg = receive_udp_message(s_socket, &client_addr);
        if (!msg) continue;

        // allocate args per thread to avoid race condition
        thread_args *args = malloc(sizeof(thread_args));
        if (!args) { free(msg); continue; }

        args->s_socket = s_socket;
        args->client_address = client_addr;
        args->msg = msg; // thread owns msg now, it must free it

        printf("[READ] %s:%d -> %s\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               msg);

        pthread_t thread_id;
        printf("[PROCESS] Spawning thread for datagram.\n");

        // free args+msg only on failure; thread owns them on success
        if (pthread_create(&thread_id, NULL, route_packet, args) != 0) {
            perror("pthread_create failed");
            free(msg);
            free(args);
        } else {
            pthread_detach(thread_id);
        }
    }

    printf("Closing server socket...\n");
    printf("Server shutdown complete.\n");
    return 0;
}
