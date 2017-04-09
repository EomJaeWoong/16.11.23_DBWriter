#include <conio.h>
#include <my_global.h>
#include <mysql.h>
#include <process.h>

#include "StreamQueue.h"
#include "DBWriter.h"

#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "mysql/lib/vs12/mysqlclient.lib")

BOOL b_shutdown;
CAyaStreamSQ StreamQueue(1000);
DWORD g_UpdateTime;

HANDLE hUpdateThread[2];
HANDLE hDBWriterThread;

__int64 iAccountNo;
__int64 iStageNo;

//--------------------------------------------------------------------------------------------
// �޽��� ���� ������
//--------------------------------------------------------------------------------------------
unsigned __stdcall UpdateThread(LPVOID updateArg)
{
	srand(time(NULL) + (int)updateArg);
	int iMessageType;
	
	char *chID;
	char *chPw;
	int iIDlength;
	int iPWlength;
	char chStringArray[35] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
		'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '1', '2', '3', '4', '5', '6', '7',
		'8', '9' };

	st_DBQUERY_HEADER header;

	while (!b_shutdown)
	{
		Sleep(5);

		if (StreamQueue.GetFreeSize() <= sizeof(st_DBQUERY_HEADER))
			continue;

		iMessageType = rand() % 3;
		//iMessageType = df_DBQUERY_MSG_NEW_ACCOUNT;

		switch (iMessageType)
		{
		case df_DBQUERY_MSG_NEW_ACCOUNT:									//ȸ������
			st_DBQUERY_MSG_NEW_ACCOUNT stAccount;

			if (StreamQueue.GetFreeSize() <=
				sizeof(st_DBQUERY_HEADER) + sizeof(st_DBQUERY_MSG_NEW_ACCOUNT))
				continue;

			//ID, PW �����
			iIDlength = (rand() % 15) + 4;
			iPWlength = (rand() % 15) + 4;

			chID = new char[iIDlength+1];
			memset(chID, 0, iIDlength+1);
			chPw = new char[iPWlength+1];
			memset(chPw, 0, iPWlength+1);

			for (int iCnt = 0; iCnt < iIDlength; iCnt++)
				chID[iCnt] = chStringArray[rand() % 35];

			for (int iCnt = 0; iCnt < iPWlength; iCnt++)
				chPw[iCnt] = chStringArray[rand() % 35];

			//����ü�� ID, PW ����
			header.iType = df_DBQUERY_MSG_NEW_ACCOUNT;
			header.iSize = sizeof(st_DBQUERY_MSG_NEW_ACCOUNT);

			strcpy_s(stAccount.szID, sizeof(stAccount.szID), chID);
			strcpy_s(stAccount.szPassword, sizeof(stAccount.szPassword), chPw);

			//���� ����ü �����ֱ�	
			StreamQueue.Lock();
			StreamQueue.Put((char *)&header, sizeof(st_DBQUERY_HEADER));
			StreamQueue.Put((char *)&stAccount, header.iSize);
			StreamQueue.Unlock();
			break;

		case df_DBQUERY_MSG_STAGE_CLEAR:									//�������� Ŭ����
			st_DBQUERY_MSG_STAGE_CLEAR stStageClear;

			if (StreamQueue.GetFreeSize() <=
				sizeof(st_DBQUERY_HEADER) + sizeof(st_DBQUERY_MSG_STAGE_CLEAR))
				continue;

			header.iType = df_DBQUERY_MSG_STAGE_CLEAR;
			header.iSize = sizeof(st_DBQUERY_MSG_STAGE_CLEAR);

			stStageClear.iAccountNo = (__int64)InterlockedIncrement64((LONG64*)&iAccountNo);
			stStageClear.iStageID = (__int64)InterlockedIncrement64((LONG64*)&iStageNo);

			//���� ����ü �����ֱ�
			StreamQueue.Lock();
			StreamQueue.Put((char *)&header, sizeof(st_DBQUERY_HEADER));
			StreamQueue.Put((char *)&stStageClear, header.iSize);
			StreamQueue.Unlock();
			break;

		case df_DBQUERY_MSG_PLAYER_UPDATE:
			st_DBQUERY_MSG_PLAYER_UPDATE stUpdate;

			if (StreamQueue.GetFreeSize() <=
				sizeof(st_DBQUERY_HEADER) + sizeof(st_DBQUERY_MSG_PLAYER_UPDATE))
				continue;

			header.iType = df_DBQUERY_MSG_PLAYER_UPDATE;
			header.iSize = sizeof(st_DBQUERY_MSG_PLAYER_UPDATE);

			stUpdate.iAccountNo = (__int64)InterlockedIncrement64((LONG64*)&iAccountNo);
			stUpdate.iExp = 50;
			stUpdate.iLevel = 1;

			//���� ����ü �����ֱ�
			StreamQueue.Lock();
			StreamQueue.Put((char *)&header, sizeof(st_DBQUERY_HEADER));
			StreamQueue.Put((char *)&stUpdate, header.iSize);
			StreamQueue.Unlock();
			break;

		default:
			printf("Update Error : Not Correct Type...\n");
			return -1;
		}
	}

	return 0;
}

