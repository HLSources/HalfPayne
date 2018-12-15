#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"client.h"
#include	"weapons.h"
#include	"cgm_gamerules.h"
#include	"custom_gamemode_config.h"
#include	<algorithm>
#include	<random>
#include	<regex>
#include	"monsters.h"
#include	"gameplay_mod.h"
#include	"../fmt/printf.h"
#include	<thread>
#include	<mutex>
#include	"../twitch/twitch.h"
#include	"kerotan.h"

extern Twitch *twitch;
std::thread twitch_thread;

// Custom Game Mode Rules

int	gmsgEndActiv = 0;
int	gmsgEndTitle = 0;
int	gmsgEndTime  = 0;
int	gmsgEndScore = 0;
int	gmsgEndStat  = 0;

int	gmsgGLogDeact	= 0;
int	gmsgGLogMsg		= 0;
int	gmsgGLogWDeact	= 0;
int	gmsgGLogWMsg	= 0;
int gmsgTimerPause  = 0;
int gmsgTimerDeact = 0;
int gmsgTimerValue = 0;
int gmsgTimerCheat = 0;

int gmsgCountDeact = 0;
int gmsgCountLen   = 0;
int gmsgCountValue = 0;
int gmsgCountOffse = 0;
int gmsgCountCheat = 0;

int gmsgScoreDeact  = 0;
int gmsgScoreValue	= 0;
int gmsgScoreCheat	= 0;

int gmsgRandModLen = 0;
int gmsgRandModVal = 0;

int gmsgPropModVin = 0;

int gmsgCLabelVal  = 0;
int gmsgCLabelGMod  = 0;

int gmsgSayText2 = 0;

bool ShouldInitializeTwitch() {
	return
		CVAR_GET_FLOAT( "twitch_integration_random_gameplay_mods_voting" ) > 0.0f ||
		CVAR_GET_FLOAT( "twitch_integration_mirror_chat" ) > 0.0f ||
		CVAR_GET_FLOAT( "twitch_integration_say" ) > 0.0f ||
		CVAR_GET_FLOAT( "twitch_integration_random_kill_messages" ) > 0.0f;
}

// CGameRules were recreated each level change and there were no built-in saving method,
// that means we'd lose config file state on each level change.
//
// This has been changed and now CGameRules state is preseved during CHANGE_LEVEL calls,
// using g_changeLevelOccured like below and see also CWorld::Precache 

extern int g_changeLevelOccured;
extern Intermission g_latestIntermission;

#define COMBO_MULTIPLIER_DECAY_TIME 8.0f;

CCustomGameModeRules::CCustomGameModeRules( CONFIG_TYPE configType ) : config( configType )
{
	configs.push_back( &config );

	if ( !gmsgEndActiv ) {
		gmsgEndActiv = REG_USER_MSG( "EndActiv", 1 );
		gmsgEndTitle = REG_USER_MSG( "EndTitle", -1 );
		gmsgEndTime  = REG_USER_MSG( "EndTime", -1 );
		gmsgEndScore = REG_USER_MSG( "EndScore", -1 );
		gmsgEndStat  = REG_USER_MSG( "EndStat", -1 );


		gmsgGLogDeact = REG_USER_MSG( "GLogDeact", 0 );
		gmsgGLogMsg = REG_USER_MSG( "GLogMsg", -1 );
		gmsgGLogWDeact = REG_USER_MSG( "GLogWDeact", 0 );
		gmsgGLogWMsg = REG_USER_MSG( "GLogWMsg", -1 );
		gmsgTimerPause = REG_USER_MSG( "TimerPause", 1 );

		gmsgTimerDeact = REG_USER_MSG( "TimerDeact", 0 );
		gmsgTimerValue = REG_USER_MSG( "TimerValue", -1 );
		gmsgTimerCheat = REG_USER_MSG( "TimerCheat", 0 );

		gmsgCountDeact = REG_USER_MSG( "CountDeact", 0 );
		gmsgCountLen = REG_USER_MSG( "CountLen", 2 );
		gmsgCountValue = REG_USER_MSG( "CountValue", -1 );
		gmsgCountOffse = REG_USER_MSG( "CountOffse", 4 );
		gmsgCountCheat = REG_USER_MSG( "CountCheat", 0 );

		gmsgScoreDeact = REG_USER_MSG( "ScoreDeact", 0 );
		gmsgScoreValue = REG_USER_MSG( "ScoreValue", 16 );
		gmsgScoreCheat = REG_USER_MSG( "ScoreCheat", 0 );

		gmsgCLabelVal = REG_USER_MSG( "CLabelVal", -1 );
		gmsgCLabelGMod = REG_USER_MSG( "CLabelGMod", -1 );
		
		gmsgPropModVin = REG_USER_MSG( "PropModVin", -1 );

		gmsgSayText2 = REG_USER_MSG( "SayText2", -1 );
	}

	// Difficulty must be initialized separately and here, becuase entities are not yet spawned,
	// and they take some of the difficulty info at spawn (like CWallHealth)

	// Monster entities also have to be fetched at this moment for ClientPrecache.
	const char *configName = CVAR_GET_STRING( "gamemode_config" );
	config.ReadFile( configName );
	RefreshSkillData();

	endMarkersActive = false;

	cheatedMessageSent = false;
	startMapDoesntMatch = false;

	musicSwitchDelay = 0.0f;

	maxYOffset = -1;

	for ( const auto &spawner : config.entityRandomSpawners ) {
		if ( !spawner.spawnOnce ) {
			entityRandomSpawnerControllers.push_back( EntityRandomSpawnerController( spawner ) );
		}
	}

	for ( size_t i = 0; i < min( config.endConditions.size(), ( size_t ) 64 ); i++ ) {
		auto &endCondition = config.endConditions.at( i );
		if ( FStrEq( gameplayModsData.endConditionsHashes[i], endCondition.GetHash().c_str() ) ) {
			endCondition.activations = gameplayModsData.endConditionsActivationCounts[i];
		}
	}
}

void CCustomGameModeRules::RestartGame() {
	char mapName[256];
	sprintf( mapName, "%s", config.startMap.c_str() );

	// Change level to [startmap] and queue the 'restart' command
	// for CCustomGameModeRules::PlayerSpawn.
	// Would be better just to execute 'cgm' command directly somehow.
	config.markedForRestart = true;
	CHANGE_LEVEL( mapName, NULL );
}

std::mutex gameLogMutex;
void CCustomGameModeRules::SendGameLogMessage( CBasePlayer *pPlayer, const std::string &message, bool logToConsole ) {
	std::lock_guard<std::mutex> guard( gameLogMutex );

	if ( maxYOffset == -1 ) {
		SendHUDMessages( pPlayer );
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgGLogMsg, NULL, pPlayer->pev );
		WRITE_STRING( message.c_str() );
		WRITE_LONG( maxYOffset );
	MESSAGE_END();

	if ( logToConsole ) {
		g_engfuncs.pfnServerPrint( ( message + "\n" ).c_str() );
	}
}

void CCustomGameModeRules::SendGameLogWorldMessage( CBasePlayer *pPlayer, const Vector &location, const std::string &message, const std::string &message2, float flashTime ) {
	MESSAGE_BEGIN( MSG_ONE, gmsgGLogWMsg, NULL, pPlayer->pev );
		WRITE_COORD( location.x );
		WRITE_COORD( location.y );
		WRITE_COORD( location.z );
		WRITE_FLOAT( flashTime );

		// You cant send TWO strings - they overwrite each other, so split them with |
		WRITE_STRING( ( message + "|" + message2 ).c_str() );
	MESSAGE_END();
}

void CCustomGameModeRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	if ( config.markedForRestart ) {
		config.markedForRestart = false;
		SERVER_COMMAND( "restart\n" );
		return;
	}

	CHalfLifeRules::PlayerSpawn( pPlayer );

	gameplayModsData.activeGameMode = GAME_MODE_CUSTOM;
	gameplayModsData.gungameSeed = aux::rand::uniformInt( 0, 10000 );
	
	if ( auto timeRestriction = gameplayMods::timeRestriction.isActive<float>() ) {
		gameplayModsData.time = *timeRestriction;
	}
	

	// '\' slashes are getting eaten by ALLOC_STRING? must prevent this by replacing them with '/'
	std::string sanitizedConfigName = config.configName;
	std::transform( sanitizedConfigName.begin(), sanitizedConfigName.end(), sanitizedConfigName.begin(), []( auto &letter ) {
		return letter == '\\' ? '/' : letter;
	} );
	sprintf_s( gameplayModsData.activeGameModeConfig, sanitizedConfigName.c_str() );

	if ( auto initialSlowmotionCharge = gameplayMods::slowmotionInitialCharge.isActive<int>() ) {
		pPlayer->slowMotionCharge = *initialSlowmotionCharge;
	}	
	
	if ( auto initialHealth = gameplayMods::initialHealth.isActive<int>() ) {
		pPlayer->pev->health = *initialHealth;
	}	

	if ( auto randomGameplayMods = gameplayMods::randomGameplayMods.isActive<RandomGameplayModsInfo>() ) {
		gameplayModsData.timeLeftUntilNextRandomGameplayMod = randomGameplayMods->timeUntilNextRandomGameplayMod;
	}

	bool spawningAfterIntermission = false;
	if ( g_latestIntermission.startPos.defined ) {
		ApplyStartPositionToEntity( pPlayer, g_latestIntermission.startPos );

		if ( g_latestIntermission.startPos.stripped ) {
			pPlayer->RemoveAllItems( false );
		}

		spawningAfterIntermission = true;
		g_latestIntermission.startPos.defined = false;
	}

	sprintf_s( gameplayModsData.activeGameModeConfigHash, config.sha1.c_str() );
	
	// Do not let player cheat by not starting at the [startmap]
	if ( !spawningAfterIntermission ) {
		const char *actualMap = STRING( gpGlobals->mapname );
		startMapDoesntMatch = config.startMap != actualMap;
	}
}

BOOL CCustomGameModeRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !gameplayMods::weaponRestricted.isActive() ) {
		return CHalfLifeRules::CanHavePlayerItem( pPlayer, pWeapon );
	}

	if ( !pPlayer->HasWeapons() || !pPlayer->loadoutReceived ) {
		return CHalfLifeRules::CanHavePlayerItem( pPlayer, pWeapon );
	}

	return FALSE;
}

