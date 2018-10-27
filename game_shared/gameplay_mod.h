#ifndef GAMEPLAY_MOD_H
#define GAMEPLAY_MOD_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "argument.h"
#include <map>
#include <set>

#ifdef CLIENT_DLL
#include "wrect.h"
#include "../cl_dll/cl_dll.h"
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif // CLIENT_DLL

#include <vector>

enum BULLET_PHYSICS_MODE {
	BULLET_PHYSICS_DISABLED,
	BULLET_PHYSICS_ENEMIES_ONLY_ON_SLOWMOTION,
	BULLET_PHYSICS_ENEMIES_AND_PLAYER_ON_SLOWMOTION,
	BULLET_PHYSICS_CONSTANT
};

enum GAME_MODE {
	GAME_MODE_VANILLA,
	GAME_MODE_CUSTOM
};

enum GAMEPLAY_MOD {
	GAMEPLAY_MOD_ALWAYS_GIB,
	GAMEPLAY_MOD_BLACK_MESA_MINUTE,
	GAMEPLAY_MOD_BLEEDING,
	GAMEPLAY_MOD_BULLET_DELAY_ON_SLOWMOTION,
	GAMEPLAY_MOD_BULLET_PHYSICS_CONSTANT,
	GAMEPLAY_MOD_BULLET_PHYSICS_DISABLED,
	GAMEPLAY_MOD_BULLET_PHYSICS_ENEMIES_AND_PLAYER_ON_SLOWMOTION,
	GAMEPLAY_MOD_BULLET_RICOCHET,
	GAMEPLAY_MOD_BULLET_SELF_HARM,
	GAMEPLAY_MOD_BULLET_TRAIL_CONSTANT,
	GAMEPLAY_MOD_CONSTANT_SLOWMOTION,
	GAMEPLAY_MOD_CROSSBOW_EXPLOSIVE_BOLTS,
	GAMEPLAY_MOD_DETACHABLE_TRIPMINES,
	GAMEPLAY_MOD_DIVING_ALLOWED_WITHOUT_SLOWMOTION,
	GAMEPLAY_MOD_DIVING_ONLY,
	GAMEPLAY_MOD_DRUNK_AIM,
	GAMEPLAY_MOD_DRUNK_LOOK,
	GAMEPLAY_MOD_EASY,
	GAMEPLAY_MOD_EDIBLE_GIBS,
	GAMEPLAY_MOD_EMPTY_SLOWMOTION,
	GAMEPLAY_MOD_FADING_OUT,
	GAMEPLAY_MOD_FRICTION,
	GAMEPLAY_MOD_GARBAGE_GIBS,
	GAMEPLAY_MOD_GOD,
	GAMEPLAY_MOD_HARD,
	GAMEPLAY_MOD_HEADSHOTS,
	GAMEPLAY_MOD_HEAL_ON_KILL,
	GAMEPLAY_MOD_HEALTH_REGENERATION,
	GAMEPLAY_MOD_INFINITE_AMMO,
	GAMEPLAY_MOD_INFINITE_AMMO_CLIP,
	GAMEPLAY_MOD_INFINITE_PAINKILLERS,
	GAMEPLAY_MOD_INFINITE_SLOWMOTION,
	GAMEPLAY_MOD_INITIAL_CLIP_AMMO,
	GAMEPLAY_MOD_INSTAGIB,
	GAMEPLAY_MOD_NO_FALL_DAMAGE,
	GAMEPLAY_MOD_NO_HEALING,
	GAMEPLAY_MOD_NO_JUMPING,
	GAMEPLAY_MOD_NO_MAP_MUSIC,
	GAMEPLAY_MOD_NO_PILLS,
	GAMEPLAY_MOD_NO_SAVING,
	GAMEPLAY_MOD_NO_SECONDARY_ATTACK,
	GAMEPLAY_MOD_NO_SLOWMOTION,
	GAMEPLAY_MOD_NO_SMG_GRENADE_PICKUP,
	GAMEPLAY_MOD_NO_TARGET,
	GAMEPLAY_MOD_NO_WALKING,
	GAMEPLAY_MOD_ONE_HIT_KO,
	GAMEPLAY_MOD_ONE_HIT_KO_FROM_PLAYER,
	GAMEPLAY_MOD_PREVENT_MONSTER_DROPS,
	GAMEPLAY_MOD_PREVENT_MONSTER_MOVEMENT,
	GAMEPLAY_MOD_PREVENT_MONSTER_SPAWN,
	GAMEPLAY_MOD_PREVENT_MONSTER_STUCK_EFFECT,
	GAMEPLAY_MOD_RANDOM_GAMEPLAY_MODS,
	GAMEPLAY_MOD_SCORE_ATTACK,
	GAMEPLAY_MOD_SHOTGUN_AUTOMATIC,
	GAMEPLAY_MOD_SHOW_TIMER,
	GAMEPLAY_MOD_SHOW_TIMER_REAL_TIME,
	GAMEPLAY_MOD_SLOWMOTION_FAST_WALK,
	GAMEPLAY_MOD_SLOWMOTION_ON_DAMAGE,
	GAMEPLAY_MOD_SLOWMOTION_ONLY_DIVING,
	GAMEPLAY_MOD_SLOW_PAINKILLERS,
	GAMEPLAY_MOD_SNARK_FRIENDLY_TO_ALLIES,
	GAMEPLAY_MOD_SNARK_FRIENDLY_TO_PLAYER,
	GAMEPLAY_MOD_SNARK_FROM_EXPLOSION,
	GAMEPLAY_MOD_SNARK_INCEPTION,
	GAMEPLAY_MOD_SNARK_INFESTATION,
	GAMEPLAY_MOD_SNARK_NUCLEAR,
	GAMEPLAY_MOD_SNARK_PARANOIA,
	GAMEPLAY_MOD_SNARK_PENGUINS,
	GAMEPLAY_MOD_SNARK_STAY_ALIVE,
	GAMEPLAY_MOD_STARTING_HEALTH,
	GAMEPLAY_MOD_SUPERHOT,
	GAMEPLAY_MOD_SWEAR_ON_KILL,
	GAMEPLAY_MOD_UPSIDE_DOWN,
	GAMEPLAY_MOD_TELEPORT_MAINTAIN_VELOCITY,
	GAMEPLAY_MOD_TELEPORT_ON_KILL,
	GAMEPLAY_MOD_TIME_RESTRICTION,
	GAMEPLAY_MOD_TOTALLY_SPIES,
	GAMEPLAY_MOD_VVVVVV,
	GAMEPLAY_MOD_WEAPON_IMPACT,
	GAMEPLAY_MOD_WEAPON_PUSH_BACK,
	GAMEPLAY_MOD_WEAPON_RESTRICTED,

