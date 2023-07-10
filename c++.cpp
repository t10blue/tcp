#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <vector> 
#include <Windows.h>
#include <mutex>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

std::vector<SOCKET> connectedClients;
std::mutex clientsMutex; // 互斥锁

// 每隔0.5秒发送一次心跳包
void sendHeartbeat() {
    while (true) {
        clientsMutex.lock();
        for (SOCKET clientSocket : connectedClients) {
            send(clientSocket, "1Heartbeat", 10, 0);
        }
        clientsMutex.unlock();
        Sleep(1000);
    }
}

// 线程函数：用于处理客户端连接
void HandleClient(SOCKET clientSocket) {
    char buffer[4096];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
        	if(buffer[0] == '0'){
        		memmove(buffer, buffer+1, strlen(buffer));
        		clientsMutex.lock();
				std::cout << "Received message from client "<<std::find(connectedClients.begin(), connectedClients.end(), clientSocket) - connectedClients.begin() << " : " << buffer << std::endl;
				clientsMutex.unlock();
			}else continue;
            
        } else {
        clientsMutex.lock();
        if (bytesRead == 0) {
            std::cout << "Client " << std::find(connectedClients.begin(), connectedClients.end(), clientSocket) - connectedClients.begin() << " disconnected." << std::endl;
            break;
            
        } else {
        	
            std::cout << "Error in receiving message from client "<< std::find(connectedClients.begin(), connectedClients.end(), clientSocket) - connectedClients.begin() <<". Error code: " << WSAGetLastError() << std::endl;
            break;
            
        }
        clientsMutex.unlock();
    }
    
    }
    std::cout<<"zhixin"; 
    clientsMutex.lock();
    connectedClients.erase(std::remove(connectedClients.begin(), connectedClients.end(), clientSocket), connectedClients.end());
    clientsMutex.unlock();
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
    std::cout<<"zhixin2";
}

int main() {
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize winsock." << std::endl;
        return 1;
    }

    // 创建服务端套接字
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create server socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // 设置绑定地址和端口
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8888);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到地址和端口
    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind server socket." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 开始监听连接
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on server socket." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    //设置超时时间
    int timeout = 1500;
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    std::cout << "Server started. Waiting for connections..." << std::endl;

    // 创建管理员输入处理线程
    std::thread adminThread([](SOCKET serverSocket) {
        std::string userInput;
        while (true) {
            std::getline(std::cin, userInput);
            if (userInput == "exit") {
                break;
            }
            std::string index = "0";
            userInput = index += userInput;
            // 遍历所有已连接的客户端，发送管理员输入消息
            clientsMutex.lock();
            for (SOCKET clientSocket : connectedClients) {
                send(clientSocket, userInput.c_str(), userInput.size(), 0);
            }
            clientsMutex.unlock();
        }

        clientsMutex.lock();
        // 关闭所有客户端连接
        for (SOCKET clientSocket : connectedClients) {
            shutdown(clientSocket, SD_BOTH);
            closesocket(clientSocket);
        }
        clientsMutex.unlock();

        // 关闭服务端套接字
        shutdown(serverSocket, SD_BOTH);
        closesocket(serverSocket);

        WSACleanup();
    }, serverSocket);

    // 创建心跳包发送线程
    std::thread heartbeatThread(sendHeartbeat);

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept client connection." << std::endl;
            break;
        }
        
        clientsMutex.lock();
        // 将客户端套接字保存到已连接客户端列表中
        connectedClients.push_back(clientSocket); 
		std::cout<<"one client "<< std::find(connectedClients.begin(), connectedClients.end(), clientSocket) - connectedClients.begin() <<" connected"<<std::endl;
        clientsMutex.unlock();
       
        // 创建处理客户端连接的线程
        std::thread clientThread(HandleClient, clientSocket);
        clientThread.detach();
    }

    // 等待心跳包发送线程结束
    heartbeatThread.join();

    // 等待管理员输入处理线程结束
    adminThread.join();

    return 0;
}