void CCustomGameModeRules::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeRules::PlayerThink( pPlayer );

	// This is terribly wrong, it would be better to reset lastGlobalTime on actual change level event
	// It was made to prevent timer messup during level changes, because each level has it's own local time
	float proposedTimeDelta = gpGlobals->time - gameplayModsData.lastGlobalTime;
	float timeDelta = fabs( proposedTimeDelta ) <= 0.1 ? ( proposedTimeDelta ) : 0.0f;
	float realTimeDelta = timeDelta / pPlayer->desiredTimeScale;

	gameplayModsData.lastGlobalTime = gpGlobals->time;

	if ( !gameplayModsData.timerPaused && pPlayer->pev->deadflag == DEAD_NO ) {
	
		if ( gameplayMods::timeRestriction.isActive() ) {
			gameplayModsData.time -= timeDelta;
		} else {
			gameplayModsData.time += timeDelta;
		}

		gameplayModsData.realTime += realTimeDelta;

		if ( gameplayMods::IsSlowmotionEnabled() ) {
			gameplayModsData.secondsInSlowmotion += timeDelta;
		}
	}

	CheckForCheats( pPlayer );

	for ( auto &spawner : entityRandomSpawnerControllers ) {
		spawner.Think( pPlayer );
	}

	size_t musicPlaylistSize = config.musicPlaylist.size();
	if (
		pPlayer->pev->deadflag == DEAD_NO &&
		musicPlaylistSize > 0 &&
		CVAR_GET_FLOAT( "sm_current_pos" ) == 0.0f &&
		gpGlobals->time > musicSwitchDelay
	) {
		size_t musicIndexToPlay = pPlayer->currentMusicPlaylistIndex + 1;
		if ( config.musicPlaylistShuffle ) {
			musicIndexToPlay = aux::rand::uniformInt<size_t>( 0, musicPlaylistSize - 1 );
		}

		if ( musicIndexToPlay >= musicPlaylistSize ) {
			musicIndexToPlay = 0;
		}
		
		pPlayer->SendPlayMusicMessage( config.musicPlaylist.at( musicIndexToPlay ).path );
		pPlayer->currentMusicPlaylistIndex = musicIndexToPlay;

		musicSwitchDelay = gpGlobals->time + 0.2f;
	}

	SendHUDMessages( pPlayer );

	if ( gameplayMods::scoreAttack.isActive() ) {
		gameplayModsData.comboMultiplierReset -= realTimeDelta;
		if ( gameplayModsData.comboMultiplierReset < 0.0f ) {
			gameplayModsData.comboMultiplierReset = 0.0f;
			gameplayModsData.comboMultiplier = 1;
		}
	}

	if ( gameplayMods::timeRestriction.isActive() && gameplayModsData.time <= 0.0f && pPlayer->pev->deadflag == DEAD_NO ) {
		ClientKill( ENT( pPlayer->pev ) );
	}

	if ( auto randomGameplayMods = gameplayMods::randomGameplayMods.isActive<RandomGameplayModsInfo>() ) {
		using namespace gameplayMods;

		for ( const auto &timedMod : timedGameplayMods ) {
			std::string modString = timedMod.mod->id + "|";
			for ( auto &arg : timedMod.args ) {
				modString += arg.string + " ";
			}
			aux::str::rtrim( &modString );
			modString += std::to_string( timedMod.time );
		}

		if ( pPlayer->pev->deadflag == DEAD_NO ) {
			gameplayModsData.timeLeftUntilNextRandomGameplayMod -= realTimeDelta;
		}

		if ( proposedGameplayMods.size() > 0 && gameplayModsData.timeLeftUntilNextRandomGameplayMod < 0.0f ) {
			gameplayModsData.timeLeftUntilNextRandomGameplayMod = randomGameplayMods->timeUntilNextRandomGameplayMod;

			bool hasOneVote = false;
			for ( auto &proposedMod : proposedGameplayMods ) {
				if ( !proposedMod.votes.empty() ) {
					hasOneVote = true;
					break;
				}
			}

			ProposedGameplayMod *randomGameplayMod;
			if ( hasOneVote ) {
				size_t totalVotes = 0;
				size_t maxVoteCount = 0;
				for ( auto &mod : proposedGameplayMods ) {
					totalVotes += mod.votes.size();
					maxVoteCount = max( maxVoteCount, mod.votes.size() );
				}

				auto most_votes_more_likely = std::string( CVAR_GET_STRING( "twitch_integration_random_gameplay_mods_voting_result" ) ) == "most_votes_more_likely";

				auto voteDistributions = aux::ctr::map<std::vector<double>>( proposedGameplayMods, [most_votes_more_likely, maxVoteCount]( const ProposedGameplayMod &proposedMod ) {
					if ( most_votes_more_likely ) {
						return ( double ) proposedMod.voteDistributionPercent;
					} else {
						return proposedMod.votes.size() == maxVoteCount ? 1.0 : 0.0;
					}
				} );

				randomGameplayMod = &proposedGameplayMods.at( aux::rand::discreteIndex( voteDistributions ) );
			} else {
				randomGameplayMod = aux::rand::choice( proposedGameplayMods.begin(), proposedGameplayMods.end() )._Ptr;
			}
			
			if ( randomGameplayMod->mod->isEvent ) {
				auto eventResults = randomGameplayMod->mod->EventInit();
				if ( randomGameplayMods->timeForRandomGameplayMod >= 10.0f ) {
					MESSAGE_BEGIN( MSG_ONE, gmsgCLabelVal, NULL, pPlayer->pev );
						WRITE_STRING( eventResults.first.c_str() );
						WRITE_STRING( eventResults.second.c_str() );
					MESSAGE_END();
				}
			} else {
				
				timedGameplayMods.push_back( {
					randomGameplayMod->mod,
					randomGameplayMod->args,
					randomGameplayMods->timeForRandomGameplayMod,
					randomGameplayMods->timeForRandomGameplayMod
				} );

				if ( randomGameplayMods->timeForRandomGameplayMod >= 10.0f ) {
					MESSAGE_BEGIN( MSG_ONE, gmsgCLabelGMod, NULL, pPlayer->pev );
						WRITE_STRING( randomGameplayMod->mod->id.c_str() );
					MESSAGE_END();
				}
			}
		
			proposedGameplayMods.clear();

		}
		
		if ( proposedGameplayMods.empty() && gameplayModsData.timeLeftUntilNextRandomGameplayMod <= randomGameplayMods->timeForRandomGameplayModVoting ) {
			
			auto filteredMods = aux::ctr::filter( allowedForRandom, [this]( GameplayMod *mod ) {

				if ( !config.randomModsWhitelist.empty() && !aux::ctr::includes( config.randomModsWhitelist, mod->id ) ) {
					return false;
				}

				if ( aux::ctr::includes( config.randomModsBlacklist, mod->id ) ) {
					return false;
				}

				if ( mod->isActive( true ) ) {
					return false;
				}

				return mod->CanBeActivatedRandomly();
			} );

			for ( int i = 0; i < 3; i++ ) {
				if ( filteredMods.empty() ) {
					break;
				}

				auto randomMod = aux::rand::choice( filteredMods );
				filteredMods.erase( randomMod );
				proposedGameplayMods.push_back( { randomMod, randomMod->GetRandomArguments() } );
			}

			if ( twitch && twitch->status == TWITCH_CONNECTED && CVAR_GET_FLOAT( "twitch_integration_random_gameplay_mods_voting" ) ) {
				twitch->SendChatMessage( "VOTE FOR NEXT MOD" );
				for ( size_t i = 0; i < proposedGameplayMods.size(); i++ ) {
					auto &proposedMod = proposedGameplayMods.at( i );
					twitch->SendChatMessage( fmt::sprintf( "%d: %s", i + 1, proposedMod.mod->name ) );
				}
			}
		}

		if ( pPlayer->pev->deadflag == DEAD_NO ) {
			for ( auto i = timedGameplayMods.begin(); i != timedGameplayMods.end(); ) {
				i->time -= realTimeDelta;

				if ( i->time <= 0 ) {
					i = timedGameplayMods.erase( i );
				} else {
					i++;
				}

			}
		}
	}

	auto gungameInfo = gameplayMods::gungame.isActive<GunGameInfo>();

	static bool lastGunGame = gungameInfo.has_value();
	gameplayMods::OnFlagChange<bool>( lastGunGame, gungameInfo.has_value(), [this, pPlayer, gungameInfo]( bool on ) {
		if ( on ) {
			gameplayModsData.gungameKillsLeft = 0;
			gameplayModsData.gungameTimeLeftUntilNextWeapon = gungameInfo->changeTime;

			if ( pPlayer->m_pActiveItem ) {
				snprintf( gameplayModsData.gungamePriorWeapon, 128, "%s", STRING( pPlayer->m_pActiveItem->pev->classname ) );
			}

			CBasePlayerItem *pItem;

			for ( int i = 0; i < MAX_ITEM_TYPES; i++ ) {
				pItem = pPlayer->m_rgpPlayerItems[i];

				while ( pItem ) {
					if ( CBasePlayerWeapon *weapon = dynamic_cast< CBasePlayerWeapon * >( pItem ) ) {
						if ( pItem->m_iId != WEAPON_CROWBAR ) {
							weapon->locked = TRUE;
						}
					}
					pItem = pItem->m_pNext;
				}
			}

			pPlayer->SendWeaponLockInfo();
		}
	} );

	if ( gungameInfo ) {
		bool shouldChangeWeapon =
			( gungameInfo->killsRequired > 0 && gameplayModsData.gungameKillsLeft <= 0 ) ||
			( gungameInfo->changeTime > 0.0f && gameplayModsData.gungameTimeLeftUntilNextWeapon <= 0.0f );

		if ( shouldChangeWeapon ) {
			static std::vector<std::string> gungameWeapons = {
				"weapon_9mmhandgun",
				"weapon_9mmhandgun_twin",
				"weapon_357",
				"weapon_9mmAR",
				"weapon_shotgun",
				"weapon_crossbow",
				"weapon_ingram",
				"weapon_ingram_twin",
				"weapon_m249",
				"weapon_rpg",
				"weapon_gauss",
				"weapon_egon",
				"weapon_hornetgun",
				"weapon_handgrenade",
				"weapon_satchel",
				"weapon_tripmine",
				"weapon_snark",
			};

			std::mt19937 gen( gameplayModsData.gungameSeed );

			static std::vector<std::string> gungameWeaponsQueue;
			gungameWeaponsQueue = gungameWeapons;
			if ( !gungameInfo->isSequential ) {
				std::shuffle( gungameWeaponsQueue.begin(), gungameWeaponsQueue.end(), gen );
			}

			if ( auto previousGungameWeapon = pPlayer->GetPlayerItem( gameplayModsData.gungameWeapon ) ) {
				if ( previousGungameWeapon->isGungameWeapon ) {
					pPlayer->RemovePlayerItem( previousGungameWeapon );
				} else {
					if ( previousGungameWeapon->m_iId != WEAPON_CROWBAR ) {
						previousGungameWeapon->locked = TRUE;
					}
				}
			}

			auto allowedIndex = CustomGameModeConfig::GetAllowedItemIndex( gungameWeaponsQueue.at( gameplayModsData.gungameWeaponSelection ).c_str() );
			auto nextGungameWeapon = allowedItems[allowedIndex];

			gameplayModsData.gungameWeaponSelection++;
			if ( gameplayModsData.gungameWeaponSelection >= gungameWeaponsQueue.size() ) {
				gameplayModsData.gungameWeaponSelection = 0;
				gameplayModsData.gungameSeed++;
			}

			snprintf( gameplayModsData.gungameWeapon, 128, "%s", nextGungameWeapon );

			auto existingWeapon = pPlayer->GetPlayerItem( nextGungameWeapon );
			if ( existingWeapon ) {
				existingWeapon->locked = FALSE;
			} else {
				pPlayer->GiveNamedItem( nextGungameWeapon, true );
				if ( auto gungameWeapon = pPlayer->GetPlayerItem( nextGungameWeapon ) ) {
					gungameWeapon->isGungameWeapon = TRUE;
				}
			}
			
			pPlayer->SelectItem( nextGungameWeapon );

			gameplayModsData.gungameKillsLeft = gungameInfo->killsRequired;
			gameplayModsData.gungameTimeLeftUntilNextWeapon = gungameInfo->changeTime;
			pPlayer->SendWeaponLockInfo();
		}

		if ( gungameInfo->changeTime > 0.0f ) {
			gameplayModsData.gungameTimeLeftUntilNextWeapon -= realTimeDelta;
		}
	} else {
		if ( !FStrEq( gameplayModsData.gungameWeapon, "" ) ) {
			snprintf( gameplayModsData.gungameWeapon, 128, "" );

			CBasePlayerItem *pItem;

			for ( int i = 0; i < MAX_ITEM_TYPES; i++ ) {
				pItem = pPlayer->m_rgpPlayerItems[i];

				while ( pItem ) {
					if ( CBasePlayerWeapon *weapon = dynamic_cast< CBasePlayerWeapon * >( pItem ) ) {
						weapon->locked = FALSE;
						if ( weapon->isGungameWeapon ) {
							pPlayer->RemovePlayerItem( weapon );
						}
					}
					pItem = pItem->m_pNext;
				}
			}

			pPlayer->SendWeaponLockInfo();

			if ( !FStrEq( gameplayModsData.gungamePriorWeapon, "" ) ) {
				pPlayer->SelectItem( gameplayModsData.gungamePriorWeapon );
				snprintf( gameplayModsData.gungamePriorWeapon, 128, "" );
			}
		}
	}

	if ( twitch && twitch->status == TWITCH_DISCONNECTED && ShouldInitializeTwitch() ) {

		auto twitch_credentials = aux::twitch::readCredentialsFromFile();
		auto login = twitch_credentials.first;
		auto password = twitch_credentials.second;
		if ( !login.empty() && !password.empty() ) {
			SendGameLogMessage( pPlayer, "Connecting to Twitch chat...", true );

			twitch_thread = twitch->Connect( login, password );
			twitch_thread.detach();
		}

	} else if ( twitch && twitch->status != TWITCH_DISCONNECTED && !ShouldInitializeTwitch() ) {
		SendGameLogMessage( pPlayer, "Disconnecting from Twitch chat", true );
		twitch->Disconnect();
	}
}

