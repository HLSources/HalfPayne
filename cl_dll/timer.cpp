#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "com_weapons.h"
#include "triangleapi.h"

DECLARE_MESSAGE( m_Timer, TimerValue )
DECLARE_MESSAGE( m_Timer, TimerMsg )

int CHudTimer::Init(void)
{
	HOOK_MESSAGE( TimerValue );
	HOOK_MESSAGE( TimerMsg );

	m_iFlags |= HUD_ACTIVE;
	
	time = 0.0f;
	m_iFlags = 0;
	
	gHUD.AddHudElem(this);

	return 1;
}

int CHudTimer::VidInit( void )
{
	return 1;
}

int CHudTimer::Draw( float flTime )
{
	int r = 200, g = 200, b = 200;

	if ( gHUD.m_iHideHUDDisplay || gEngfuncs.IsSpectateOnly( ) || CL_IsDead() )
		return 1;

	int x = ScreenWidth - UPPER_RIGHT_CORNER_OFFSET;
	int y = UPPER_RIGHT_CORNER_OFFSET;

	DrawFormattedTime( x - gHUD.GetNumberSpriteWidth( ) * 8, y, r, g, b );

	y += 24;
	DrawMessages( x, y, r, g, b );

	return 1;
}

// Format is mm:ss.msec | 123:45.678
void CHudTimer::DrawFormattedTime( int x, int y, int r, int g, int b )
{
	float minutes = time / 60.0f;
	int actualMinutes = floor( minutes );
	float seconds = fmod( time, 60.0f );
	float secondsIntegral;
	float secondsFractional = modf( seconds, &secondsIntegral );

	int actualSeconds = secondsIntegral;
	int actualMilliseconds = secondsFractional * 100;

	const int NUMBER_WIDTH = gHUD.GetNumberSpriteWidth();

	if ( actualMinutes < 10 ) {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, 0, r, g, b );
		x += NUMBER_WIDTH;
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualMinutes, r, g, b );
		x += NUMBER_WIDTH;
	}
	else if ( actualMinutes >= 100 ) {
		x -= NUMBER_WIDTH;
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualMinutes, r, g, b );
		x += NUMBER_WIDTH * 3;
	} else {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualMinutes, r, g, b );
		x += NUMBER_WIDTH * 2;
	}

	gHUD.DrawColon( x, y, r, g, b );
	x += NUMBER_WIDTH;

	if ( actualSeconds < 10 ) {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, 0, r, g, b );
		x += NUMBER_WIDTH;
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualSeconds, r, g, b );
		x += NUMBER_WIDTH;
	}
	else {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualSeconds, r, g, b );
		x += NUMBER_WIDTH * 2;
	}

	gHUD.DrawDecimalSeparator( x, y, r, g, b );
	x += NUMBER_WIDTH;

	if ( actualMilliseconds < 10 ) {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, 0, r, g, b );
		x += NUMBER_WIDTH;
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualMilliseconds, r, g, b );
	}
	else {
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, actualMilliseconds, r, g, b );
	}
}

void CHudTimer::DrawMessages( int x, int y, int r, int g, int b )
{
	for ( int i = messages.size() - 1; i >= 0; i-- ) {
		CHudTimerMessage timerMessage = messages.at( i );

		if ( fabs( timerMessage.timerMessageRemovalTime - gHUD.m_flTime ) >= TIMER_MESSAGE_REMOVAL_TIME ) {
			messages.erase( messages.begin() + i );
		}
	}

	for ( int i = messages.size( ) - 1; i >= 0; i-- ) {
		CHudTimerMessage timerMessage = messages.at( i );

		char* message = const_cast<char*>( timerMessage.message.c_str() );
		gHUD.DrawHudStringKeepRight( x, y, 200, message, r, g, b );

		y += gHUD.m_scrinfo.iCharHeight - 2;

		float timePassed = TIME_ADDED_REMOVAL_TIME - max( 0.0f, timerMessage.timeAddedRemovalTime - gHUD.m_flTime );
		int alpha = 200 - ( ( timePassed - TIME_ADDED_FLASHING_TIME * 2 ) / ( TIME_ADDED_REMOVAL_TIME - TIME_ADDED_FLASHING_TIME * 2 ) ) * 200;


		if ( ( timePassed >= timerMessage.timeAddedFlash1Time && timePassed <= timerMessage.timeAddedFlash2Time ) 
			|| ( timePassed >= TIME_ADDED_REMOVAL_TIME ) ) {
			continue;
		}
		
		Vector screen;
		if ( gEngfuncs.pTriAPI->WorldToScreen( timerMessage.coords, screen ) ) {
			// Don't draw if offscreen
			continue;
		}

		// fade out
		if ( timePassed >= timerMessage.timeAddedFlash2Time ) {
			ScaleColors( r, g, b, alpha );
		}

		char* timeAdded = const_cast<char*>( timerMessage.timeAddedString.c_str() );
		gHUD.DrawHudStringKeepCenter( XPROJECT( screen.x ), YPROJECT( screen.y ), 200, timeAdded, r, g, b );
	}
}

int CHudTimer::MsgFunc_TimerValue( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ(pbuf, iSize);
	time = READ_FLOAT();

	m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudTimer::MsgFunc_TimerMsg( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const char* message = READ_STRING();
	int timeAdded = READ_LONG();
	float coordsX = READ_COORD();
	float coordsY = READ_COORD();
	float coordsZ = READ_COORD();

	Vector coords( coordsX, coordsY, coordsZ );
	messages.push_back( CHudTimerMessage( std::string( message ), timeAdded, coords, gHUD.m_flTime ) );

	m_iFlags |= HUD_ACTIVE;

	return 1;
}