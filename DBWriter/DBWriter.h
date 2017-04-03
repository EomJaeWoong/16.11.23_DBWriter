#ifndef __DBWRITER__H__
#define __DBWRITER__H__

#define DB_HOST "127.0.0.1"
#define DB_USER "root"
#define DB_PASS "6535(Djawodnd!)"
#define DB_NAME "test"

#pragma pack(1)
//--------------------------------------------------------------------------------------------
//  DB 메시지 헤더
//--------------------------------------------------------------------------------------------
struct st_DBQUERY_HEADER
{
	int iType;
	int iSize;
};

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 회원가입
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_NEW_ACCOUNT		0
struct st_DBQUERY_MSG_NEW_ACCOUNT
{
	char	szID[20];
	char	szPassword[20];
};

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 스테이지 클리어
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_STAGE_CLEAR		1
struct st_DBQUERY_MSG_STAGE_CLEAR
{
	__int64	iAccountNo;
	int	iStageID;
};

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_PLAYER_UPDATE		2
struct st_DBQUERY_MSG_PLAYER_UPDATE
{
	__int64	iAccountNo;
	int	iLevel;
	__int64 iExp;
};
#pragma pack(4)

#endif