void CCustomGameModeRules::OnKilledEntityByPlayer( CBasePlayer *pPlayer, CBaseEntity *victim, KILLED_ENTITY_TYPE killedEntity, BOOL isHeadshot, BOOL killedByExplosion, BOOL killedByCrowbar ) {

	gameplayModsData.kills++;

	if ( killedByExplosion ) {
		gameplayModsData.explosiveKills++;
	} else if ( killedByCrowbar ) {
		gameplayModsData.crowbarKills++;
	} else if ( isHeadshot ) {
		gameplayModsData.headshotKills++;
	} else if ( killedEntity == KILLED_ENTITY_GRENADE ) {
		gameplayModsData.projectileKills++;
	}

	if ( auto teleportOnKillWeapon = gameplayMods::teleportOnKill.isActive<std::string>() ) {
		if (
			( pPlayer->m_pActiveItem && FStrEq( STRING( pPlayer->m_pActiveItem->pev->classname ), teleportOnKillWeapon->c_str() ) ) ||
			*teleportOnKillWeapon == "any"
		) {
			TraceResult tr;
			bool canTeleport = false;
			Vector destination = victim->pev->origin;
			for ( int i = 0; i < 73 && !canTeleport; i++ ) {
				UTIL_TraceHull( destination, destination, dont_ignore_monsters, human_hull, victim->edict(), &tr );
				if ( tr.fAllSolid ) {
					destination.z += 1.0f;
				} else {
					canTeleport = true;
				}
			}

			if ( canTeleport ) {
				UTIL_SetOrigin( pPlayer->pev, destination );
				if ( CBaseMonster *monster = dynamic_cast<CBaseMonster *>( victim ) ) {
					monster->GibMonster();
				}
			}
		}
	}

	if ( twitch && twitch->status == TWITCH_CONNECTED && CVAR_GET_FLOAT( "twitch_integration_random_kill_messages" ) > 0.0f && twitch->killfeedMessages.size() > 0 ) {
		Vector deathPos = victim->pev->origin;
		deathPos.z += victim->pev->size.z + 5.0f;
		auto it = aux::rand::choice( twitch->killfeedMessages.begin(), twitch->killfeedMessages.end() );
		if ( it != twitch->killfeedMessages.end() ) {
			SendGameLogWorldMessage( pPlayer, deathPos, *it, "", 3.0f );
			twitch->killfeedMessages.erase( it );
		}
	}

	if ( gameplayMods::blackMesaMinute.isActive() && pPlayer->pev->deadflag == DEAD_NO ) {
		CalculateScoreForBlackMesaMinute( pPlayer, victim, killedEntity, isHeadshot, killedByExplosion, killedByCrowbar );
	}

	if ( gameplayMods::scoreAttack.isActive() && pPlayer->pev->deadflag == DEAD_NO ) {
		CalculateScoreForScoreAttack( pPlayer, victim, killedEntity, isHeadshot, killedByExplosion, killedByCrowbar );
	}

	if ( gameplayMods::gungame.isActive() ) {
		gameplayModsData.gungameKillsLeft--;
	}

	CHalfLifeRules::OnKilledEntityByPlayer( pPlayer, victim, killedEntity, isHeadshot, killedByExplosion, killedByCrowbar );
}

void CCustomGameModeRules::CalculateScoreForBlackMesaMinute( CBasePlayer *pPlayer, CBaseEntity *victim, KILLED_ENTITY_TYPE killedEntity, BOOL isHeadshot, BOOL killedByExplosion, BOOL killedByCrowbar ) {
	int timeToAdd = 0;
	std::string message;

	if ( killedByExplosion ) {
		timeToAdd = 10;
		message = "EXPLOSION BONUS";
	} else if ( killedByCrowbar ) {
		timeToAdd = 6;
		message = "MELEE BONUS";
	} else if ( isHeadshot ) {
		timeToAdd = 6;
		message = "HEADSHOT BONUS";
	} else if ( killedEntity == KILLED_ENTITY_GRENADE ) {
		timeToAdd = 1;
		message = "PROJECTILE BONUS";
	} else {
		timeToAdd = 5;
		message = "TIME BONUS";
	}

	Vector deathPos = victim->pev->origin;
	deathPos.z += victim->pev->size.z + 5.0f;

	switch ( killedEntity ) {
		case KILLED_ENTITY_SENTRY:
			deathPos = victim->EyePosition() + Vector( 0, 0, 20 );
			break;

		case KILLED_ENTITY_SNIPER:
			deathPos = victim->pev->origin + ( victim->pev->mins + victim->pev->maxs ) * 0.5;
			break;

		case KILLED_ENTITY_NIHILANTH_CRYSTAL:
			deathPos = victim->pev->origin + ( victim->pev->mins + victim->pev->maxs ) * 0.5;
			timeToAdd = 10;
			break;

		case KILLED_ENTITY_GONARCH_SACK:
			timeToAdd = 10;
			break;

		case KILLED_ENTITY_SCIENTIST:
		case KILLED_ENTITY_BARNEY:
			timeToAdd = 0;

		default:
			break;
	}

	IncreaseTime( pPlayer, deathPos, timeToAdd, message.c_str() );
}

void CCustomGameModeRules::CalculateScoreForScoreAttack( CBasePlayer *pPlayer, CBaseEntity *victim, KILLED_ENTITY_TYPE killedEntity, BOOL isHeadshot, BOOL killedByExplosion, BOOL killedByCrowbar ) {
	int scoreToAdd = -1;
	float additionalMultiplier = 0.0f;

	switch ( killedEntity ) {
		case KILLED_ENTITY_BIG_MOMMA:
		case KILLED_ENTITY_GARGANTUA:
		case KILLED_ENTITY_ARMORED_VEHICLE:
		case KILLED_ENTITY_ICHTYOSAUR:
		case KILLED_ENTITY_NIHILANTH_CRYSTAL:
		case KILLED_ENTITY_GONARCH_SACK:
			scoreToAdd = 1000;
			break;

		case KILLED_ENTITY_ALIEN_CONTROLLER:
		case KILLED_ENTITY_ALIEN_GRUNT:
		case KILLED_ENTITY_HUMAN_ASSASSIN:
		case KILLED_ENTITY_HUMAN_GRUNT:
		case KILLED_ENTITY_SNIPER:
		case KILLED_ENTITY_ALIEN_SLAVE:
			scoreToAdd = 100;
			break;

		case KILLED_ENTITY_MINITURRET:
		case KILLED_ENTITY_SENTRY:
			scoreToAdd = 50;
			break;

		case KILLED_ENTITY_BULLSQUID:
		case KILLED_ENTITY_ZOMBIE:
			scoreToAdd = 30;
			break;

		case KILLED_ENTITY_HEADCRAB:
		case KILLED_ENTITY_HOUNDEYE:
		case KILLED_ENTITY_SNARK:
			scoreToAdd = 20;
			break;

		case KILLED_ENTITY_BARNACLE:
			scoreToAdd = 5;
			break;

		case KILLED_ENTITY_BABYCRAB:
		case KILLED_ENTITY_GRENADE:
			scoreToAdd = 0;
			break;

		default:
			return;
	}

	std::string message;

	if ( killedByExplosion ) {
		message = "EXPLOSION BONUS";
		additionalMultiplier = 1.0f;
	} else if ( killedByCrowbar ) {
		message = "MELEE BONUS";
		additionalMultiplier = 0.5f;
	} else if ( isHeadshot ) {
		message = "HEADSHOT BONUS";
		additionalMultiplier = 0.5f;
	} else if ( killedEntity == KILLED_ENTITY_GRENADE ) {
		message = "PROJECTILE BONUS";
	} else {
		message = "SCORE BONUS";
	}

	Vector deathPos = victim->pev->origin;
	deathPos.z += victim->pev->size.z + 5.0f;

	switch ( killedEntity ) {
	case KILLED_ENTITY_SENTRY:
		deathPos = victim->EyePosition() + Vector( 0, 0, 20 );
		break;

	case KILLED_ENTITY_SNIPER:
		deathPos = victim->pev->origin + ( victim->pev->mins + victim->pev->maxs ) * 0.5;
		break;

	case KILLED_ENTITY_NIHILANTH_CRYSTAL:
		deathPos = victim->pev->origin + ( victim->pev->mins + victim->pev->maxs ) * 0.5;
		break;

	default:
		break;
	}

	gameplayModsData.score += scoreToAdd * ( gameplayModsData.comboMultiplier + additionalMultiplier );

	if ( scoreToAdd != -1 ) {

		if ( scoreToAdd > 0 ) {
			SendGameLogMessage( pPlayer, message );

			float comboMultiplier = gameplayModsData.comboMultiplier + additionalMultiplier;
			bool comboMultiplierIsInteger = abs( comboMultiplier - std::lround( comboMultiplier ) ) < 0.00000001f;

			char upperString[128];
			if ( comboMultiplierIsInteger ) {
				std::sprintf( upperString, "%d x%.0f", scoreToAdd, comboMultiplier );
			} else {
				std::sprintf( upperString, "%d x%.1f", scoreToAdd, comboMultiplier );
			}

			if ( ( twitch && twitch->status != TWITCH_CONNECTED ) || CVAR_GET_FLOAT( "twitch_integration_random_kill_messages" ) == 0.0f ) {
				if ( gameplayMods::blackMesaMinute.isActive() ) {
					SendGameLogWorldMessage( pPlayer, deathPos, "", std::string( upperString ) + " / " + std::to_string( ( int ) ( scoreToAdd * comboMultiplier ) ) );
				} else {
					SendGameLogWorldMessage( pPlayer, deathPos, std::to_string( ( int ) ( scoreToAdd * comboMultiplier ) ), upperString );
				}
			}
		}

		switch ( killedEntity ) {
			case KILLED_ENTITY_BABYCRAB:
			case KILLED_ENTITY_BARNACLE:
			case KILLED_ENTITY_GRENADE:
				// don't add to combo multiplier
				break;

			default:
				gameplayModsData.comboMultiplier++;
				break;
		}

		gameplayModsData.comboMultiplierReset = COMBO_MULTIPLIER_DECAY_TIME;
	}
}

