#include "ChatServer.h"

#include "../Common/ChatProtocol.h"
#include "ChatUser.h"

ChatServer::ChatServer()
{
	NetLib::E_FUNCTION_RESULT result = m_IOCPServer.StartServer();
	if (result == NetLib::FUNCTION_RESULT_SUCCESS)
	{
		m_MaxPacketSize = m_IOCPServer.GetMaxPacketSize();
		m_MaxConnectionCount = m_IOCPServer.GetMaxConnectionCount();
		m_PostMessagesThreadsCount = m_IOCPServer.GetPostMessagesThreadsCount();

		m_IsEchoTestMode = (m_PostMessagesThreadsCount > 1) ? false : true;

		if (Init())
		{
			m_IsSuccessStartServer = true;

			//스레드 생성
			for (int i = 0; i < m_PostMessagesThreadsCount; ++i)
			{
				m_PostMessagesThreads.push_back(std::make_unique<std::thread>(&ChatServer::PostMessagesThreadFunction, this));
			}
		}
	}
}

ChatServer::~ChatServer()
{
	m_IOCPServer.EndServer();

	if (m_IsSuccessStartServer)
	{
		m_IsSuccessStartServer = false;

		for (int i = 0; i < m_PostMessagesThreads.size(); ++i)
		{
			m_PostMessagesThreads[i].get()->join();
		}
	}
}

void ChatServer::Run(void)
{
	if (m_IsSuccessStartServer)
	{
		std::cout << "아무 키를 누르면 종료" << std::endl;
		getchar();
	}
	else
	{
		std::cout << "===서버 동작 실패===" << std::endl;
		std::cout << "아무 키를 누르면 종료" << std::endl;
		getchar();
	}
}

void ChatServer::PostMessagesThreadFunction(void)
{
	if (m_MaxPacketSize <= 0)
	{
		return;
	}

	char* pBuf = new char[m_MaxPacketSize];
	ZeroMemory(pBuf, sizeof(char) * m_MaxPacketSize);

	while (true)
	{
		if (!m_IsSuccessStartServer)
		{
			return;
		}

		ZeroMemory(pBuf, sizeof(char) * m_MaxPacketSize);

		INT8 operationType = 0;
		INT32 connectionIndex = 0;
		INT16 copySize = 0;

		if (!m_IOCPServer.PostMessages(operationType, connectionIndex, pBuf, copySize))
		{
			return;
		}

		switch (operationType)
		{
		case NetLib::OP_CONNECTION:
			ConnectConnection(connectionIndex);
			break;
		case NetLib::OP_CLOSE:
			DisconnectConnection(connectionIndex);
			break;
		case NetLib::OP_RECV_PACKET:
			CommandRecvPacket(connectionIndex, pBuf, copySize);
			break;
		}
	}
}

bool ChatServer::Init(void)
{
	if (m_MaxConnectionCount < 0 || m_MaxRoomCount < 0 || m_PostMessagesThreadsCount < 0)
	{
		return false;
	}

	//패킷 처리 함수 등록
	RegisterProcessPacketFunc();

	//유저 정보 생성
	for (int i = 0; i < m_MaxConnectionCount; ++i)
	{
		m_UsersInfo.push_back(std::make_unique<ChatUser>());
	}

	//방 생성
	for (int i = 0; i < m_MaxRoomCount; ++i)
	{
		std::unordered_set<int/*connectionIndex*/> Users;
		m_Rooms.push_back(Users);
	}

	return true;
}

