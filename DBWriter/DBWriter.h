#ifndef __DBWRITER__H__
#define __DBWRITER__H__

#define DB_HOST "127.0.0.1"
#define DB_USER "root"
#define DB_PASS "6535(Djawodnd!)"
#define DB_NAME "test"

#pragma pack(1)
//--------------------------------------------------------------------------------------------
//  DB �޽��� ���
//--------------------------------------------------------------------------------------------
struct st_DBQUERY_HEADER
{
	int iType;
	int iSize;
};

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - ȸ������
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_NEW_ACCOUNT		0
struct st_DBQUERY_MSG_NEW_ACCOUNT
{
	char	szID[20];
	char	szPassword[20];
};

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - �������� Ŭ����
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_STAGE_CLEAR		1
struct st_DBQUERY_MSG_STAGE_CLEAR
{
	__int64	iAccountNo;
	int	iStageID;
};

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - 
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