#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h> //do polecenia basename
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

#define SIZE 4096 //rozmiar bufora

// Funkcja sprawdzająca, czy plik o danej nazwie już istnieje

int file_exists(const char* filename) {
    return access(filename, F_OK) != -1;
}

void receive_file(int e) {
    char buffer[SIZE]; //bufor
    int n;
    uint64_t progress=0;

    // Odbierz nazwę pliku
    uint32_t filename_size;
    n = recv(e, &filename_size, sizeof(uint32_t), 0);
    if (n <= 0) {
        perror("[!]Error receiving file name size.");
        exit(1);
    }

    char filename[filename_size + 1];
    n = recv(e, filename, filename_size, 0);
    filename[n] = '\0';  // dołożenie znaku końca stringa
    printf("[i]Receiving file: %s\n", filename);

    char* basename_filename = basename(filename);//wydobycie ze ścieżki nazwy pliku

    //sprawdzenie czy jest juz plik o takiej nazwie
    //jeśli jest dopisz do nazwy pliku prefix np. 1_{nazwa_pliku}
    if (file_exists(basename_filename)) {
        fprintf(stderr, "[!]Error: File %s already exists.\n", basename_filename);
        int numer = 1;
        char mod_filename[SIZE];
        do {
            snprintf(mod_filename, sizeof(mod_filename), "%d_%s", numer, basename_filename);
            numer++;
        } while (file_exists(mod_filename));

        // Użyj zmodyfikowanej nazwy pliku w dalszej części programu
        strcpy(basename_filename, mod_filename);
        fprintf(stderr, "[i]Filename renamed to %s.\n", basename_filename);
    }
    //otworzenie pliku do zapisu danych
    FILE* fp;
    fp = fopen(basename_filename, "wb");
    if (fp == NULL) {
        perror("[!]Error opening file for writing.");
        exit(1);
    }

    // Odbierz rozmiar pliku
    uint64_t file_size;
    n = recv(e, &file_size, sizeof(uint64_t), 0);
    printf("Incoming file size: %ld bytes.\n", file_size);
    uint64_t full_size = file_size;
    if (n <= 0) {
        perror("[!]Error receiving file size.");
        exit(1);
    }

    // Odbieranie i zapisywanie pliku blokami
    while (file_size > 0) {
        n = recv(e, buffer, SIZE, 0);
        if (n <= 0) {
            break;
        }
        fwrite(buffer, 1, n, fp);
        file_size -= n;
        progress += n;
        // Aktualizacja progresu w terminalu
        long percent = progress*100 / full_size;
        printf("\rDownloading file:  %ld / %ld bytes. Progress: %ld %%", (long)progress , (long)full_size, percent);
        fflush(stdout);

    }
    printf("\n");
    fclose(fp);
}

int main(int argc, char* argv[]) {
    if (argc != 2) { // podanie jako argument ip serwera
        fprintf(stderr, "Usage: %s <server_ip> \n", argv[0]);
        exit(1);
    }

    char* ip = argv[1];// ip serwera
    int port = 7777;
    int e;

    int sockfd;
    struct sockaddr_in server_addr;

#ifdef _WIN32 // inicjalizacja socketu dla windowsa
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        perror("[!]Failed to initialize Winsock");
        exit(1);
    }
#endif

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[!]Error in socket");
        exit(1);
    }
    printf("[i]Client socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    //połącz z serwerem
    e = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (e == -1) {
        perror("[!]Error in socket");
        exit(1);
    }
    printf("[i]Connected to Server.\n");

    receive_file(sockfd);

    printf("[DONE]File received successfully.\n");
    printf("[i]Closing the connection.\n");
    close(sockfd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}