void CCustomGameModeRules::VoteForRandomGameplayMod( CBasePlayer *pPlayer, const std::string &voter, size_t modIndex ) {
	using namespace gameplayMods;
	
	if ( modIndex < 0 || modIndex > proposedGameplayMods.size() - 1 ) {
		return;
	}

	for ( auto &proposedMod : proposedGameplayMods ) {
		proposedMod.votes.erase( voter );
	}

	proposedGameplayMods.at( modIndex ).votes.insert( voter );

	size_t totalVotes = 0;
	size_t maxVoteCount = 0;
	for ( auto &mod : proposedGameplayMods ) {
		totalVotes += mod.votes.size();
		maxVoteCount = max( maxVoteCount, mod.votes.size() );
	}
	for ( auto &mod : proposedGameplayMods ) {
		mod.voteDistributionPercent = ( mod.votes.size() / ( float ) totalVotes ) * 100;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgPropModVin, NULL, pPlayer->pev );
		WRITE_SHORT( proposedGameplayMods.size() - 1 - modIndex );
		WRITE_STRING( voter.c_str() );
	MESSAGE_END();
}

void CCustomGameModeRules::VoteForRandomGameplayMod( CBasePlayer *pPlayer, const std::string &voter, const std::string &modStringIndex ) {
	std::regex vote_index_regex( "^[#!0]*([1-9]{1}).*" );
	std::smatch base_match;
	if ( std::regex_match( modStringIndex, base_match, vote_index_regex ) ) {
		if ( base_match.size() == 2 ) {
			int modIndex = std::stoi( base_match[1].str() ) - 1;
			VoteForRandomGameplayMod( pPlayer, voter, modIndex );
		}
	}
}

void CCustomGameModeRules::SendHUDMessages( CBasePlayer *pPlayer ) {
	const int SPACING = 56;
	int yOffset = 0;

	if ( gameplayMods::timerShown.isActive() ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgTimerValue, NULL, pPlayer->pev );
			WRITE_STRING(
				gameplayMods::timerShownReal.isActive() ? "REAL TIME" :
				gameplayMods::timeRestriction.isActive() ? "TIME LEFT" :
				"TIME"
			);
			WRITE_FLOAT( gameplayMods::timerShownReal.isActive() ? gameplayModsData.realTime : gameplayModsData.time );
			WRITE_LONG( yOffset );
		MESSAGE_END();

		yOffset += SPACING;
	}

	if ( gameplayMods::scoreAttack.isActive() ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgScoreValue, NULL, pPlayer->pev );
			WRITE_LONG( gameplayModsData.score );
			WRITE_LONG( gameplayModsData.comboMultiplier );
			WRITE_FLOAT( gameplayModsData.comboMultiplierReset );
			WRITE_LONG( yOffset );
		MESSAGE_END();

		yOffset += SPACING;
	}

	struct CounterData {
		int count;
		int maxCount;
		std::string text;
		int offset;
	};

	std::vector<CounterData> counterData;
	for ( auto &endCondition : config.endConditions ) {
		counterData.push_back( {
			endCondition.activations,
			endCondition.activationsRequired,
			endCondition.objective,
			endCondition.activationsRequired > 1 ? SPACING : SPACING - 34
		} );
	}
	if ( gameplayMods::kerotanDetector.isActive() ) {
		auto chapterMaps = pPlayer->GetCurrentChapterMapNames();
		counterData.push_back( {
			pPlayer->GetAmountOfKerotansInCurrentChapter(),
			( int ) chapterMaps.second.size(),
			chapterMaps.first.c_str(),
			SPACING
		} );
	}
	
	if ( auto gungame = gameplayMods::gungame.isActive<GunGameInfo>() ) {
		auto chapterMaps = pPlayer->GetCurrentChapterMapNames();
		if ( gungame->killsRequired ) {
			counterData.push_back( {
				gungame->killsRequired > 1 ? gungame->killsRequired - gameplayModsData.gungameKillsLeft : -1,
				gungame->killsRequired > 1 ? gungame->killsRequired : -1,
				gungame->killsRequired > 1 ? "KILLS UNTIL NEXT WEAPON" : "KILL TO GET THE NEXT WEAPON",
				SPACING
			} );
		}
		
		if ( gungame->changeTime ) {
			counterData.push_back( {
				( int ) std::ceil( gameplayModsData.gungameTimeLeftUntilNextWeapon ),
				-1,
				"TIME LEFT UNTIL NEXT WEAPON",
				SPACING
			} );
		}
	}
	if ( gameplayMods::timescaleOnDamage.isActive() ) {
		auto timescale_multiplier = *gameplayMods::timescale.isActive<float>() + gameplayModsData.timescaleAdditive;

		counterData.push_back( {
			-1,
			-1,
			fmt::sprintf( "TIMESCALE BASE: %.2f", timescale_multiplier ),
			SPACING - 34
		} );
	}

	int conditionsHeight = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgCountLen, NULL, pPlayer->pev );
		WRITE_SHORT( counterData.size() );
	MESSAGE_END();

	for ( size_t i = 0 ; i < counterData.size() ; i++ ) {
		auto &counter = counterData.at( i );
		MESSAGE_BEGIN( MSG_ONE, gmsgCountValue, NULL, pPlayer->pev );
			WRITE_SHORT( i );
			WRITE_LONG( counter.count );
			WRITE_LONG( counter.maxCount );
			WRITE_STRING( counter.text.c_str() );
		MESSAGE_END();

		conditionsHeight += counter.offset;
	}
	
	MESSAGE_BEGIN( MSG_ONE, gmsgCountOffse, NULL, pPlayer->pev );
		WRITE_LONG( yOffset );
	MESSAGE_END();

	yOffset += conditionsHeight;
	maxYOffset = max( yOffset, maxYOffset );
}

void CCustomGameModeRules::ActivateEndMarkers( CBasePlayer *pPlayer ) {
	if ( !endMarkersActive ) {
		endMarkersActive = true;
		if ( pPlayer ) {
			EMIT_SOUND( ENT( pPlayer->pev ), CHAN_STATIC, "var/end_marker.wav", 1, ATTN_NORM, true );
			SendGameLogMessage( pPlayer, "HEAD TO THE GOAL" );
		}
	}

	tasks.push_back( { 0.0f, []( CBasePlayer *pPlayer ) {
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, Vector( 0, 0, 0 ), 8192 ) ) != NULL ) {
			if ( FStrEq( STRING( pEntity->pev->classname ), "end_marker" ) ) {
				pEntity->Use( NULL, NULL, USE_ON, 0.0f );
			}
		}
	} } );
}

void CCustomGameModeRules::CheckForCheats( CBasePlayer *pPlayer )
{
	if ( gameplayModsData.cheated && cheatedMessageSent || ended ) {
		return;
	}

	if ( gameplayModsData.cheated ) {
		OnCheated( pPlayer );
		return;
	}

	if ( ( pPlayer->pev->flags & FL_GODMODE && !gameplayMods::godConstant.isActive() ) ||
		 ( pPlayer->pev->flags & FL_NOTARGET && !gameplayMods::noTargetConstant.isActive() ) ||
		 ( pPlayer->pev->movetype & MOVETYPE_NOCLIP ) ||
		gameplayModsData.usedCheat || startMapDoesntMatch ||
		 std::string( gameplayModsData.activeGameModeConfigHash ) != config.sha1
	) {
		SendGameLogMessage( pPlayer, "RESULTS WON'T BE SAVED" );
		SendGameLogMessage( pPlayer, "YOU'VE BEEN CHEATING" );
		gameplayModsData.cheated = true;
	}

}

