#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed.\n"; return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Socket creation failed.\n"; return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "\nKoneksi ke Server Gagal! Pastikan Server sudah menyalakan menu 7.\n";
        return 1;
    }

    cout << "=== CLIENT REGISTRASI PANITIA ===\n";
    string namaPeserta;

    cout << "Masukkan Nama Peserta: ";
    getline(cin, namaPeserta);

    json j;
    j["action"] = "register";
    j["nama"] = namaPeserta;

    string payload = j.dump();

    send(sock, payload.c_str(), (int)payload.length(), 0);
    cout << ">> Data pendaftaran atas nama '" << namaPeserta << "' berhasil dikirim!\n";

    closesocket(sock);
    WSACleanup();
    return 0;
}