unsigned __stdcall DBWriterThread(LPVOID writerArg)
{
	/*
	// connection �����ʹ� conn�� ����Ŵ(mysql�Լ��� ���ϰ��� MYSQL �����Ͱ�)
	MYSQL *connection = NULL, conn;

	//Query�� ���� ��� ��
	MYSQL_RES sql_result;

	// MYSQL_ROW�� ���ڿ� ������(����������)
	// utf8�̶� �ٲ���� ��(�����츸 utf16)
	MYSQL_ROW sql_row;
	int query_stat;

	mysql_init(&conn);

	connection = mysql_real_connect(&conn, "127.0.0.1", "root", "1234", "test", 3306, (char*)NULL, 0);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error :%s\n", mysql_error(&conn));
		return;
	}

	//mysql_store_result -> ��ü ����� �غ� �� �� �о��
	//mysql_use_result -> �� ������ ���� �Ҵ��� �ؼ� �׾� ����
	*/
	//--------------------------------------------------------------
	// MYSQL ��ü
	//--------------------------------------------------------------
	MYSQL *connection = NULL, conn;

	//--------------------------------------------------------------
	// ������ ������
	//--------------------------------------------------------------
	MYSQL_RES *sql_result;
	MYSQL_ROW sql_row;			

	int query_stat;
	DWORD dwCountThread = 0;

	// �ʱ�ȭ
	mysql_init(&conn);

	// DB����
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char *)NULL, 0);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error :%s\n", mysql_error(&conn));
		return 1;
	}

	// �ѱ� ���
	// ���߿� ������ ����� ã�ƺ���
	mysql_query(connection, "set session character_set_connection=euckr;");
	mysql_query(connection, "set session character_set_result=euckr;");
	mysql_query(connection, "set session character_set_client=euckr;");

	st_DBQUERY_HEADER header;
	char query[200] = { 0, };

	while (1)
	{
		if (g_UpdateTime == 0)
			g_UpdateTime = timeGetTime();


		if (b_shutdown && StreamQueue.GetUseSize() == 0)	return 0;

		StreamQueue.Lock();
		if (StreamQueue.GetUseSize() > 0)
		{
			StreamQueue.Get((char *)&header, sizeof(st_DBQUERY_HEADER));

			switch (header.iType)
			{
			case df_DBQUERY_MSG_NEW_ACCOUNT:
				st_DBQUERY_MSG_NEW_ACCOUNT stAccount;
				StreamQueue.Get((char *)&stAccount, header.iSize);
				sprintf_s(query, 200, "INSERT INTO `%s`.`account` (`id`, `password`) VALUES ('%s', '%s');",
					DB_NAME, stAccount.szID, stAccount.szPassword);
				break;

			case df_DBQUERY_MSG_STAGE_CLEAR:
				st_DBQUERY_MSG_STAGE_CLEAR stStageClear;
				StreamQueue.Get((char *)&stStageClear, header.iSize);
				sprintf_s(query, 200, "INSERT INTO `%s`.`stage` (`account_id`, `stageid`) VALUES (`%d`, `%d`);",
					DB_NAME, stStageClear.iAccountNo, stStageClear.iStageID);
				break;

			case df_DBQUERY_MSG_PLAYER_UPDATE:
				st_DBQUERY_MSG_PLAYER_UPDATE stUpdate;
				StreamQueue.Get((char *)&stUpdate, header.iSize);
				sprintf_s(query, 200, "INSERT INTO `%s`.`player` (`accountno`, `level`, `exp`) VALUES (`%d`, `%d`, `%d`);",
					DB_NAME, stUpdate.iAccountNo, stUpdate.iLevel, stUpdate.iExp);
				break;

			default:
				printf("DBWriter Error : Not Correct Type...\n");
				return 0;
			}

			query_stat = mysql_query(connection, query);
			if (query_stat != 0)
			{
				fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
				return 1;
			}
			memset(query, 0, 200);
			dwCountThread++;
		}
		StreamQueue.Unlock();

		if (timeGetTime() - g_UpdateTime >= 1000)
		{
			printf("------------------------------------\n");
			printf("Writing Count : %d\n", dwCountThread);
			printf("Using Queue size : %d\n", StreamQueue.GetUseSize());
			printf("------------------------------------\n\n");
			g_UpdateTime = timeGetTime();
			dwCountThread = 0;
		}
	}
}

void Initial()
{
	StreamQueue.ClearBuffer();
	b_shutdown = FALSE;
	g_UpdateTime = 0;

	iAccountNo = 0;
	iStageNo = 0;
}

void main()
{
	DWORD dwThreadID;

	char chControlKey;

	Initial();

	hUpdateThread[0] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, (LPVOID)0, 0, (unsigned int *)&dwThreadID);
	hUpdateThread[1] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, (LPVOID)2000, 0, (unsigned int *)&dwThreadID);
	hDBWriterThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, (LPVOID)0, 0, (unsigned int *)&dwThreadID);

	while (1)
	{
		chControlKey = _getch();
		if (chControlKey == 'q' || chControlKey == 'Q')
		{
			//------------------------------------------------
			// ����ó��
			//------------------------------------------------
			b_shutdown = TRUE;
			break;
		}
	}

	DWORD ExitCode;

	/*
	wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

	GetExitCodeThread(hUpdateThread[0], &ExitCode);
	if (ExitCode != 0)
		wprintf(L"error - Update Thread[0] not exit\n");
	CloseHandle(hUpdateThread[0]);

	GetExitCodeThread(hUpdateThread[1], &ExitCode);
	if (ExitCode != 0)
		wprintf(L"error - IUpdate Thread[1] not exit\n");
	CloseHandle(hUpdateThread[1]);

	GetExitCodeThread(hDBWriterThread, &ExitCode);
	if (ExitCode != 0)
		wprintf(L"error - DBWriter Thread not exit\n");
	CloseHandle(hDBWriterThread);
	*/
}