void CCustomGameModeRules::OnCheated( CBasePlayer *pPlayer ) {
	cheatedMessageSent = true;

	MESSAGE_BEGIN( MSG_ONE, gmsgTimerCheat, NULL, pPlayer->pev );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgCountCheat, NULL, pPlayer->pev );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgScoreCheat, NULL, pPlayer->pev );
	MESSAGE_END();
}

void CCustomGameModeRules::OnEnd( CBasePlayer *pPlayer ) {
	PauseTimer( pPlayer );

	auto timeRestriction = gameplayMods::timeRestriction.isActive();

	if ( !config.gameFinishedOnce && timeRestriction ) {
		config.record.time = 0.0f;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgGLogDeact, NULL, pPlayer->pev );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgGLogWDeact, NULL, pPlayer->pev );
	MESSAGE_END();
	
	MESSAGE_BEGIN( MSG_ONE, gmsgEndTitle, NULL, pPlayer->pev );
		WRITE_STRING( config.ConfigTypeToGameModeName( true ).c_str() );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgEndTitle, NULL, pPlayer->pev );
		WRITE_STRING(
			config.name.empty() ?
			"LEVEL COMPLETE" :
			( config.name + " - COMPLETE" ).c_str()
		);
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgTimerDeact, NULL, pPlayer->pev );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgEndTime, NULL, pPlayer->pev );
		WRITE_STRING( timeRestriction ? "TIME SCORE|PERSONAL BESTS" : "TIME|PERSONAL BESTS" );
		WRITE_FLOAT( gameplayModsData.time );
		WRITE_FLOAT( config.record.time );
		WRITE_BYTE( timeRestriction ? gameplayModsData.time > config.record.time : gameplayModsData.time < config.record.time  );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgEndTime, NULL, pPlayer->pev );
		WRITE_STRING( "REAL TIME" );
		WRITE_FLOAT( gameplayModsData.realTime );
		WRITE_FLOAT( config.record.realTime );
		WRITE_BYTE( gameplayModsData.realTime < config.record.realTime );
	MESSAGE_END();

	if ( timeRestriction ) {
		float realTimeMinusTime = max( 0.0f, gameplayModsData.realTime - gameplayModsData.time );
		MESSAGE_BEGIN( MSG_ONE, gmsgEndTime, NULL, pPlayer->pev );
			WRITE_STRING( "REAL TIME MINUS SCORE" );
			WRITE_FLOAT( realTimeMinusTime );
			WRITE_FLOAT( config.record.realTimeMinusTime );
			WRITE_BYTE( realTimeMinusTime < config.record.realTimeMinusTime );
		MESSAGE_END();
	}

	int secondsInSlowmotion = ( int ) roundf( gameplayModsData.secondsInSlowmotion );
	if ( secondsInSlowmotion > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "SECONDS IN SLOWMOTION|" + std::to_string( secondsInSlowmotion ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayModsData.kills > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "TOTAL KILLS|" + std::to_string( gameplayModsData.kills ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayModsData.headshotKills > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "HEADSHOT KILLS|" + std::to_string( gameplayModsData.headshotKills ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayModsData.explosiveKills > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "EXPLOSION KILLS|" + std::to_string( gameplayModsData.explosiveKills ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayModsData.crowbarKills > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "MELEE KILLS|" + std::to_string( gameplayModsData.crowbarKills ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayModsData.projectileKills > 0 ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgEndStat, NULL, pPlayer->pev );
			WRITE_STRING( ( "PROJECTILES DESTROYED KILLS|" + std::to_string( gameplayModsData.projectileKills ) ).c_str() );
		MESSAGE_END();
	}

	if ( gameplayMods::scoreAttack.isActive() ) {
		MESSAGE_BEGIN( MSG_ONE, gmsgScoreDeact, NULL, pPlayer->pev );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgEndScore, NULL, pPlayer->pev );
			WRITE_STRING( "SCORE|PERSONAL BEST" );
			WRITE_LONG( gameplayModsData.score );
			WRITE_LONG( config.record.score );
			WRITE_BYTE( gameplayModsData.score > config.record.score );
		MESSAGE_END();
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgEndActiv, NULL, pPlayer->pev );
		WRITE_BYTE( gameplayModsData.cheated );
	MESSAGE_END();

	if ( !gameplayModsData.cheated ) {
		config.record.Save( pPlayer );
	}
}

void CCustomGameModeRules::OnHookedModelIndex( CBasePlayer *pPlayer, CBaseEntity *activator, int modelIndex, const std::string &className, const std::string &targetName, bool firstTime )
{
	CHalfLifeRules::OnHookedModelIndex( pPlayer, activator, modelIndex, className, targetName, firstTime );
	
	if ( config.endTrigger.Fits( modelIndex, className, targetName, firstTime ) ) {
		End( pPlayer );
	}

	for ( const auto &potentialIntermission : config.intermissions ) {
		if ( potentialIntermission.Fits( modelIndex, className, targetName, firstTime ) ) {
			g_latestIntermission = potentialIntermission;
			CHANGE_LEVEL( ( char * ) g_latestIntermission.entityName.c_str(), NULL );
			// after that, g_latestIntermission becomes undefined in PlayerSpawn function
			break;
		}
	}

	if (
		std::string( STRING( gpGlobals->mapname ) ) == "c1a0e" &&
		gameplayMods::snarkPenguins.isActive() &&
		gameplayMods::snarkFriendlyToPlayer.isActive() &&
		gameplayMods::snarkFriendlyToAllies.isActive()
	) {
		CBaseEntity *list[512];
		int amount = UTIL_MonstersInSphere( list, 512, Vector( 0, 0, 0 ), 8192.0f );

		if ( targetName == "se_motor_sound" ) {
			
			for ( int i = 0 ; i < amount ; i++ ) {
				if ( CSqueakGrenade *penguin = dynamic_cast<CSqueakGrenade *>( list[i] ) ) {
					if ( !penguin->isPanic ) {
						penguin->pev->velocity = Vector( 0, 0, 0 );
						penguin->pev->angles = UTIL_VecToAngles( activator->pev->origin - penguin->pev->origin );
						penguin->pev->angles[0] = 0;
						penguin->pev->sequence = 1;
						penguin->isStill = true;
					}
				}
			}
		} else if ( targetName == "speaker_ohno" ) {

			for ( int i = 0 ; i < amount ; i++ ) {
				if ( CSqueakGrenade *penguin = dynamic_cast<CSqueakGrenade *>( list[i] ) ) {
					penguin->pev->sequence = 2;
					penguin->isStill = false;
					penguin->isPanic = true;
				}
			}
		}
	}

	if (
		targetName == "on_map_start" &&
		std::string( STRING( gpGlobals->mapname ) ) == "c2a3d" &&
		!gameplayMods::preserveNightmare.isActive()
	) {
		for ( auto it = mapConfig.intermissions.begin(); it != mapConfig.intermissions.end(); it++ ) {
			if ( it->entityName == "nightmare" ) {
				mapConfig.intermissions.erase( it );
				break;
			}
		}
	}

	if ( targetName == "on_map_start" && gameplayMods::preventMonsterSpawn.isActive() ) {

		for ( int i = 0 ; i < 1024 ; i++ ) {
			edict_t *edict = g_engfuncs.pfnPEntityOfEntIndex( i );
			if ( !edict ) {
				continue;
			}

			if ( CBaseEntity *entity = CBaseEntity::Instance( edict ) ) {
				if ( entity->pev->spawnflags & SF_MONSTER_PRESERVE ) {
					continue;
				}

				if ( std::string( STRING( entity->pev->classname ) ).find( "monster_" ) == 0 ) {
					entity->pev->flags |= FL_KILLME;
				}
			}
		}
	}

	for ( const auto &timerPause : config.timerPauses ) {
		if ( timerPause.Fits( modelIndex, className, targetName, firstTime ) ) {
			PauseTimer( pPlayer );
		}
	}

	for ( const auto &timerResume : config.timerResumes ) {
		if ( timerResume.Fits( modelIndex, className, targetName, firstTime ) ) {
			ResumeTimer( pPlayer );
		}
	}

	for ( auto &condition : config.endConditions ) {
		if ( condition.Fits( modelIndex, className, targetName, firstTime ) ) {
			condition.activations++;

			if ( condition.activations >= condition.activationsRequired ) {

				bool allComplete = true;

				for ( const auto &condition : config.endConditions ) {
					if ( condition.activations < condition.activationsRequired ) {
						allComplete = false;
						break;
					}
				}

				if ( allComplete ) {
					if ( config.hasEndMarkers ) {
						ActivateEndMarkers( pPlayer );
					} else {
						End( pPlayer );
					}
				}
			}
		}
	}

	for ( const auto &teleporterRedirect : config.teleports ) {
		if ( teleporterRedirect.Fits( modelIndex, className, targetName, true ) ) {
			ApplyStartPositionToEntity( pPlayer, teleporterRedirect.pos );
		}
	}

}

void CCustomGameModeRules::OnChangeLevel() {
	CHalfLifeRules::OnChangeLevel();
	musicSwitchDelay = 0.0f;

	for ( auto &spawner : entityRandomSpawnerControllers ) {
		spawner.ResetSpawnPeriod();
	}

	if ( endMarkersActive ) {
		ActivateEndMarkers();
	}

	using namespace gameplayMods;
	tasks.push_back( { 0.0f, []( CBasePlayer *pPlayer ) {
		for ( auto i = proposedGameplayMods.begin(); i != proposedGameplayMods.end(); ) {
			if ( i->mod->canBeCancelledAfterChangeLevel && !i->mod->CanBeActivatedRandomly() ) {
				i = proposedGameplayMods.erase( i );
			} else {
				i++;
			}
		}

		for ( auto i = timedGameplayMods.begin(); i != timedGameplayMods.end(); ) {
			if ( i->mod->canBeCancelledAfterChangeLevel && !i->mod->CanBeActivatedRandomly() ) {
				i = timedGameplayMods.erase( i );
			} else {
				i++;
			}
		}
	} } );

	if ( kerotanDetector.isActive() ) {
		tasks.push_back( { 0.0f, [this]( CBasePlayer *pPlayer ) {
			CBaseEntity *pEntity = NULL;
			while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, Vector( 0, 0, 0 ), 8192 ) ) != NULL ) {
				if ( CKerotan *kerotan = dynamic_cast<CKerotan *>( pEntity ) ) {
					if ( FStrEq( STRING( kerotan->mapName ), STRING( gpGlobals->mapname ) ) ) {
						SendGameLogMessage( pPlayer, fmt::sprintf( "%s: %s", STRING( gpGlobals->mapname ), kerotan->hasBeenFound ? "Kerotan has been found" : "Kerotan not found" ) );
						break;
					}
				}
			}
		} } );
	}
}

