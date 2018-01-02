#include "gameplay_mod.h"
GameplayMods gameplayMods;

#ifdef CLIENT_DLL
#include "parsemsg.h"
#else
int gmsgGmplayMod = 0;
#endif

void GameplayMods::Init() {
#ifndef CLIENT_DLL
	if ( !gmsgGmplayMod ) {
		gmsgGmplayMod = REG_USER_MSG( "GmplayMod", -1 );
	}
#else
	gEngfuncs.pfnHookUserMsg( "GmplayMod", []( const char *pszName, int iSize, void *pbuf ) -> int {
		BEGIN_READ( pbuf, iSize );
		gameplayMods.instaGib = READ_BYTE();

		return 1;
	} );
#endif
}

#ifndef CLIENT_DLL

int GameplayMods::Save( CSave &save ) {
	return save.WriteFields( "GAMEPLAY_MODS", this, fields.data(), fields.size() );
}

int GameplayMods::Restore( CRestore &restore ) {
	return restore.ReadFields( "GAMEPLAY_MODS", this, fields.data(), fields.size() );
}

void GameplayMods::SendToClient() {
	if ( !gmsgGmplayMod ) {
		return;
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgGmplayMod );
		WRITE_BYTE( instaGib );
	MESSAGE_END();
}

#endif