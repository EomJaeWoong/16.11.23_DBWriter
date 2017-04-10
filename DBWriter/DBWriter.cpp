#include <conio.h>
#include <my_global.h>
#include <mysql.h>
#include <process.h>

#include "MemoryPool.h"
#include "StreamQueue.h"
#include "DBWriter.h"

#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "mysql/lib/vs12/mysqlclient.lib")

BOOL b_DBShutdown;
BOOL b_shutdown;

CMemoryPool<st_DBQUERY> DBQueryPool(200000);
CAyaStreamSQ StreamQueue(1000);

DWORD g_UpdateTime;
DWORD dwCountThread;

HANDLE hUpdateThread[2];
HANDLE hDBWriterThread;
HANDLE hMonitorThread;

__int64 iAccountNo;
__int64 iStageNo;



//--------------------------------------------------------------------------------------------
// 메시지 생성 쓰레드
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

	while (!b_shutdown)
	{
		Sleep(5);

		if (StreamQueue.GetFreeSize() <= sizeof(st_DBQUERY*))
			continue;

		iMessageType = rand() % 3;

		DBQueryPool.Lock();
		//Message Type 넣기
		st_DBQUERY* pDBQuery = DBQueryPool.Alloc();
		DBQueryPool.Unlock();

		pDBQuery->type = iMessageType;

		switch (iMessageType)
		{
		case df_DBQUERY_MSG_NEW_ACCOUNT:									//회원가입
			/////////////////////////////////////////////////////////////////////////////
			// ID, PW 만들기
			/////////////////////////////////////////////////////////////////////////////
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

			// Message 삽입
			strcpy_s(((st_DBQUERY_MSG_NEW_ACCOUNT*)(pDBQuery->message))->szID, iIDlength + 1, chID);
			strcpy_s(((st_DBQUERY_MSG_NEW_ACCOUNT*)(pDBQuery->message))->szPassword, iPWlength + 1, chPw);

			break;

		case df_DBQUERY_MSG_STAGE_CLEAR:									//스테이지 클리어
			// Message 삽입
			((st_DBQUERY_MSG_STAGE_CLEAR*)(pDBQuery->message))->iAccountNo = 
				(__int64)InterlockedIncrement64((LONG64*)&iAccountNo);
			((st_DBQUERY_MSG_STAGE_CLEAR*)(pDBQuery->message))->iStageID =
				(__int64)InterlockedIncrement64((LONG64*)&iStageNo);
			break;

		case df_DBQUERY_MSG_PLAYER_UPDATE:
			// Message 삽입
			((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iAccountNo =
				(__int64)InterlockedIncrement64((LONG64*)&iAccountNo);
			((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iLevel = 1;
			((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iExp= 50;
			break;

		default:
			printf("Update Error : Not Correct Type...\n");
			return -1;
		}

		StreamQueue.Lock();
		StreamQueue.Put((char*)&pDBQuery, sizeof(pDBQuery));
		StreamQueue.Unlock();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------
// DB 저장 쓰레드
//--------------------------------------------------------------------------------------------
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

	//--------------------------------------------------------------
	// 초기화
	//--------------------------------------------------------------
	mysql_init(&conn);

	//--------------------------------------------------------------
	// DB연결
	//--------------------------------------------------------------
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

	char query[200] = { 0, };

	st_DBQUERY* pDBQuery = NULL;

	while (!b_DBShutdown)
	{
		if (g_UpdateTime == 0)
			g_UpdateTime = timeGetTime();

		if (StreamQueue.GetUseSize() > 0)
		{
			StreamQueue.Lock();
			StreamQueue.Get((char*)&pDBQuery, sizeof(pDBQuery));

			switch (pDBQuery->type)
			{
			case df_DBQUERY_MSG_NEW_ACCOUNT:
				sprintf_s(query, 200, "INSERT INTO `%s`.`account` (`id`, `password`) VALUES ('%s', '%s');",
					DB_NAME, 
					((st_DBQUERY_MSG_NEW_ACCOUNT*)(pDBQuery->message))->szID, 
					((st_DBQUERY_MSG_NEW_ACCOUNT*)(pDBQuery->message))->szPassword);
				break;

			case df_DBQUERY_MSG_STAGE_CLEAR:
				sprintf_s(query, 200, "INSERT INTO `%s`.`stage` (`accountno`, `stageid`) VALUES (%d, %d);",
					DB_NAME,
					((st_DBQUERY_MSG_STAGE_CLEAR*)(pDBQuery->message))->iAccountNo,
					((st_DBQUERY_MSG_STAGE_CLEAR*)(pDBQuery->message))->iStageID);
				break;

			case df_DBQUERY_MSG_PLAYER_UPDATE:
				sprintf_s(query, 200, "INSERT INTO `%s`.`player` (`accountno`, `level`, `exp`) VALUES (%d, %d, %d);",
					DB_NAME, 
					((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iAccountNo,
					((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iLevel,
					((st_DBQUERY_MSG_PLAYER_UPDATE*)(pDBQuery->message))->iExp);
				break;

			default:
				printf("DBWriter Error : Not Correct Type...\n");
				return 0;
			}
			StreamQueue.Unlock();

			query_stat = mysql_query(connection, query);
			if (query_stat != 0)
			{
				fprintf(stderr, "Mysql query error : %s\n\n", mysql_error(&conn));
				return 1;
			}
			InterlockedIncrement64((LONG64*)&dwCountThread);

			DBQueryPool.Lock();
			DBQueryPool.Free(pDBQuery);
			DBQueryPool.Unlock();

			memset(query, 0, 200);
		}
	}

	mysql_close(connection);
	return 0;
}

unsigned __stdcall MonitorThread(LPVOID writerArg)
{
	while (!b_DBShutdown)
	{
		if (timeGetTime() - g_UpdateTime >= 1000)
		{
			printf("------------------------------------\n");
			printf("Writing Count : %d\n", dwCountThread);
			printf("Using Queue size : %d\n", StreamQueue.GetUseSize());
			printf("Alloc Count : %d\n", DBQueryPool.GetAllocCount());
			printf("------------------------------------\n\n");
			g_UpdateTime = timeGetTime();
			dwCountThread = 0;
		}
	}

	return 0;
}

void Initial()
{
	StreamQueue.ClearBuffer();
	b_shutdown = FALSE;
	b_DBShutdown = FALSE;

	g_UpdateTime = 0;
	dwCountThread = 0;

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
	hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (LPVOID)0, 0, (unsigned int *)&dwThreadID);

	while (!b_shutdown)
	{
		chControlKey = _getch();
		if (chControlKey == 'q' || chControlKey == 'Q')
		{
			//------------------------------------------------
			// 종료처리
			//------------------------------------------------
			b_shutdown = TRUE;
		}
	}

	//-------------------------------------------------------------------------------
	// 쓰레드 종료 과정
	//-------------------------------------------------------------------------------
	wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

	// UpdateThread 종료
	DWORD ExitCode;
	if (WaitForMultipleObjects(2, hUpdateThread, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		wprintf(L"Thread Exit Error : %d\n", GetLastError());
		return;
	}

	GetExitCodeThread(hUpdateThread[0], &ExitCode);
	if (ExitCode == STILL_ACTIVE)
		wprintf(L"error - Update Thread[0] not exit\n");
	else
		wprintf(L"[%08x] Update Thread[0] is done : %d\n", hUpdateThread[0], ExitCode);
	CloseHandle(hUpdateThread[0]);

	GetExitCodeThread(hUpdateThread[1], &ExitCode);
	if (ExitCode == STILL_ACTIVE)
		wprintf(L"error - Update Thread[1] not exit\n");
	else
		wprintf(L"[%08x] Update Thread[1] is done : %d\n", hUpdateThread[1], ExitCode);
	CloseHandle(hUpdateThread[1]);

	// 나머지 쓰레드 종료
	b_DBShutdown = TRUE;

	HANDLE hThread[2] = { hMonitorThread, hDBWriterThread };
	if (WaitForMultipleObjects(2, hThread, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		wprintf(L"Thread Exit Error : %d\n", GetLastError());
		return;
	}
	
	GetExitCodeThread(hMonitorThread, &ExitCode);
	if(ExitCode == STILL_ACTIVE)
		wprintf(L"error - Monitor Thread not exit\n");
	else
		wprintf(L"[%08x] hMonitorThread is done : %d\n", hMonitorThread, ExitCode);
	CloseHandle(hMonitorThread);

	GetExitCodeThread(hDBWriterThread, &ExitCode);
	if (ExitCode == STILL_ACTIVE)
		wprintf(L"error - DBWriter Thread not exit\n");
	else
		wprintf(L"[%08x] hDBWriterThread is done : %d\n", hDBWriterThread, ExitCode);
	CloseHandle(hDBWriterThread);

	wprintf(L"Queue using size : %d\n", StreamQueue.GetUseSize());

	wprintf(L"isdfjosdfi");
}