	GAMEPLAY_MOD_EVENT_GIVE_RANDOM_WEAPON,
	GAMEPLAY_MOD_EVENT_SPAWN_RANDOM_MONSTERS,
};

struct GameplayMod {
	std::string id;
	std::string name;
	std::string description;

	std::vector<Argument> arguments;

	bool cannotBeActivatedRandomly;
	bool canBeDeactivated;
	bool isEvent;
	bool canBeCancelledAfterChangeLevel;
	std::function<void( CBasePlayer * )> Deactivate;
	std::function<bool( CBasePlayer * )> CanBeActivatedRandomly;

	std::set<std::string> votes;

	GameplayMod() {};
	GameplayMod( const std::string &id, const std::string &name ) :
		id( id ), name( name ),
		description( "" ), arguments( std::vector<Argument>() ),

		cannotBeActivatedRandomly( false ),
		init( []( CBasePlayer *, const std::vector<Argument> & ) -> std::pair<std::string, std::string> {
			return { "", "" };
		} ),

		canBeDeactivated( false ),
		isEvent( false ),
		canBeCancelledAfterChangeLevel( false ),
		Deactivate( []( CBasePlayer * ) {} ),
		CanBeActivatedRandomly( []( CBasePlayer * ) { return true; } )
	{
	}

	std::pair<std::string, std::string> Init( CBasePlayer *player ) {
		return this->init( player, this->arguments );
	}

	void Init( CBasePlayer *player, const std::vector<std::string> &stringArgs ) {
		for ( size_t i = 0; i < min( this->arguments.size(), stringArgs.size() ); i++ ) {
			auto parsingError = this->arguments.at( i ).Init( stringArgs.at( i ) );

			if ( !parsingError.empty() ) {
				ALERT( at_notice, "%s, argument %d: %s", this->name.c_str(), i, parsingError.c_str() );
				return;
			}
		}

		this->init( player, this->arguments );
	}

