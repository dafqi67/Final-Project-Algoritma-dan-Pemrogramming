#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

mutex mtx;
SOCKET globalServerSocket = INVALID_SOCKET;

class AttendeeNode {
public:
    int attendeeID;
    string name;
    AttendeeNode* next;
    AttendeeNode(int id, string n) : attendeeID(id), name(n), next(nullptr) {}
};

class AttendeeList {
private:
    AttendeeNode* head;
public:
    AttendeeList() { head = nullptr; }

    void addAttendee(int id, string name) {
        AttendeeNode* newNode = new AttendeeNode(id, name);
        if (head == nullptr) {
            head = newNode;
        } else {
            AttendeeNode* temp = head;
            while (temp->next != nullptr) temp = temp->next;
            temp->next = newNode;
        }
    }

    void displayAttendees() {
        AttendeeNode* temp = head;
        if (temp == nullptr) {
            cout << ">> Belum ada peserta terdaftar untuk acara ini.\n";
            return;
        }

        cout << "+-----+------------+--------------------------------+\n";
        cout << "| No. | ID Peserta | Nama Peserta                   |\n";
        cout << "+-----+------------+--------------------------------+\n";

        int no = 1;
        while (temp != nullptr) {
            cout << "| " << left << setfill(' ') << setw(3) << no++ << " | ";

            cout << right << setfill('0') << setw(3) << temp->attendeeID;
            cout << left << setfill(' ') << setw(7) << "" << " | ";

            string namaPrint = temp->name;
            if (namaPrint.length() > 30) {
                namaPrint = namaPrint.substr(0, 27) + "...";
            }
            cout << left << setw(30) << namaPrint << " |\n";

            temp = temp->next;
        }
        cout << "+-----+------------+--------------------------------+\n";
    }

    void searchAttendee(int targetID) {
        AttendeeNode* temp = head;
        bool found = false;
        while (temp != nullptr) {
            if (temp->attendeeID == targetID) {
                cout << ">> [DITEMUKAN] Peserta ID " << setfill('0') << setw(3) << targetID << " : " << temp->name << "\n";
                found = true; break;
            }
            temp = temp->next;
        }
        if (!found) cout << ">> [TIDAK DITEMUKAN] ID " << targetID << " belum terdaftar.\n";
    }

    void sortAttendees() {
        if (head == nullptr || head->next == nullptr) return;
        bool swapped;
        AttendeeNode* ptr1;
        AttendeeNode* lptr = nullptr;
        do {
            swapped = false;
            ptr1 = head;
            while (ptr1->next != lptr) {
                if (ptr1->attendeeID > ptr1->next->attendeeID) {
                    swap(ptr1->attendeeID, ptr1->next->attendeeID);
                    swap(ptr1->name, ptr1->next->name);
                    swapped = true;
                }
                ptr1 = ptr1->next;
            }
            lptr = ptr1;
        } while (swapped);
        cout << ">> Data peserta berhasil diurutkan berdasarkan ID!\n";
    }
};

class Event {
protected:
    int eventID;
    string eventName;
    string eventDate;
    int nextAttendeeID;

public:
    AttendeeList attendees;

    Event(int id, string name, string date) : eventID(id), eventName(name), eventDate(date), nextAttendeeID(1) {}
    virtual ~Event() {}
    virtual void displayEventDetails() = 0;

    string getEventName() { return eventName; }
    virtual string getCategory() = 0;

    void registerAttendee(string name) {
        int assignedID = nextAttendeeID++;
        attendees.addAttendee(assignedID, name);
        cout << ">> [SISTEM] Peserta '" << name << "' sukses terdaftar dengan ID: " << setfill('0') << setw(3) << assignedID << "\n";
    }

    void showAttendees() { attendees.displayAttendees(); }
};

class SportsEvent : public Event {
private:
    string sportType;
public:
    SportsEvent(int id, string name, string date, string sport) : Event(id, name, date), sportType(sport) {}

    string getCategory() override { return "Cabang Olahraga: " + sportType; }

    void displayEventDetails() override {
        cout << "\n=================================================\n";
        cout << "[Sports Event] " << eventName << "\nCabang\t\t: " << sportType << "\nTanggal Dimulai\t: " << eventDate << "\n";
        cout << "=================================================\n";
    }
};

class ArtsEvent : public Event {
private:
    string artTheme;
public:
    ArtsEvent(int id, string name, string date, string theme) : Event(id, name, date), artTheme(theme) {}

    string getCategory() override { return "Tema Seni: " + artTheme; }

    void displayEventDetails() override {
        cout << "\n=================================================\n";
        cout << "[Arts Event] " << eventName << "\nTema\t\t: " << artTheme << "\nTanggal Dimulai\t: " << eventDate << "\n";
        cout << "=================================================\n";
    }
};

void handleClient(SOCKET clientSocket, Event* targetEvent) {
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string receivedData(buffer);

    try {
        json j = json::parse(receivedData);
        if (j["action"] == "register" && targetEvent != nullptr) {
            string nama = j["nama"];

            mtx.lock();
            cout << "\n[JARINGAN MASUK] Ada pendaftaran baru lewat jaringan!\n";
            targetEvent->registerAttendee(nama);
            cout << "Ketik angka menu untuk lanjut: \n"; 
            mtx.unlock();
        }
    } catch (json::parse_error& e) {
        cout << "[SERVER] Format JSON tidak valid.\n";
    }
    closesocket(clientSocket);
}