extern int g_autoSaved;
void CCustomGameModeRules::OnSave( CBasePlayer *pPlayer ) {
	if ( gameplayMods::noSaving.isActive() && !g_autoSaved ) {
		SendGameLogMessage( pPlayer, "SAVING IS ACTUALLY DISABLED" );
	}
	CHalfLifeRules::OnSave( pPlayer );
}

void CCustomGameModeRules::PauseTimer( CBasePlayer *pPlayer )
{
	if ( gameplayModsData.timerPaused ) {
		return;
	}

	gameplayModsData.timerPaused = true;

	MESSAGE_BEGIN( MSG_ONE, gmsgTimerPause, NULL, pPlayer->pev );
		WRITE_BYTE( true );
	MESSAGE_END();
}

void CCustomGameModeRules::ResumeTimer( CBasePlayer *pPlayer )
{
	if ( !gameplayModsData.timerPaused ) {
		return;
	}

	gameplayModsData.timerPaused = false;

	MESSAGE_BEGIN( MSG_ONE, gmsgTimerPause, NULL, pPlayer->pev );
		WRITE_BYTE( false );
	MESSAGE_END();
}

void CCustomGameModeRules::IncreaseTime( CBasePlayer *pPlayer, const Vector &eventPos, int timeToAdd, const char *message ) {
	if ( gameplayModsData.timerPaused || timeToAdd <= 0 ) {
		return;
	}

	gameplayModsData.time += timeToAdd;

	SendGameLogMessage( pPlayer, message );

	char timeAddedCString[6];
	sprintf( timeAddedCString, "00:%02d", timeToAdd ); // mm:ss
	const std::string timeAddedString = std::string( timeAddedCString );

	if ( ( twitch && twitch->status != TWITCH_CONNECTED ) || CVAR_GET_FLOAT( "twitch_integration_random_kill_messages" ) == 0.0f ) {
		SendGameLogWorldMessage( pPlayer, eventPos, timeAddedString );
	}
}