	GameplayMod &Description( const std::string &description ) {
		this->description = description;
		return *this;
	}

	GameplayMod &Arguments( const std::vector<Argument> &args ) {
		this->arguments = args;
		return *this;
	}

	GameplayMod &Toggles( BOOL *flag ) {
		this->OnInit( [flag]( CBasePlayer *, const std::vector<Argument> & ) {
			*flag = TRUE;
		} );
		this->OnDeactivation( [flag]( CBasePlayer * ) {
			*flag = FALSE;
		} );
		return *this;
	}
	
	GameplayMod &Toggles( const std::vector<BOOL *> flags ) {
		this->OnInit( [flags]( CBasePlayer *, const std::vector<Argument> & ) {
			for ( auto flag : flags ) {
				*flag = TRUE;
			}
		} );
		this->OnDeactivation( [flags]( CBasePlayer * ) {
			for ( auto flag : flags ) {
				*flag = FALSE;
			}
		} );
		return *this;
	}

	GameplayMod &OnInit( const std::function<void( CBasePlayer*, const std::vector<Argument> & )> &init ) {
		this->init = [init]( CBasePlayer *player, const std::vector<Argument> &args ) -> std::pair<std::string, std::string> {
			init( player, args );
			return { "", "" };
		};
		return *this;
	}

	GameplayMod &OnEventInit( const std::function<std::pair<std::string, std::string>( CBasePlayer*, const std::vector<Argument> & )> &init ) {
		this->init = init;
		this->isEvent = true;
		return *this;
	}

	GameplayMod &OnDeactivation( const std::function<void( CBasePlayer* )> &deactivate ) {
		this->Deactivate = deactivate;
		this->canBeDeactivated = true;
		return *this;
	}

	GameplayMod &CannotBeActivatedRandomly() {
		this->cannotBeActivatedRandomly = true;
		return *this;
	}

	GameplayMod &CanOnlyBeActivatedRandomlyWhen( const std::function<bool( CBasePlayer* )> &randomActivateCondition ) {
		this->CanBeActivatedRandomly = randomActivateCondition;
		return *this;
	}

	GameplayMod &CanBeCancelledAfterChangeLevel() {
		this->canBeCancelledAfterChangeLevel = true;
		return *this;
	}

	private:
		std::function<std::pair<std::string, std::string>( CBasePlayer*, const std::vector<Argument> & )> init;
};

// ISSUE: Player instance still relies on itself in some cases, like when managin superhot or aimOffset
// No idea what should manage these, it's always possible to include Think function here for that too.

class GameplayMods
{
public:
	std::vector<TYPEDESCRIPTION> fields;
	std::vector<TYPEDESCRIPTION> gameplayFields;
	template<typename T> T _Field( int offset, char *fieldName, T default, FIELDTYPE type, bool isGameplayField ) {
		
		if ( isGameplayField ) {
			this->gameplayFields.push_back(
				{ type, fieldName, offset, 1, 0 }
			);
		} else {
			this->fields.push_back(
				{ type, fieldName, offset, 1, 0 }
			);
		}
		return default;
	}
#define Field( ctype, name, default, type, isGameplayField, ... ) ctype name = _Field( offsetof( GameplayMods, name ), #name, default, type, isGameplayField, ##__VA_ARGS__ )
#define FieldBool( name, default ) Field( BOOL, name, default, FIELD_BOOLEAN, false )
#define FieldInt( name, default ) Field( int, name, default, FIELD_INTEGER, false )
#define FieldFloat( name, default ) Field( float, name, default, FIELD_FLOAT, false )
#define FieldTime( name, default ) Field( float, name, default, FIELD_TIME, false )

#define GameplayFieldBool( name, default ) Field( BOOL, name, default, FIELD_BOOLEAN, true )
#define GameplayFieldInt( name, default ) Field( int, name, default, FIELD_INTEGER, true )
#define GameplayFieldFloat( name, default ) Field( float, name, default, FIELD_FLOAT, true )
#define GameplayFieldTime( name, default ) Field( float, name, default, FIELD_TIME, true )

	bool addedAdditionalFields = false;

