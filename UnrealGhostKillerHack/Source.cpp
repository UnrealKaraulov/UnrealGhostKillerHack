#include <Windows.h>
#include <string>
#include <TlHelp32.h>
#include "verinfo.h"
#include <vector>

using namespace std;

int GameDll;
int IsInGame;
int CAgents;


LPVOID TlsValue;
DWORD TlsIndex;
DWORD _W3XTlsIndex;

DWORD GetIndex( )
{
	return *( DWORD* ) ( _W3XTlsIndex );
}

DWORD GetW3TlsForIndex( DWORD index )
{
	DWORD pid = GetCurrentProcessId( );
	THREADENTRY32 te32;
	HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, pid );
	te32.dwSize = sizeof( THREADENTRY32 );

	if ( Thread32First( hSnap, &te32 ) )
	{
		do
		{
			if ( te32.th32OwnerProcessID == pid )
			{
				HANDLE hThread = OpenThread( THREAD_ALL_ACCESS, false, te32.th32ThreadID );
				if ( hThread )
				{
					CONTEXT ctx = { CONTEXT_SEGMENTS };
					LDT_ENTRY ldt;
					GetThreadContext( hThread, &ctx );
					GetThreadSelectorEntry( hThread, ctx.SegFs, &ldt );
					DWORD dwThreadBase = ldt.BaseLow | ( ldt.HighWord.Bytes.BaseMid <<
														 16u ) | ( ldt.HighWord.Bytes.BaseHi << 24u );
					CloseHandle( hThread );
					if ( dwThreadBase == NULL )
						continue;
					DWORD* dwTLS = *( DWORD** ) ( dwThreadBase + 0xE10 + 4 * index );
					if ( dwTLS == NULL )
						continue;
					return ( DWORD ) dwTLS;
				}
			}
		}
		while ( Thread32Next( hSnap, &te32 ) );
	}

	return NULL;
}

void SetTlsForMe( )
{
	TlsIndex = GetIndex( );
	LPVOID tls = ( LPVOID ) GetW3TlsForIndex( TlsIndex );
	TlsValue = tls;
}


struct JassStringData
{
	UINT32 vtable;
	UINT32 refCount;
	UINT32 dwUnk1;
	UINT32 pUnk2;
	UINT32 pUnk3;
	UINT32 pUnk4;
	UINT32 pUnk5;
	void *data;
};


struct JassString
{
	UINT32 vtable;
	UINT32 dw0;
	JassStringData *data;
	UINT32 dw1;
};

typedef void( __cdecl * pStoreInteger )( UINT cache, JassString *missionKey, JassString *key, int value );
pStoreInteger StoreInteger;

typedef void( __cdecl * pSyncStoredInteger )( UINT cache, JassString *missionKey, JassString *key );
pSyncStoredInteger SyncStoredInteger;

typedef int( __cdecl * pGetStoredInteger )( UINT cache, JassString *missionKey, JassString *key );
pGetStoredInteger GetStoredInteger;

int GetJSOffset = 0;

void GetJassString( char *szString, JassString *String )
{
	int Address = GetJSOffset;


	__asm
	{
		PUSH szString;
		MOV ECX, String;
		CALL Address;
	}
}

UINT gamecache;
BOOL cachefound = FALSE;

BOOL IsGameCache( UINT gamecacheid )
{

	JassString * GameStartString = new JassString( );
	JassString * DataString = new JassString( );
	GetJassString( "Data", DataString );
	GetJassString( "GameStart", GameStartString );


	int randomvalue = 5 + ( rand( ) % 100 );
	StoreInteger( gamecacheid, DataString, GameStartString, randomvalue );
	int returnvalue = GetStoredInteger( gamecacheid, DataString, GameStartString );
	if ( returnvalue == randomvalue )
	{
		StoreInteger( gamecacheid, DataString, GameStartString, 1 );
		SyncStoredInteger( gamecacheid, DataString, GameStartString );
		gamecache = gamecacheid;
		cachefound = TRUE;
	}

	delete GameStartString;
	delete DataString;
	return returnvalue == randomvalue;
}

int GetMaxUnitForMap( )
{
	int tmp = 0;
	int tmpaddr = *( int* ) ( CAgents + 0xC );
	if ( tmpaddr > 0 )
	{
		tmp += *( int* ) ( tmpaddr + 0x428 );
		if ( tmp > 0 )
		{
			tmp += 0x100000;
			if ( tmp == 0x100000 )
				return 0;
			return tmp;
		}
	}
	return 0;
}

void SetGameCacheForMe( )
{
	int maxunits = GetMaxUnitForMap( );
	for ( int i = 0x100000; i < maxunits; i++ )
	{
		if ( IsGameCache( i ) )
		{
			return;
		}
		maxunits = GetMaxUnitForMap( );
	}
}



