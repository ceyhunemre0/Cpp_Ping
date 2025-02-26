#include <QCoreApplication>
using namespace std;
#include <iostream>
#define WIN32_LEAN_AND_MEAN // kütüphanedeki gereksiz paketlerin yüklenmemesini sağlar.
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <chrono>

// ICMP Başlığı Yapısı
struct ICMPHeader { // hepsi unsigned yapıdadır yani negatif olamazlar. windows.h kütüphanesinden geliyorlar.
    BYTE Type;
    BYTE Code;
    USHORT Checksum;
    USHORT ID;
    USHORT Sequence;
};

// ICMP Checksum Hesaplama
USHORT ComputeChecksum(USHORT* buffer, int size) { // buffer pointer olarak tanımlanmıştır.
    unsigned long checksum = 0;
    while (size > 1) {
        checksum += *buffer++; // önce buffer'ın işaret ettiği değer alınır sonra bir sonraki bellek adresine kaydırılır.
        size -= sizeof(USHORT);
    }
    if (size) {
        checksum += *(UCHAR*)buffer;
    }
    checksum = (checksum >> 16) + (checksum & 0xFFFF);
    checksum += (checksum >> 16);
    return (USHORT)(~checksum);
}

// Ping fonksiyonu hem IP hem de domain alabilir.
void ping(const std::string& target) {
    std::string ip = target;

    if (inet_addr(target.c_str()) == INADDR_NONE) { // Eğer geçerli bir IP değilse domain çözümle
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup başarısız!" << std::endl;
            return;
        }

        struct addrinfo hints = { 0,AF_INET,SOCK_RAW,IPPROTO_ICMP,0,nullptr,nullptr,nullptr }, *res;

        if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0) {
            std::cerr << "Domain çözülemedi: " << target << std::endl;
            WSACleanup();
            return;
        }

        char ipStr[INET_ADDRSTRLEN];
        sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
        ip = ipStr;

        freeaddrinfo(res);
        WSACleanup();
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup başarısız!" << std::endl;
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Raw socket oluşturulamadı! Hata kodu: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    ICMPHeader icmp;
    icmp.Type = 8;  // Echo Request
    icmp.Code = 0;
    icmp.Checksum = 0;
    icmp.ID = (USHORT)GetCurrentProcessId();
    icmp.Sequence = 1;
    icmp.Checksum = ComputeChecksum((USHORT*)&icmp, sizeof(icmp));

    auto start = std::chrono::high_resolution_clock::now();
    if (sendto(sock, (char*)&icmp, sizeof(icmp), 0, (sockaddr*)&destAddr, sizeof(destAddr)) == SOCKET_ERROR) {
        std::cerr << "Ping paketi gönderilemedi! Hata kodu: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    char recvBuf[64];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    if (recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&fromAddr, &fromLen) == SOCKET_ERROR) {  // (sockaddr*)&fromAddr ile sockaddr_in tipinden sockaddr tipindeki pointer haline dönüştürülüyor.
        std::cerr << "Yanıt alınamadı! Hata kodu: " << WSAGetLastError() << std::endl;
    } else {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << target << " (" << ip << ") adresine ping basarili! Gecikme: " << elapsed.count() << " ms" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
}

int main() {
    ping("1.1.1.1");        // IP'ye ping at
    ping("google.com");     // Domain'e ping at
    ping("8.8.8.8");        // Başka bir IP'ye ping at
    ping("openai.com");     // Başka bir domain'e ping at
    return 0;
}