	Field( GAME_MODE, activeGameMode, GAME_MODE_VANILLA, FIELD_INTEGER, false );
	char activeGameModeConfig[256];
	char activeGameModeConfigHash[128];

	GameplayFieldFloat( aimOffsetMaxX, 0.0f );
	GameplayFieldFloat( aimOffsetMaxY, 0.0f );
	GameplayFieldFloat( aimOffsetChangeFreqency, 0.0f );

	GameplayFieldBool( automaticShotgun, FALSE );

	GameplayFieldBool( bleeding, FALSE );
	GameplayFieldFloat( bleedingUpdatePeriod, 1.0f );
	GameplayFieldFloat( bleedingHandicap, 20.0f );
	GameplayFieldFloat( bleedingImmunityPeriod, 10.0f );

	GameplayFieldBool( bulletDelayOnSlowmotion, FALSE );
	GameplayFieldInt( bulletRicochetCount, 0 );
	GameplayFieldInt( bulletRicochetError, 5 );
	GameplayFieldFloat( bulletRicochetMaxDotProduct, 0.5 );
	GameplayFieldBool( bulletPhysical, FALSE );
	Field( BULLET_PHYSICS_MODE, bulletPhysicsMode, BULLET_PHYSICS_ENEMIES_ONLY_ON_SLOWMOTION, FIELD_INTEGER, true );
	GameplayFieldBool( bulletSelfHarm, FALSE );
	GameplayFieldBool( bulletTrailConstant, FALSE );

	GameplayFieldBool( crossbowExplosiveBolts, FALSE );

	GameplayFieldBool( detachableTripmines, FALSE );
	GameplayFieldBool( detachableTripminesInstantly, FALSE );

	GameplayFieldBool( divingOnly, FALSE );
	GameplayFieldBool( divingAllowedWithoutSlowmotion, FALSE );

	GameplayFieldInt( drunkiness, 0 );

	GameplayFieldBool( gibsAlways, FALSE );
	GameplayFieldBool( gibsEdible, FALSE );
	GameplayFieldBool( gibsGarbage, FALSE );

	GameplayFieldInt( fade, 255 );
	GameplayFieldBool( fadeOut, FALSE );
	GameplayFieldInt( fadeOutThreshold, 25 );
	GameplayFieldFloat( fadeOutUpdatePeriod, 0.5f );

	GameplayFieldFloat( frictionOverride, -1.0f );

	GameplayFieldBool( godConstant, FALSE );

	GameplayFieldBool( healOnKill, FALSE );
	GameplayFieldFloat( healOnKillMultiplier, 0.25f );
	
	GameplayFieldBool( infiniteAmmo, FALSE );
	GameplayFieldBool( infiniteAmmoClip, FALSE );
	GameplayFieldInt( initialClipAmmo, 0 );

	GameplayFieldBool( instaGib, FALSE );

	GameplayFieldBool( noFallDamage, FALSE );
	GameplayFieldBool( noJumping, FALSE );
	GameplayFieldBool( noMapMusic, FALSE );
	GameplayFieldBool( noHealing, FALSE );
	GameplayFieldBool( noSaving, FALSE );
	GameplayFieldBool( noSecondaryAttack, FALSE );
	GameplayFieldBool( noSmgGrenadePickup, FALSE );
	GameplayFieldBool( noTargetConstant, FALSE );
	GameplayFieldBool( noWalking, FALSE );

	GameplayFieldBool( oneHitKO, FALSE );
	GameplayFieldBool( oneHitKOFromPlayer, FALSE );

	GameplayFieldBool( preventMonsterDrops, FALSE );
	GameplayFieldBool( preventMonsterMovement, FALSE );
	GameplayFieldBool( preventMonsterSpawn, FALSE );
	GameplayFieldBool( preventMonsterStuckEffect, FALSE );

	GameplayFieldFloat( regenerationMax, 20.0f );
	GameplayFieldFloat( regenerationDelay, 3.0f );
	GameplayFieldFloat( regenerationFrequency, 0.2f );
	GameplayFieldBool( reverseGravity, FALSE );

	GameplayFieldBool( painkillersForbidden, FALSE );
	GameplayFieldBool( painkillersInfinite, FALSE );
	GameplayFieldBool( painkillersSlow, FALSE );

