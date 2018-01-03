#ifndef GAMEPLAY_MOD_H
#define GAMEPLAY_MOD_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#ifdef CLIENT_DLL
#include "wrect.h"
#include "../cl_dll/cl_dll.h"
#endif // CLIENT_DLL

#include <vector>

class GameplayMods
{
public:
	std::vector<TYPEDESCRIPTION> fields;
	template<typename T> T _Field( int offset, T default, FIELDTYPE type, short count = 1 ) {
		std::string fieldName = "gameplayModsField" + std::to_string( fields.size() + 1 );

		this->fields.push_back(
			{ type, _strdup( fieldName.c_str() ), offset, count, 0 }
		);
		return default;
	}
#define Field( ctype, name, default, type, ... ) ctype name = _Field( offsetof( GameplayMods, name ), default, type, ##__VA_ARGS__ )

	Field( BOOL, automaticShotgun, FALSE, FIELD_BOOLEAN );
	Field( BOOL, instaGib, FALSE, FIELD_BOOLEAN );
	Field( BOOL, noSecondaryAttack, FALSE, FIELD_BOOLEAN );
	Field( BOOL, shouldProducePhysicalBullets, FALSE, FIELD_BOOLEAN );
	Field( BOOL, snarkNuclear, FALSE, FIELD_BOOLEAN );

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