void Init126aVer( )
{
	_W3XTlsIndex = 0xAB7BF4 + GameDll;
	IsInGame = GameDll + 0xAB62A4;
	CAgents = 0xAAE2F0 + GameDll;
	GetJSOffset = GameDll + 0x011300;
	StoreInteger = ( pStoreInteger ) ( GameDll + 0x3CA0A0 );
	SyncStoredInteger = ( pSyncStoredInteger ) ( GameDll + 0x3CA6E0 );
	GetStoredInteger = ( pGetStoredInteger ) ( GameDll + 0x3CA870 );

}


void Init127aVer( )
{
	_W3XTlsIndex = 0xBB8628 + GameDll;
	IsInGame = GameDll + 0xBE6530;
	CAgents = GameDll + 0xBB8060;
	GetJSOffset = GameDll + 0x051310;
	StoreInteger = ( pStoreInteger ) ( GameDll + 0x1F8280 );
	SyncStoredInteger = ( pSyncStoredInteger ) ( GameDll + 0x1F8940 );
	GetStoredInteger = ( pGetStoredInteger ) ( GameDll + 0x1E5710 );

}



void InitializeFogClickWatcher( )
{
	HMODULE hGameDll = GetModuleHandle( "Game.dll" );
	if ( !hGameDll )
	{
		MessageBox( 0, "No game.dll found.", "Game.dll not found", 0 );
		return;
	}

	GameDll = ( int ) hGameDll;

	CFileVersionInfo gdllver;
	gdllver.Open( hGameDll );
	// Game.dll version (1.XX)
	int GameDllVer = gdllver.GetFileVersionQFE( );
	gdllver.Close( );

	if ( GameDllVer == 6401 )
	{
		Init126aVer( );
	}
	else if ( GameDllVer == 52240 )
	{
		Init127aVer( );
	}
	else
	{
		MessageBox( 0, "Game version not supported.", "\nGame version not supported", 0 );
		return;
	}



	// 6401 - 126
	// 52240 - 127

}


void KillHero( int killed )
{

	char buffer1[ 512 ];

	JassString * DataString = new JassString( );
	GetJassString( "Data", DataString );

	JassString * KillHeroString = new JassString( );

	sprintf_s( buffer1, sizeof( buffer1 ), "%s%i", "Hero", killed );
	GetJassString( buffer1, KillHeroString );

	StoreInteger( gamecache, DataString, KillHeroString, killed );
	SyncStoredInteger( gamecache, DataString, KillHeroString );


	delete KillHeroString;
	delete DataString;

}

#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0


void Watcher( )
{
	while ( true )
	{
		Sleep( 100 );

		while ( *( int* ) IsInGame <= 0 )
		{
			Sleep( 100 );
		}
		Sleep( 5000 );

		if ( *( int* ) IsInGame > 0 )
		{

			gamecache = 0;
			cachefound = FALSE;

			while ( *( int* ) IsInGame > 0 && !cachefound )
			{
				SetGameCacheForMe( );
				Sleep( 5000 );
			}

			while ( *( int* ) IsInGame > 0 && cachefound )
			{
				for ( int times = 0; times < 5 && *( int* ) IsInGame; times++ )
				{

					if ( IsKeyPressed( VK_LCONTROL ) )
					{
						Sleep( 1000 );
						continue;
					}

					for ( int i = 1; i <= 5; i++ )
					{
						if ( *( int* ) IsInGame > 0 )
						{
							KillHero( i );
							Sleep( 20 );
						}
					}

					for ( int i = 7; i <= 11; i++ )
					{
						if ( *( int* ) IsInGame > 0 )
						{
							KillHero( i );
							Sleep( 20 );
						}
					}

					Sleep( 32 );
				}
			}
		}
	}
}

void * DeadInitializerid = 0;
BOOL FirstInit = TRUE;

DWORD WINAPI DeadInitializer( LPVOID )
{
	if ( FirstInit )
	{
		MessageBox( 0, "Вы запустили абсолютно \nбесплатный UnrеаlGhоsТКillеrНасk!", "БЕСПЛАТНЫЙ ПОЛНОСТЬЮ!", MB_OK );
	}
	else
	{
		if ( rand( ) % 100 > 50 )
		{
			MessageBox( 0, "Вы запустили абсолютно \nбесплатный UnrеаlGhоsТКillеrНасk!", "БЕСПЛАТНЫЙ ПОЛНОСТЬЮ!", MB_OK );
		}
	}
	FirstInit = FALSE;
	Sleep( 2000 );
	try
	{
		InitializeFogClickWatcher( );
		SetTlsForMe( );
		TlsSetValue( TlsIndex, TlsValue );
		Watcher( );
	}
	catch ( ... )
	{
		DeadInitializerid = CreateThread( 0, 0, DeadInitializer, 0, 0, 0 );
		return 0;

	}

	return 0;
}


BOOL __stdcall DllMain( HINSTANCE i, DWORD r, LPVOID )
{
	if ( r == DLL_PROCESS_ATTACH )
	{
		DeadInitializerid = CreateThread( 0, 0, DeadInitializer, 0, 0, 0 );
	}
	else if ( r == DLL_PROCESS_DETACH )
	{
		TerminateThread( DeadInitializerid, 0 );
	}
	return TRUE;
}