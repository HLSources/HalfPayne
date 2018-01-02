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
	int fieldCounter = 1;
	template<typename T> T Field( T *address, T default, FIELDTYPE type, short count = 1 ) {	
		int offset = ( size_t ) address - ( size_t ) this;

		std::string fieldName = "gameplayModsField" + std::to_string( fieldCounter );
		fieldCounter++;

		this->fields.push_back(
			{ type, _strdup( fieldName.c_str() ), offset, count, 0 }
		);
		return default;
	}

	BOOL instaGib = Field( &instaGib, FALSE, FIELD_BOOLEAN );

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