// Hardcoded values so it won't depend on console variables
void CCustomGameModeRules::RefreshSkillData() 
{
	gSkillData.barneyHealth = 35;
	gSkillData.slaveDmgClawrake = 25.0f;

	gSkillData.leechHealth = 2.0f;
	gSkillData.leechDmgBite = 2.0f;

	gSkillData.scientistHealth = 20.0f;

	gSkillData.snarkHealth = 2.0f;
	gSkillData.snarkDmgBite = 10.0f;
	gSkillData.snarkDmgPop = 5.0f;

	gSkillData.plrDmgCrowbar = 10.0f;
	gSkillData.plrDmg9MM = 8.0f;
	gSkillData.plrDmg357 = 40.0f;
	gSkillData.plrDmgMP5 = 5.0f;
	gSkillData.plrDmgM203Grenade = 100.0f;
	gSkillData.plrDmgBuckshot = 5.0f;
	gSkillData.plrDmgCrossbowClient = 10.0f;
	gSkillData.plrDmgCrossbowMonster = 50.0f;
	gSkillData.plrDmgRPG = 100.0f;
	gSkillData.plrDmgGauss = 20.0f;
	gSkillData.plrDmgEgonNarrow = 6.0f;
	gSkillData.plrDmgEgonWide = 14.0f;
	gSkillData.plrDmgHornet = 7;
	gSkillData.plrDmgHandGrenade = 100.0f;
	gSkillData.plrDmgSatchel = 150.0f;
	gSkillData.plrDmgTripmine = 150.0f;

	gSkillData.healthkitCapacity = 15.0f; // doesn't matter - it's painkiller
	gSkillData.scientistHeal = 25.0f;

	gSkillData.monHead = 3.0f;
	gSkillData.monChest = 1.0f;
	gSkillData.monStomach = 1.0f;
	gSkillData.monLeg = 1.0f;
	gSkillData.monArm = 1.0f;

	gSkillData.plrHead = 3.0f;
	gSkillData.plrChest = 1.0f;
	gSkillData.plrStomach = 1.0f;
	gSkillData.plrLeg = 1.0f;
	gSkillData.plrArm = 1.0f;

	if ( gameplayMods::difficultyEasy.isActive() ) {
		
		gSkillData.iSkillLevel = 1;

		gSkillData.agruntHealth = 60.0f;
		gSkillData.agruntDmgPunch = 10.0f;

		gSkillData.apacheHealth = 150.0f;
	
		gSkillData.bigmommaHealthFactor = 1.0f;
		gSkillData.bigmommaDmgSlash = 50.0f;
		gSkillData.bigmommaDmgBlast = 100.0f;
		gSkillData.bigmommaRadiusBlast = 250.0f;

		gSkillData.bullsquidHealth = 40.0f;
		gSkillData.bullsquidDmgBite = 15.0f;
		gSkillData.bullsquidDmgWhip = 25.0f;
		gSkillData.bullsquidDmgSpit = 10.0f;

		gSkillData.gargantuaHealth = 800.0f;
		gSkillData.gargantuaDmgSlash = 10.0f;
		gSkillData.gargantuaDmgFire = 3.0f;
		gSkillData.gargantuaDmgStomp = 50.0f;

		gSkillData.hassassinHealth = 30.0f;

		gSkillData.headcrabHealth = 10.0f;
		gSkillData.headcrabDmgBite = 5.0f;

		gSkillData.hgruntHealth = 50.0f;
		gSkillData.hgruntDmgKick = 5.0f;
		gSkillData.hgruntShotgunPellets = 3.0f;
		gSkillData.hgruntGrenadeSpeed = 400.0f;

		gSkillData.houndeyeHealth = 20.0f;
		gSkillData.houndeyeDmgBlast = 10.0f;

		gSkillData.slaveHealth = 30.0f;
		gSkillData.slaveDmgClaw = 8.0f;
		gSkillData.slaveDmgZap = 10.0f;

		gSkillData.ichthyosaurHealth = 200.0f;
		gSkillData.ichthyosaurDmgShake = 20.0f;

		gSkillData.controllerHealth = 60.0f;
		gSkillData.controllerDmgZap = 15.0f;
		gSkillData.controllerSpeedBall = 650.0f;
		gSkillData.controllerDmgBall = 3.0f;

		gSkillData.nihilanthHealth = 800.0f;
		gSkillData.nihilanthZap = 30.0f;
	
		gSkillData.zombieHealth = 50.0f;
		gSkillData.zombieDmgOneSlash = 10.0f;
		gSkillData.zombieDmgBothSlash = 25.0f;

		gSkillData.turretHealth = 50.0f;
		gSkillData.miniturretHealth = 40.0f;
		gSkillData.sentryHealth = 40.0f;

		gSkillData.monDmg12MM = 8.0f;
		gSkillData.monDmgMP5 = 3.0f;
		gSkillData.monDmg9MM = 5.0f;
		
		gSkillData.monDmgHornet = 4.0f;

		gSkillData.suitchargerCapacity = 75.0f;
		gSkillData.batteryCapacity = 15.0f;
		gSkillData.healthchargerCapacity = 50.0f;
		
	} else if ( gameplayMods::difficultyHard.isActive() ) {
		gSkillData.iSkillLevel = 3;

		gSkillData.agruntHealth = 120.0f;
		gSkillData.agruntDmgPunch = 20.0f;

		gSkillData.apacheHealth = 400.0f;
	
		gSkillData.bigmommaHealthFactor = 2.0f;
		gSkillData.bigmommaDmgSlash = 70.0f;
		gSkillData.bigmommaDmgBlast = 160.0f;
		gSkillData.bigmommaRadiusBlast = 275.0f;

		gSkillData.bullsquidHealth = 40.0f;
		gSkillData.bullsquidDmgBite = 25.0f;
		gSkillData.bullsquidDmgWhip = 35.0f;
		gSkillData.bullsquidDmgSpit = 10.0f;

		gSkillData.gargantuaHealth = 1000.0f;
		gSkillData.gargantuaDmgSlash = 30.0f;
		gSkillData.gargantuaDmgFire = 5.0f;
		gSkillData.gargantuaDmgStomp = 100.0f;

		gSkillData.hassassinHealth = 50.0f;

		gSkillData.headcrabHealth = 20.0f;
		gSkillData.headcrabDmgBite = 10.0f;

		gSkillData.hgruntHealth = 80.0f;
		gSkillData.hgruntDmgKick = 10.0f;
		gSkillData.hgruntShotgunPellets = 6.0f;
		gSkillData.hgruntGrenadeSpeed = 800.0f;

		gSkillData.houndeyeHealth = 30.0f;
		gSkillData.houndeyeDmgBlast = 15.0f;

		gSkillData.slaveHealth = 60.0f;
		gSkillData.slaveDmgClaw = 10.0f;
		gSkillData.slaveDmgZap = 15.0f;

		gSkillData.ichthyosaurHealth = 400.0f;
		gSkillData.ichthyosaurDmgShake = 50.0f;

		gSkillData.controllerHealth = 100.0f;
		gSkillData.controllerDmgZap = 35.0f;
		gSkillData.controllerSpeedBall = 1000.0f;
		gSkillData.controllerDmgBall = 5.0f;

		gSkillData.nihilanthHealth = 1000.0f;
		gSkillData.nihilanthZap = 50.0f;
	
		gSkillData.zombieHealth = 100.0f;
		gSkillData.zombieDmgOneSlash = 20.0f;
		gSkillData.zombieDmgBothSlash = 40.0f;

		gSkillData.turretHealth = 60.0f;
		gSkillData.miniturretHealth = 50.0f;
		gSkillData.sentryHealth = 50.0f;

		gSkillData.monDmg12MM = 10.0f;
		gSkillData.monDmgMP5 = 5.0f;
		gSkillData.monDmg9MM = 8.0f;
		
		gSkillData.monDmgHornet = 8.0f;

		gSkillData.suitchargerCapacity = 35.0f;
		gSkillData.batteryCapacity = 10.0f;
		gSkillData.healthchargerCapacity = 25.0f;
	} else {
		gSkillData.iSkillLevel = 2;

		gSkillData.agruntHealth = 90.0f;
		gSkillData.agruntDmgPunch = 20.0f;

		gSkillData.apacheHealth = 250.0f;

		gSkillData.bigmommaHealthFactor = 1.5f;
		gSkillData.bigmommaDmgSlash = 60.0f;
		gSkillData.bigmommaDmgBlast = 120.0f;
		gSkillData.bigmommaRadiusBlast = 250.0f;

		gSkillData.bullsquidHealth = 40.0f;
		gSkillData.bullsquidDmgBite = 25.0f;
		gSkillData.bullsquidDmgWhip = 35.0f;
		gSkillData.bullsquidDmgSpit = 10.0f;

		gSkillData.gargantuaHealth = 800.0f;
		gSkillData.gargantuaDmgSlash = 30.0f;
		gSkillData.gargantuaDmgFire = 5.0f;
		gSkillData.gargantuaDmgStomp = 100.0f;

		gSkillData.hassassinHealth = 50.0f;

		gSkillData.headcrabHealth = 10.0f;
		gSkillData.headcrabDmgBite = 10.0f;

		gSkillData.hgruntHealth = 50.0f;
		gSkillData.hgruntDmgKick = 10.0f;
		gSkillData.hgruntShotgunPellets = 5.0f;
		gSkillData.hgruntGrenadeSpeed = 600.0f;

		gSkillData.houndeyeHealth = 20.0f;
		gSkillData.houndeyeDmgBlast = 15.0f;

		gSkillData.slaveHealth = 30.0f;
		gSkillData.slaveDmgClaw = 10.0f;
		gSkillData.slaveDmgZap = 10.0f;

		gSkillData.ichthyosaurHealth = 200.0f;
		gSkillData.ichthyosaurDmgShake = 35.0f;

		gSkillData.controllerHealth = 60.0f;
		gSkillData.controllerDmgZap = 25.0f;
		gSkillData.controllerSpeedBall = 800.0f;
		gSkillData.controllerDmgBall = 4.0f;

		gSkillData.nihilanthHealth = 800.0f;
		gSkillData.nihilanthZap = 30.0f;

		gSkillData.zombieHealth = 50.0f;
		gSkillData.zombieDmgOneSlash = 20.0f;
		gSkillData.zombieDmgBothSlash = 40.0f;

		gSkillData.turretHealth = 50.0f;
		gSkillData.miniturretHealth = 40.0f;
		gSkillData.sentryHealth = 40.0f;

		gSkillData.monDmg12MM = 10.0f;
		gSkillData.monDmgMP5 = 4.0f;
		gSkillData.monDmg9MM = 5.0f;

		gSkillData.monDmgHornet = 5.0f;

		gSkillData.suitchargerCapacity = 50.0f;
		gSkillData.batteryCapacity = 15.0f;
		gSkillData.healthchargerCapacity = 40.0f;
	}
}

EntityRandomSpawnerController::EntityRandomSpawnerController( const EntityRandomSpawner &entityRandomSpawner ) :
	mapName( entityRandomSpawner.mapName ),
	spawnData( entityRandomSpawner.entity ),
	maxAmount( entityRandomSpawner.maxAmount ),
	spawnPeriod( entityRandomSpawner.spawnPeriod ),
	nextSpawn( gpGlobals->time + entityRandomSpawner.spawnPeriod )
{ }

void EntityRandomSpawnerController::Think( CBasePlayer *pPlayer ) {
	if ( gpGlobals->time > nextSpawn ) {
		ResetSpawnPeriod();

		if (
			FStrEq( STRING( gpGlobals->mapname ), mapName.c_str() ) ||
			mapName == "everywhere"
		) {
			Spawn( pPlayer );
		}
	}
}

void EntityRandomSpawnerController::ResetSpawnPeriod() {
	nextSpawn = gpGlobals->time + spawnPeriod;
}

void EntityRandomSpawnerController::Spawn( CBasePlayer *pPlayer ) {
	CBaseEntity *list[1024] = { NULL };
	int amountOfMonsters = UTIL_MonstersInSphere( list, 1024, Vector( 0, 0, 0 ), 8192.0f );
	int amountOfMonstersOfThisType = 0;
	for ( int i = 0 ; i < amountOfMonsters ; i++ ) {
		CBaseEntity *entity = list[i];
		if (
			( entity->pev->spawnflags & SF_MONSTER_PRESERVE ) &&
		   !( entity->pev->deadflag & DEAD_DEAD ) &&
			FStrEq( STRING( entity->pev->classname ), spawnData.name.c_str() )
		) {
			amountOfMonstersOfThisType++;
		}
	}

	if ( amountOfMonstersOfThisType >= maxAmount ) {
		return;
	}

	if ( spawnData.DetermineBestSpawnPosition( pPlayer ) ) {
		CCustomGameModeRules::SpawnBySpawnData( spawnData );
	}
}

