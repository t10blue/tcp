#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")

// ȫ�ֱ���
HWND g_hEdit; // �ı�����
HWND g_hOutput; // �����Ϣ�Ĵ��ھ��
SOCKET g_clientSocket; // �ͻ����׽���
std::thread heartbeatThread; // �����������߳�
std::thread receiveThread; // ������Ϣ���߳�

// ÿ��0.5�뷢��һ��������
void sendHeartbeat() {
    while (true) {
    	
        send(g_clientSocket, "1Heartbeat", 10, 0);
        Sleep(1000);
        
    }
}

// �̺߳��������ڽ��շ������Ϣ
void ReceiveMessages() {
    char buffer[4096];
  
    while (true) { // ʹ�ñ�־�����̵߳�����״̬  
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(g_clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
        	buffer[bytesRead] = '\0';
        	//�ж��ַ�����ͷ�ı�־ 
			if(buffer[0]=='1')continue;
        	else  memmove(buffer, buffer+1, strlen(buffer));
            // ��ӻس����з�
            std::string receivedMessage = buffer;
            receivedMessage += "\r\n";

            // ����Ϣ��ӵ���Ϣ����
            SendMessage(g_hOutput, EM_SETSEL, -1, -1);
            SendMessageA(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)receivedMessage.c_str());
        } else if (bytesRead == 0) {
            // �����ѹرգ��˳�ѭ��
            break;
        } else {
            MessageBoxA(NULL, "Error in receiving message from server.", "Error", MB_OK);
            break;
        }
    }
}

// ������Ϣ������
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) { // Send��ť��IDΪ1001
                char userInput[256];
                GetWindowTextA(g_hEdit, userInput, sizeof(userInput));
                std::string message = userInput;
                    char sentMessages[256];
                    memset(sentMessages, 0, sizeof(sentMessages));
                    sentMessages[0] = '0';
                    strcat(sentMessages, message.c_str());
                    send(g_clientSocket, sentMessages, strlen(sentMessages), 0);
                
                SetWindowTextA(g_hEdit, "");
            } else if (LOWORD(wParam) == 1002) { // Exit��ť��IDΪ1002
            
                SendMessage(hwnd, WM_CLOSE, 0, 0); // ���͹رմ�����Ϣ
                
            }
            break;

        case WM_CLOSE:
        shutdown(g_clientSocket, SD_BOTH);
        // �ر������������߳�
        if (heartbeatThread.joinable()) {
        heartbeatThread.join();
        }
    	// �رս�����Ϣ���߳�
    	if (receiveThread.joinable()) {
       		receiveThread.join();
    	}
    	// �ر��׽��ֺ����� Winsock
    	closesocket(g_clientSocket);
    	WSACleanup();
    // �رմ���
    	DestroyWindow(hwnd);
    	break;


        case WM_DESTROY:
        shutdown(g_clientSocket, SD_BOTH);
        // �ر������������߳�
        if (heartbeatThread.joinable()) {
        heartbeatThread.join();
        }
    	// �رս�����Ϣ���߳�
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
    // ��ʼ�� Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "Failed to initialize winsock.", "Error", MB_OK);
        return 1;
    }

    // �����ͻ����׽���
    g_clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_clientSocket == INVALID_SOCKET) {
        MessageBoxA(NULL, "Failed to create client socket.", "Error", MB_OK);
        WSACleanup();
        return 1;
    }

    // ���÷���˵�ַ�Ͷ˿�
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(40436);
    serverAddress.sin_addr.s_addr = inet_addr("159.89.214.31"); // �滻Ϊ����˵� IP ��ַ

    // ���ӵ������
    if (connect(g_clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        MessageBoxA(NULL, "Failed to connect to server.", "Error", MB_OK);
        closesocket(g_clientSocket);
        WSACleanup();
        return 1;
    }
    //���ó�ʱʱ��
   int timeout = 1500;
    setsockopt(g_clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    // ����������Ϣ���߳�
    std::thread receiveThread(ReceiveMessages);

    // ע�ᴰ����
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MyWindowClass";
    RegisterClass(&wc);

    // ��������
    HWND hwnd = CreateWindowEx(0, "MyWindowClass", "TCP Client", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                               NULL, NULL, hInstance, NULL);

    // �����ı���
    g_hEdit = CreateWindowEx(0, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                             20, 20, 200, 25, hwnd, NULL, hInstance, NULL);

    // ����Send��ť
    CreateWindowEx(0, "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   230, 20, 100, 25, hwnd, (HMENU)1001, hInstance, NULL);

    // ����Exit��ť
    CreateWindowEx(0, "BUTTON", "Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   230, 55, 100, 25, hwnd, (HMENU)1002, hInstance, NULL);

    // ���������Ϣ�Ĵ���
    g_hOutput = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                               20, 90, 310, 160, hwnd, NULL, hInstance, NULL);

    // ���������Ϣ����Ϊֻ��
    SendMessage(g_hOutput, EM_SETREADONLY, TRUE, 0);

    // ��ʾ����
    ShowWindow(hwnd, nCmdShow);
    
     // ���������������߳�
    std::thread heartbeatThread(sendHeartbeat);
    // ��Ϣѭ��
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
     
     //�ر����������� 
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
    // �رս�����Ϣ���߳�
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    return (int)msg.wParam;
}