void ChatServer::ConnectConnection(INT32 connectionIndex)
{
	if (m_IsEchoTestMode)
	{
		return;
	}

	auto iter = m_Users.find(connectionIndex);
	if (iter == m_Users.end())
	{
		(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_CONNECT;
		m_Users.insert(connectionIndex);
	}
}

void ChatServer::DisconnectConnection(INT32 connectionIndex)
{
	if (m_IsEchoTestMode)
	{
		return;
	}

	auto currentState = (m_UsersInfo[connectionIndex].get())->UserState;
	switch (currentState)
	{
	case E_USER_STATE_ROOM:
	{
		auto roomNumber = (m_UsersInfo[connectionIndex].get())->RoomNumber;
		auto iter_RoomNumber = m_Rooms[roomNumber].find(connectionIndex);
		if (iter_RoomNumber != m_Rooms[roomNumber].end())
		{
			iter_RoomNumber = m_Rooms[roomNumber].erase(iter_RoomNumber);
		}
	}
	case E_USER_STATE_LOGIN:
	case E_USER_STATE_CONNECT:
	{
		auto iter_Connection = m_Users.find(connectionIndex);
		if (iter_Connection != m_Users.end())
		{
			iter_Connection = m_Users.erase(iter_Connection);
		}

		auto iter_ID = m_UserIDs.find((m_UsersInfo[connectionIndex].get())->UserID);
		if (iter_ID != m_UserIDs.end())
		{
			iter_ID = m_UserIDs.erase(iter_ID);
		}

		(m_UsersInfo[connectionIndex].get())->Clear();
		break;
	}
	default: return;
	}
}

void ChatServer::CommandRecvPacket(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	PACKET_HEADER* pHeader = reinterpret_cast<PACKET_HEADER*>(pBuf);

	auto iter = m_PacketProcesser.find(pHeader->PacketId);
	if (iter != m_PacketProcesser.end())
	{
		(this->*(iter->second))(connectionIndex, pBuf, copySize);
	}
}

void ChatServer::RegisterProcessPacketFunc(void)
{
	if (m_IsEchoTestMode)
	{
		m_PacketProcesser[ECHO_REQUEST] = &ChatServer::ProcessPacketEcho;
	}
	else
	{
		m_PacketProcesser[LOGIN_REQUEST] = &ChatServer::ProcessPacketLogin;
		m_PacketProcesser[ROOM_NEW_REQUEST] = &ChatServer::ProcessPacketRoomNew;
		m_PacketProcesser[ROOM_ENTER_REQUEST] = &ChatServer::ProcessPacketRoomEnter;
		m_PacketProcesser[ROOM_LEAVE_REQUEST] = &ChatServer::ProcessPacketRoomLeave;
		m_PacketProcesser[ROOM_CHAT_REQUEST] = &ChatServer::ProcessPacketRoomChat;
	}
}

void ChatServer::ProcessPacketEcho(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	ECHO_REQUEST_PACKET* pReqPacket = reinterpret_cast<ECHO_REQUEST_PACKET*>(pBuf);

	ECHO_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = copySize;
	resPacket.PacketId = ECHO_RESPONSE;

	CopyMemory(resPacket.Contents, pReqPacket->Contents, sizeof(resPacket.Contents));

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, copySize);
}

void ChatServer::ProcessPacketLogin(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	if (copySize != sizeof(LOGIN_REQUEST_PACKET))
	{
		return;
	}

	LOGIN_REQUEST_PACKET* pReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pBuf);

	LOGIN_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	resPacket.PacketId = LOGIN_RESPONSE;

	auto currentUserState = (m_UsersInfo[connectionIndex].get())->UserState;
	if (currentUserState == E_USER_STATE_NONE)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_NONE;
	}
	else if (currentUserState == E_USER_STATE_CONNECT)
	{
		std::wstring reqPacketID(pReqPacket->UserID);
		auto iter = m_UserIDs.find(reqPacketID);
		if (iter == m_UserIDs.end())
		{
			resPacket.Result = E_PACKET_RESULT_SUCCESS;

			m_UserIDs.insert(reqPacketID);
			(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_LOGIN;
			(m_UsersInfo[connectionIndex].get())->UserID = reqPacketID;
		}
		else
		{
			resPacket.Result = E_PACKET_RESULT_FAIL_SAME_ID;
		}
	}
	else if (currentUserState == E_USER_STATE_LOGIN)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOBBY;
	}
	else if (currentUserState == E_USER_STATE_ROOM)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_ROOM;
	}

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, resPacket.PacketLength);
}

void ChatServer::ProcessPacketRoomNew(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	if (copySize != sizeof(ROOM_NEW_REQUEST_PACKET))
	{
		return;
	}

	ROOM_NEW_REQUEST_PACKET* pReqPacket = reinterpret_cast<ROOM_NEW_REQUEST_PACKET*>(pBuf);

	ROOM_NEW_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = sizeof(ROOM_NEW_RESPONSE_PACKET);
	resPacket.PacketId = ROOM_NEW_RESPONSE;
	resPacket.RoomNumber = NetLib::INVALID_VALUE;

	auto currentUserState = (m_UsersInfo[connectionIndex].get())->UserState;
	if (currentUserState == E_USER_STATE_NONE)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_NONE;
	}
	else if (currentUserState == E_USER_STATE_CONNECT)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOGIN;
	}
	else if (currentUserState == E_USER_STATE_LOGIN)
	{
		auto roomNumber = NetLib::INVALID_VALUE;
		for (int i = 0; i < m_MaxRoomCount; ++i)
		{
			if (m_Rooms[i].size() == 0)
			{
				roomNumber = i;
				break;
			}
		}

		if (roomNumber == NetLib::INVALID_VALUE)
		{
			resPacket.Result = E_PACKET_RESULT_FAIL_MAX_MAKE_ROOM;
		}
		else
		{
			resPacket.Result = E_PACKET_RESULT_SUCCESS;
			resPacket.RoomNumber = roomNumber;

			m_Rooms[roomNumber].insert(connectionIndex);
			(m_UsersInfo[connectionIndex].get())->RoomNumber = roomNumber;
			(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_ROOM;
		}
	}
	else if (currentUserState == E_USER_STATE_ROOM)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_ROOM;
	}

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, resPacket.PacketLength);
}

