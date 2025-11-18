#include "Net.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996) 


int sock;
struct sockaddr_in addr;
WSAData wsaData;
struct timeval tv;


void Script_Net::BeginPlay() {
    // BeginPlay
    WSACleanup();
    WSAStartup(MAKEWORD(2, 0), &wsaData);
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(50008);// 待ち受けポート番号を50008にする
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");// 送信アドレスを設定
}

void Script_Net::Update() {
    // Update
    char data[] = "test";

    //データ送信
    sendto(sock, data, sizeof(data), 0, (struct sockaddr*)&addr, sizeof(addr));//addrに文字列送信
}

void Script_Net::EndPlay() {
    // EndPlay
    closesocket(sock);
    WSACleanup();
}
