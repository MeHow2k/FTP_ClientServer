#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#endif

#define SIZE 4096// rozmiar bufora

int connections = 0;//liczba wykonanych połączeń
char *file_path;  // Zmienna globalna do przechowywania ścieżki do pliku

void send_file(FILE *fp, int sockfd) {
    int n;
    char data[SIZE];//dane
    uint64_t progress=0;
       // Wysyłanie długości nazwy pliku(ścieżki)
    uint32_t filename_size = strlen(file_path);
	if (send(sockfd, &filename_size, sizeof(uint32_t), 0) == -1) {
	    perror("[!]Error sending file name size.");      
	}
    // Wysyłanie nazwy pliku
    if (send(sockfd, file_path, filename_size, 0) == -1) {
        perror("[!]Error sending file name.");       
    }

  printf("[i]Sending file: %s\n", file_path);

    // Odczytaj rozmiar pliku
    fseek(fp, 0, SEEK_END);
    uint64_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

   // Wysyłanie rozmiaru pliku
    if (send(sockfd, &file_size, sizeof(uint64_t), 0) == -1) {
        perror("[!]Error sending file size.");
    }
    printf("File size: %zu bytes\n", file_size);
    uint64_t full_size = file_size;
    // Wysyłanie pliku blokami
    while ((n = fread(data, 1, SIZE, fp)) > 0) {
        
        progress += n;
        // Aktualizacja progresu w terminalu
        long percent = progress * 100 / full_size;
        printf("\rSending file:  %ld / %ld bytes. Progress: %ld %%", (long)progress, (long)full_size, percent);
        fflush(stdout);
        if (send(sockfd, data, n, 0) == -1) {
            perror("[!]Error sending file data.");
            break;
        }
    }
    printf("\n");
}
#ifdef _WIN32
DWORD WINAPI handle_connection(void* arg) {//utworzenie wątku dla windows
#else
void* handle_connection(void* arg) {//utworzenie wątku dla linux
#endif
    int new_sock = *((int *)arg);

    // Otwórz plik do wysłania
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        perror("[!]Error in reading file.");
        fprintf(stderr, "Cannot open file: %s\n", file_path);
        close(new_sock);
        exit(1);
#ifdef _WIN32
        return 1;
#else
        pthread_exit(NULL);
#endif
    }

    send_file(fp, new_sock);

    connections++;
    printf("[+]Data sent successfully. Connection No. %d ended.\n", connections);

    fclose(fp);
    close(new_sock);
#ifdef _WIN32
    return 0;
#else
    pthread_exit(NULL);
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 3) { //podanie 2 argumentow: IP oraz ścieżki do pliku który chcemy wysłać
        fprintf(stderr, "Usage: %s <server_ip> <file_path>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];    // ip
    file_path = argv[2];  //  ścieżka do pliku 
    int port = 7777;
    int e; 
    int sockfd, new_sock;
    struct sockaddr_in server_addr, new_addr;
#ifdef _WIN32
    int addr_size; //dla windowsa int bo nie ma socklen_t
#else
    socklen_t addr_size;
    pthread_t tid;//dla linuxa
#endif
#ifdef _WIN32
    WSADATA wsa; //inicjalizacja socketu dla windowsa
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        perror("[!]Failed to initialize Windows socket");
        exit(1);
    }
#endif

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[!]Error in socket");
        exit(1);
    }
    printf("[i]Server socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    //bind
    e = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (e < 0) {
        perror("[!]Error in bind");
        exit(1);
    }
    printf("[i]Binding successful.\n");
    //nasłuchiwanie serwera
    if (listen(sockfd, 10) == 0) {
        printf("[i]Listening....\n");
    } else {
        perror("[!]Error in listening");
        exit(1);
    }
    //obsługa przychodzących połączeń 
    while (1) {
        addr_size = sizeof(new_addr);
        new_sock = accept(sockfd, (struct sockaddr *)&new_addr, &addr_size);

        if (new_sock < 0) {
            perror("[!]Error in accepting");
            exit(1);
        }

#ifdef _WIN32
        // Tworzenie nowego wątku dla obsługi połączenia dla Windows
        HANDLE thread = CreateThread(NULL, 0, handle_connection, (LPVOID)&new_sock, 0, NULL);
        if (thread == NULL) {
            perror("[!]Error in thread creation");
            exit(1);
        }
        CloseHandle(thread);
#else
        // Tworzenie nowego wątku dla obsługi połączenia dla Linux
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, (void*)&new_sock) != 0) {
            perror("[!]Error in thread creation");
            exit(1);
        }
        pthread_detach(tid); 
#endif
    }

    close(sockfd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
