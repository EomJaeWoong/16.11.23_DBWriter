//#include <windows.h>
#include <process.h>
#include <conio.h>
#include <my_global.h>
#include <mysql.h>

#pragma comment (lib, "mysql/lib/vs12/mysqlclient.lib")

#include "StreamQueue.h"
#include "DBWriter.h"

BOOL b_shutdown;
CAyaStreamSQ StreamQueue;

HANDLE hUpdateThread[2];
HANDLE hDBWriterThread;

unsigned __stdcall UpdateThread(LPVOID updateArg)
{
	srand(time(NULL) + (int)updateArg);
	__int64 iAccountNo = 0;
	int iMessageType;
	int iStage = 1;
	char *chID;
	char *chPw;
	int iIDlength;
	int iPWlength;

	st_DBQUERY_HEADER header;

	while (!b_shutdown)
	{
		Sleep(200);

		//iMessageType = rand() % 3;
		iMessageType = df_DBQUERY_MSG_NEW_ACCOUNT;

		StreamQueue.Lock();
		switch (iMessageType)
		{
		case df_DBQUERY_MSG_NEW_ACCOUNT:	
			st_DBQUERY_MSG_NEW_ACCOUNT stAccount;

			//ID, PW 만들기
			iIDlength = (rand() % 6) + 4;
			iPWlength = (rand() % 6) + 4;

			chID = new char[iIDlength+1];
			memset(chID, 0, iIDlength+1);
			chPw = new char[iPWlength+1];
			memset(chPw, 0, iPWlength+1);

			for (int iCnt = 0; iCnt < iIDlength; iCnt++)
				chID[iCnt] = (rand() % 92) + 33;

			for (int iCnt = 0; iCnt < iPWlength; iCnt++)
				chPw[iCnt] = (rand() % 92) + 33;

			//구조체에 ID, PW 삽입
			header.iType = df_DBQUERY_MSG_NEW_ACCOUNT;
			header.iSize = sizeof(st_DBQUERY_MSG_NEW_ACCOUNT);

			strcpy_s(stAccount.szID, sizeof(chID), chID);
			strcpy_s(stAccount.szPassword, sizeof(chPw), chPw);

			//만든 구조체 보내주기	
			StreamQueue.Put((char *)&header, sizeof(st_DBQUERY_HEADER));
			StreamQueue.Put((char *)&stAccount, header.iSize);

			printf("MSG_NEW_ACCOUNT isDone UPDATE.\n");
			break;

		case df_DBQUERY_MSG_STAGE_CLEAR:
			st_DBQUERY_MSG_STAGE_CLEAR stStageClear;

			header.iType = df_DBQUERY_MSG_STAGE_CLEAR;
			header.iSize = sizeof(st_DBQUERY_MSG_STAGE_CLEAR);

			stStageClear.iAccountNo = (__int64)InterlockedIncrement((long*)&iAccountNo);
			stStageClear.iStageID = (int)InterlockedIncrement((long*)&iStage);

			//만든 구조체 보내주기
			StreamQueue.Put((char *)&header, sizeof(st_DBQUERY_HEADER));
			StreamQueue.Put((char *)&stStageClear, header.iSize);

			printf("MSG_STAGE_CLEAR isDone UPDATE.\n");
			break;

		case df_DBQUERY_MSG_PLAYER_UPDATE:
			st_DBQUERY_MSG_PLAYER_UPDATE stUpdate;
			memset(&stUpdate, 0, sizeof(st_DBQUERY_MSG_PLAYER_UPDATE));

			break;

		default:
			printf("Update Error : Not Correct Type...\n");
			return -1;
		}
		StreamQueue.Unlock();
	}

	return 0;
}

unsigned __stdcall DBWriterThread(LPVOID writerArg)
{
	//--------------------------------------------------------------
	// MYSQL 객체
	//--------------------------------------------------------------
	MYSQL *connection = NULL, conn;

	//--------------------------------------------------------------
	// 실질적 데이터
	//--------------------------------------------------------------
	MYSQL_RES *sql_result;
	MYSQL_ROW sql_row;			

	int query_stat;

	// 초기화
	mysql_init(&conn);

	// DB연결
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char *)NULL, 0);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error :%s\n", mysql_error(&conn));
		return 1;
	}

	// 한글 사용
	// 나중에 문제가 생기면 찾아보셈
	mysql_query(connection, "set session character_set_connection=euckr;");
	mysql_query(connection, "set session character_set_result=euckr;");
	mysql_query(connection, "set session character_set_client=euckr;");

	st_DBQUERY_HEADER header;
	char query[200];

	while (1)
	{
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
				printf("MSG_NEW_ACCOUNT isDone.\n");
				break;

			case df_DBQUERY_MSG_STAGE_CLEAR:
				st_DBQUERY_MSG_STAGE_CLEAR stStageClear;
				StreamQueue.Get((char *)&stStageClear, header.iSize);
				sprintf_s(query, 200, "INSERT INTO `%s`.`stage` (`account_id`, `stageid`) VALUES ('%s', '%s');",
					DB_NAME, stStageClear.iAccountNo, stStageClear.iStageID);
				printf("MSG_STAGE_CLEAR isDone.\n");
				break;

			case df_DBQUERY_MSG_PLAYER_UPDATE:

				break;

			default:
				printf("DBWriter Error : Not Correct Type...\n");
				return 0;
			}
		}
		StreamQueue.Unlock();
	}
}

void Initial()
{
	StreamQueue.ClearBuffer();
	b_shutdown = FALSE;
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
			// 종료처리
			//------------------------------------------------
			b_shutdown = TRUE;
			break;
		}
	}

	DWORD ExitCode;

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
}