void ChatServer::ProcessPacketRoomEnter(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	if (copySize != sizeof(ROOM_ENTER_REQUEST_PACKET))
	{
		return;
	}

	ROOM_ENTER_REQUEST_PACKET* pReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pBuf);

	ROOM_ENTER_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);
	resPacket.PacketId = ROOM_ENTER_RESPONSE;

	auto currentUserState = (m_UsersInfo[connectionIndex].get())->UserState;
	if (currentUserState == E_USER_STATE_NONE)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_NONE;
	}
	else if (currentUserState == E_USER_STATE_CONNECT)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOGIN;
	}
	else if (currentUserState == E_USER_STATE_LOGIN)
	{
		int roomNumber = pReqPacket->RoomNumber;
		if (roomNumber == NetLib::INVALID_VALUE || roomNumber >= m_MaxRoomCount)
		{
			resPacket.Result = E_PACKET_RESULT_FAIL_WRONG_ROOM_NUMBER;
		}
		else
		{
			if (m_Rooms[pReqPacket->RoomNumber].size() > 0)
			{
				resPacket.Result = E_PACKET_RESULT_SUCCESS;

				m_Rooms[pReqPacket->RoomNumber].insert(connectionIndex);
				(m_UsersInfo[connectionIndex].get())->RoomNumber = pReqPacket->RoomNumber;
				(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_ROOM;
			}
			else
			{
				resPacket.Result = E_PACKET_RESULT_FAIL_NOT_EXIST_ROOM;
			}
		}
	}
	else if (currentUserState == E_USER_STATE_ROOM)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_ROOM;
	}

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, resPacket.PacketLength);
}

void ChatServer::ProcessPacketRoomLeave(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	if (copySize != sizeof(ROOM_LEAVE_REQUEST_PACKET))
	{
		return;
	}

	ROOM_LEAVE_REQUEST_PACKET* pReqPacket = reinterpret_cast<ROOM_LEAVE_REQUEST_PACKET*>(pBuf);

	ROOM_LEAVE_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);
	resPacket.PacketId = ROOM_LEAVE_RESPONSE;

	auto currentUserState = (m_UsersInfo[connectionIndex].get())->UserState;
	if (currentUserState == E_USER_STATE_NONE)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_NONE;
	}
	else if (currentUserState == E_USER_STATE_CONNECT)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOGIN;
	}
	else if (currentUserState == E_USER_STATE_LOGIN)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOBBY;
	}
	else if (currentUserState == E_USER_STATE_ROOM)
	{
		auto roomNumber = (m_UsersInfo[connectionIndex].get())->RoomNumber;
		auto iter = m_Rooms[roomNumber].find(connectionIndex);
		if (iter != m_Rooms[roomNumber].end())
		{
			resPacket.Result = E_PACKET_RESULT_SUCCESS;

			iter = m_Rooms[roomNumber].erase(iter);

			(m_UsersInfo[connectionIndex].get())->RoomNumber = NetLib::INVALID_VALUE;
			(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_LOGIN;
		}
		else
		{
			resPacket.Result = E_PACKET_RESULT_FAIL;

			(m_UsersInfo[connectionIndex].get())->RoomNumber = NetLib::INVALID_VALUE;
			(m_UsersInfo[connectionIndex].get())->UserState = E_USER_STATE_LOGIN;
		}
	}

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, resPacket.PacketLength);
}

void ChatServer::ProcessPacketRoomChat(INT32 connectionIndex, char* pBuf, INT16 copySize)
{
	if (copySize != sizeof(ROOM_CHAT_REQUEST_PACKET))
	{
		return;
	}

	ROOM_CHAT_REQUEST_PACKET* pReqPacket = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pBuf);

	ROOM_CHAT_RESPONSE_PACKET resPacket;
	resPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);
	resPacket.PacketId = ROOM_CHAT_RESPONSE;

	auto currentUserState = (m_UsersInfo[connectionIndex].get())->UserState;
	if (currentUserState == E_USER_STATE_NONE)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_NONE;
	}
	else if (currentUserState == E_USER_STATE_CONNECT)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOGIN;
	}
	else if (currentUserState == E_USER_STATE_LOGIN)
	{
		resPacket.Result = E_PACKET_RESULT_FAIL_IN_LOBBY;
	}
	else if (currentUserState == E_USER_STATE_ROOM)
	{
		resPacket.Result = E_PACKET_RESULT_SUCCESS;

		for (auto roomConnectionIndex : m_Rooms[(m_UsersInfo[connectionIndex].get())->RoomNumber])
		{
			ROOM_CHAT_NOTIFY_PACKET notPacket;
			notPacket.PacketLength = sizeof(ROOM_CHAT_NOTIFY_PACKET);
			notPacket.PacketId = ROOM_CHAT_NOTIFY;
			CopyMemory(notPacket.Message, pReqPacket->Message, sizeof(notPacket.Message));
			CopyMemory(notPacket.UserID, (m_UsersInfo[connectionIndex].get())->UserID.c_str(), sizeof(notPacket.UserID));

			m_IOCPServer.SendPacket(roomConnectionIndex, &notPacket, notPacket.PacketLength);
		}
	}

	m_IOCPServer.SendPacket(connectionIndex, &resPacket, resPacket.PacketLength);
}