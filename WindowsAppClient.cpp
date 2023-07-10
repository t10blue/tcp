#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")

// 全局变量
HWND g_hEdit; // 文本框句柄
HWND g_hOutput; // 输出消息的窗口句柄
SOCKET g_clientSocket; // 客户端套接字
std::thread heartbeatThread; // 心跳包发送线程
std::thread receiveThread; // 接收消息的线程

// 每隔0.5秒发送一次心跳包
void sendHeartbeat() {
    while (true) {
    	
        send(g_clientSocket, "1Heartbeat", 10, 0);
        Sleep(1000);
        
    }
}

// 线程函数：用于接收服务端消息
void ReceiveMessages() {
    char buffer[4096];
  
    while (true) { // 使用标志控制线程的运行状态  
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(g_clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
        	buffer[bytesRead] = '\0';
        	//判断字符串开头的标志 
			if(buffer[0]=='1')continue;
        	else  memmove(buffer, buffer+1, strlen(buffer));
            // 添加回车换行符
            std::string receivedMessage = buffer;
            receivedMessage += "\r\n";

            // 将消息添加到消息窗口
            SendMessage(g_hOutput, EM_SETSEL, -1, -1);
            SendMessageA(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)receivedMessage.c_str());
        } else if (bytesRead == 0) {
            // 连接已关闭，退出循环
            break;
        } else {
            MessageBoxA(NULL, "Error in receiving message from server.", "Error", MB_OK);
            break;
        }
    }
}

// 窗口消息处理函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) { // Send按钮的ID为1001
                char userInput[256];
                GetWindowTextA(g_hEdit, userInput, sizeof(userInput));
                std::string message = userInput;
                    char sentMessages[256];
                    memset(sentMessages, 0, sizeof(sentMessages));
                    sentMessages[0] = '0';
                    strcat(sentMessages, message.c_str());
                    send(g_clientSocket, sentMessages, strlen(sentMessages), 0);
                
                SetWindowTextA(g_hEdit, "");
            } else if (LOWORD(wParam) == 1002) { // Exit按钮的ID为1002
            
                SendMessage(hwnd, WM_CLOSE, 0, 0); // 发送关闭窗口消息
                
            }
            break;

        case WM_CLOSE:
        shutdown(g_clientSocket, SD_BOTH);
        // 关闭心跳包发送线程
        if (heartbeatThread.joinable()) {
        heartbeatThread.join();
        }
    	// 关闭接收消息的线程
    	if (receiveThread.joinable()) {
       		receiveThread.join();
    	}
    	// 关闭套接字和清理 Winsock
    	closesocket(g_clientSocket);
    	WSACleanup();
    // 关闭窗口
    	DestroyWindow(hwnd);
    	break;


        case WM_DESTROY:
        shutdown(g_clientSocket, SD_BOTH);
        // 关闭心跳包发送线程
        if (heartbeatThread.joinable()) {
        heartbeatThread.join();
        }
    	// 关闭接收消息的线程
    	if (receiveThread.joinable()) {
       		receiveThread.join();
    	}
    		closesocket(g_clientSocket);
    		WSACleanup();
    		PostQuitMessage(0);
            break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "Failed to initialize winsock.", "Error", MB_OK);
        return 1;
    }

    // 创建客户端套接字
    g_clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_clientSocket == INVALID_SOCKET) {
        MessageBoxA(NULL, "Failed to create client socket.", "Error", MB_OK);
        WSACleanup();
        return 1;
    }

    // 设置服务端地址和端口
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(40436);
    serverAddress.sin_addr.s_addr = inet_addr("159.89.214.31"); // 替换为服务端的 IP 地址

    // 连接到服务端
    if (connect(g_clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        MessageBoxA(NULL, "Failed to connect to server.", "Error", MB_OK);
        closesocket(g_clientSocket);
        WSACleanup();
        return 1;
    }
    //设置超时时间
   int timeout = 1500;
    setsockopt(g_clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    // 创建接收消息的线程
    std::thread receiveThread(ReceiveMessages);

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MyWindowClass";
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowEx(0, "MyWindowClass", "TCP Client", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                               NULL, NULL, hInstance, NULL);

    // 创建文本框
    g_hEdit = CreateWindowEx(0, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                             20, 20, 200, 25, hwnd, NULL, hInstance, NULL);

    // 创建Send按钮
    CreateWindowEx(0, "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   230, 20, 100, 25, hwnd, (HMENU)1001, hInstance, NULL);

    // 创建Exit按钮
    CreateWindowEx(0, "BUTTON", "Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   230, 55, 100, 25, hwnd, (HMENU)1002, hInstance, NULL);

    // 创建输出消息的窗口
    g_hOutput = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                               20, 90, 310, 160, hwnd, NULL, hInstance, NULL);

    // 设置输出消息窗口为只读
    SendMessage(g_hOutput, EM_SETREADONLY, TRUE, 0);

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    
     // 创建心跳包发送线程
    std::thread heartbeatThread(sendHeartbeat);
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
     
     //关闭心跳包发送 
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
    // 关闭接收消息的线程
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    return (int)msg.wParam;
}

