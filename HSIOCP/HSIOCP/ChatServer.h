#pragma once

#include "../NetLib/NetLibHeader.h"

struct ChatUser;
class ChatServer
{
public:
	ChatServer();
	~ChatServer();

private:
	typedef void(ChatServer::*PROCESS_RECV_PACKET_FUNCTION)(INT32, char*, INT16);
	std::map<short/*packetID*/, PROCESS_RECV_PACKET_FUNCTION> m_PacketProcesser;

public:
	void Run(void);

private:
	void PostMessagesThreadFunction(void);

	bool Init(void);

	void ConnectConnection(INT32 connectionIndex);
	void DisconnectConnection(INT32 connectionIndex);
	void CommandRecvPacket(INT32 connectionIndex, char* pBuf, INT16 copySize);

	void RegisterProcessPacketFunc(void);
	void ProcessPacketEcho(INT32 connectionIndex, char* pBuf, INT16 copySize);
	void ProcessPacketLogin(INT32 connectionIndex, char* pBuf, INT16 copySize);
	void ProcessPacketRoomNew(INT32 connectionIndex, char* pBuf, INT16 copySize);
	void ProcessPacketRoomEnter(INT32 connectionIndex, char* pBuf, INT16 copySize);
	void ProcessPacketRoomLeave(INT32 connectionIndex, char* pBuf, INT16 copySize);
	void ProcessPacketRoomChat(INT32 connectionIndex, char* pBuf, INT16 copySize);

private:
	NetLib::IOCPServer m_IOCPServer;

	bool m_IsSuccessStartServer = false;

	bool m_IsEchoTestMode = false;

	int m_PostMessagesThreadsCount = NetLib::INVALID_VALUE;
	std::vector<std::unique_ptr<std::thread>> m_PostMessagesThreads;

	int m_MaxPacketSize = NetLib::INVALID_VALUE;
	int m_MaxConnectionCount = NetLib::INVALID_VALUE;
	std::unordered_set<int/*connectionIndex*/> m_Users;
	std::unordered_set<std::wstring> m_UserIDs;
	std::vector<std::unique_ptr<ChatUser>> m_UsersInfo;

	int m_MaxRoomCount = 100;
	std::vector<std::unordered_set<int/*connectionIndex*/>> m_Rooms;
};