Event* pilihAcaraDinamis(vector<Event*>& daftarAcara) {
    if (daftarAcara.empty()) {
        cout << ">> Error: Belum ada cabang acara yang dibuat!\n";
        return nullptr;
    }
    cout << "\n--- PILIH CABANG ACARA ---\n";
    for (size_t i = 0; i < daftarAcara.size(); i++) {
        cout << i + 1 << ". " << daftarAcara[i]->getEventName() << " (" << daftarAcara[i]->getCategory() << ")\n";
    }
    cout << "Pilihanmu (Angka): ";
    int idx;
    cin >> idx;
    if (idx < 1 || idx > daftarAcara.size()) {
        cout << ">> Error: Pilihan tidak valid!\n";
        return nullptr;
    }
    return daftarAcara[idx - 1];
}

void jalankanServerJaringan(SOCKET serverSocket, Event* targetEvent) {
    while(true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            break; 
        }
        thread t(handleClient, clientSocket, targetEvent);
        t.detach();
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed.\n"; return 1;
    }

    vector<Event*> daftarAcara;
    int pilihan;

    while (true) {
        cout << "\n=== SMART EVENT ORGANIZER ===\n";
        cout << "1. Buat Cabang Teknik Cup\n";
        cout << "2. Buat Cabang Seni\n";
        cout << "3. Registrasi Peserta Manual\n";
        cout << "4. Tampilkan Laporan Acara\n";
        cout << "5. Cari Peserta (Berdasarkan ID)\n";
        cout << "6. Urutkan Peserta\n";
        cout << "7. Nyalakan Server Jaringan\n";
        cout << "8. Keluar\nMenu: ";
        cin >> pilihan;

        if (pilihan == 1) {
            string cabor, tanggal;
            cin.ignore();
            cout << "Nama Cabang Olahraga: "; getline(cin, cabor);
            cout << "Tanggal Dimulai (DD-MM-YYYY): "; getline(cin, tanggal);

            daftarAcara.push_back(new SportsEvent(daftarAcara.size() + 1, "Teknik Cup", tanggal, cabor));
            cout << ">> Cabang '" << cabor << "' untuk Teknik Cup berhasil dibuat!\n";

        } else if (pilihan == 2) {
            string tema, tanggal;
            cin.ignore();
            cout << "Nama Cabang/Tema Seni: "; getline(cin, tema);
            cout << "Tanggal Dimulai (DD-MM-YYYY): "; getline(cin, tanggal);

            daftarAcara.push_back(new ArtsEvent(daftarAcara.size() + 1, "Seni", tanggal, tema));
            cout << ">> Cabang Seni bertema '" << tema << "' berhasil dibuat!\n";

        } else if (pilihan == 3) {
            Event* targetEvent = pilihAcaraDinamis(daftarAcara);
            if (!targetEvent) continue;

            string nama;
            cin.ignore();
            cout << "Nama Peserta: "; getline(cin, nama);
            targetEvent->registerAttendee(nama);

        } else if (pilihan == 4) {
            Event* targetEvent = pilihAcaraDinamis(daftarAcara);
            if (!targetEvent) continue;

            targetEvent->displayEventDetails();
            targetEvent->showAttendees();

        } else if (pilihan == 5) {
            Event* targetEvent = pilihAcaraDinamis(daftarAcara);
            if (!targetEvent) continue;

            int id;
            cout << "Cari ID Peserta (ketik angkanya saja, misal 1 atau 5): ";
            cin >> id;
            targetEvent->attendees.searchAttendee(id);

        } else if (pilihan == 6) {
            Event* targetEvent = pilihAcaraDinamis(daftarAcara);
            if (!targetEvent) continue;

            targetEvent->attendees.sortAttendees();

        } else if (pilihan == 7) {
            Event* targetEvent = pilihAcaraDinamis(daftarAcara);
            if (!targetEvent) continue;

            if (globalServerSocket != INVALID_SOCKET) {
                cout << ">> Menutup gerbang online acara sebelumnya...\n";
                closesocket(globalServerSocket);
                globalServerSocket = INVALID_SOCKET;
                this_thread::sleep_for(chrono::milliseconds(200));
            }

            globalServerSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (globalServerSocket == INVALID_SOCKET) {
                cout << ">> Gagal membuat socket server.\n";
                continue;
            }

            struct sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(8080);

            char opt = 1;
            setsockopt(globalServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            if (bind(globalServerSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                cout << ">> Gagal melakukan bind ke Port 8080. Mungkin port masih tersangkut.\n";
                closesocket(globalServerSocket);
                globalServerSocket = INVALID_SOCKET;
                continue;
            }

            listen(globalServerSocket, 3);
            cout << "\n=======================================================\n";
            cout << ">> GERBANG ONLINE PINDAH & DIBUKA di Port 8080!\n";
            cout << ">> Sekarang mendengarkan untuk: " << targetEvent->getCategory() << "\n";
            cout << ">> (Data cabang lain AMAN & terminal tetap AKTIF)\n";
            cout << "=======================================================\n";

            thread serverThread(jalankanServerJaringan, globalServerSocket, targetEvent);
            serverThread.detach();

        } else if (pilihan == 8) {
            cout << ">> Sistem ditutup. Terima kasih!\n";
            break;
        } else {
            cout << ">> Pilihan tidak valid!\n";
        }
    }

    for (Event* e : daftarAcara) {
        delete e;
    }
    daftarAcara.clear();

    WSACleanup();
    return 0;
}