	GameplayFieldBool( slowmotionConstant, FALSE );
	GameplayFieldBool( slowmotionFastWalk, FALSE );
	GameplayFieldBool( slowmotionForbidden, FALSE );
	GameplayFieldBool( slowmotionInfinite, FALSE );
	GameplayFieldBool( slowmotionOnDamage, FALSE );
	GameplayFieldBool( slowmotionOnlyDiving, FALSE );

	GameplayFieldBool( snarkFriendlyToAllies, FALSE );
	GameplayFieldBool( snarkFriendlyToPlayer, FALSE );
	GameplayFieldBool( snarkFromExplosion, FALSE );
	GameplayFieldBool( snarkInception, FALSE );
	GameplayFieldInt( snarkInceptionDepth, 10 );
	GameplayFieldBool( snarkInfestation, FALSE );
	GameplayFieldBool( snarkNuclear, FALSE );
	GameplayFieldBool( snarkPenguins, FALSE );
	GameplayFieldBool( snarkStayAlive, FALSE );

	GameplayFieldBool( superHot, FALSE );
	GameplayFieldBool( swearOnKill, FALSE );

	GameplayFieldBool( teleportMaintainVelocity, FALSE );
	GameplayFieldBool( teleportOnKill, FALSE );
	char teleportOnKillWeapon[64];
	
	GameplayFieldBool( totallySpies, FALSE );

	GameplayFieldBool( upsideDown, FALSE );
	GameplayFieldBool( vvvvvv, FALSE );

	GameplayFieldFloat( weaponImpact, 0.0f );
	GameplayFieldBool( weaponPushBack, FALSE );
	GameplayFieldFloat( weaponPushBackMultiplier, 1.0f );
	GameplayFieldBool( weaponRestricted, FALSE );

	// Statistics and game state
	FieldBool( cheated, FALSE );
	FieldBool( usedCheat, FALSE );

	FieldBool( blackMesaMinute, FALSE );
	FieldBool( scoreAttack, FALSE );

	FieldBool( randomGameplayMods, FALSE );
	FieldFloat( timeForRandomGameplayMod, 10.0f );
	FieldFloat( timeUntilNextRandomGameplayMod, 10.0f );
	FieldFloat( timeForRandomGameplayModVoting, 10.0f );
	float timeLeftUntilNextRandomGameplayMod = 10.0f;
	std::vector<GameplayMod> proposedGameplayMods;
	
	FieldFloat( time, 0.0f );
	FieldFloat( realTime, 0.0f );
	FieldFloat( lastGlobalTime, 0.0f );
	FieldFloat( lastRealTime, 0.0f );

	FieldInt( score, 0 );
	FieldInt( comboMultiplier, 1 );
	FieldTime( comboMultiplierReset, 0.0f );
	
	FieldBool( timerShown, FALSE );
	FieldBool( timerBackwards, FALSE );
	FieldBool( timerPaused, FALSE );
	FieldBool( timerShowReal, FALSE );

	FieldInt( kills, 0 );
	FieldInt( headshotKills, 0 );
	FieldInt( explosiveKills, 0 );
	FieldInt( crowbarKills, 0 );
	FieldInt( projectileKills, 0 );
	FieldFloat( secondsInSlowmotion, 0.0f );

	void Init();

#ifndef CLIENT_DLL
	void SendToClient();
	int Save( CSave &save );
	int Restore( CRestore &restore );
	int RestoreMods( CRestore &restore );
#endif // CLIENT_DLL

	bool AllowedToVoteOnRandomGameplayMods();

	void AddArrayFieldDefinitions();
	void SetGameplayModActiveByString( const std::string &line, bool isActive = false );
	std::vector<GameplayMod> GetRandomGameplayMod( CBasePlayer *player, int modAmount, std::function<bool( const GameplayMod &mod )> filter = []( const GameplayMod & ){ return true; } );
	GameplayMod GetRandomGameplayMod( CBasePlayer *player );

	std::vector<std::pair<GameplayMod, float>> timedGameplayMods;

	static void Reset();
};

extern GameplayMods gameplayMods;
extern std::map<GAMEPLAY_MOD, GameplayMod> gameplayModDefs;

#ifndef CLIENT_DLL
extern TYPEDESCRIPTION gameplayModsSaveData[];
#endif // !CLIENT_DLL

#endif // GAMEPLAY_MOD_H