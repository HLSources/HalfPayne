#ifndef GAMEPLAY_MOD_H
#define GAMEPLAY_MOD_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#ifdef CLIENT_DLL
#include "wrect.h"
#include "../cl_dll/cl_dll.h"
#endif // CLIENT_DLL

class GameplayMods
{
public:
	BOOL instaGib = FALSE;

	void Init();

#ifndef CLIENT_DLL
	void SendToClient();
	int Save( CSave &save );
	int Restore( CRestore &restore );
#endif // CLIENT_DLL
};

extern GameplayMods gameplayMods;

#ifndef CLIENT_DLL
extern TYPEDESCRIPTION gameplayModsSaveData[];
#endif // !CLIENT_DLL

#endif // GAMEPLAY_MOD_H