﻿#include "wrect.h"
#include "cl_dll.h"
#include "FontAwesome.h"

#include "gamemode_gui.h"

extern SDL_Window *window;

std::vector< CustomGameModeConfig > cgmConfigs;
std::vector< CustomGameModeConfig > bmmConfigs;
std::vector< CustomGameModeConfig > sagmConfigs;

int configAmount;
int configCompleted;
float configCompletedPercent;

int selectedGamemodeTab = CONFIG_TYPE_CGM;

void GameModeGUI_Init() {
	GameModeGUI_RefreshConfigFiles();
}

int GetAmountOfCompletedConfigs( const std::vector< CustomGameModeConfig > &configs ) {
	int result = 0;
	for ( const auto &config : configs ) {
		if ( config.gameFinishedOnce ) {
			result++;
		}
	}

	return result;
}

void GameModeGUI_RefreshConfigFiles() {
	GameModeGUI_RefreshConfigFileList( CONFIG_TYPE_CGM );
	GameModeGUI_RefreshConfigFileList( CONFIG_TYPE_BMM );
	GameModeGUI_RefreshConfigFileList( CONFIG_TYPE_SAGM );

	configAmount = cgmConfigs.size() + bmmConfigs.size() + sagmConfigs.size();
	configCompleted = GetAmountOfCompletedConfigs( cgmConfigs ) + GetAmountOfCompletedConfigs( bmmConfigs ) + GetAmountOfCompletedConfigs( sagmConfigs );
	configCompletedPercent = ( ( ( float ) configCompleted ) / configAmount );
}

void GameModeGUI_RefreshConfigFileList( CONFIG_TYPE configType ) {
	std::vector< CustomGameModeConfig > *configs = GameModeGUI_GameModeConfigVectorFromType( configType );
	configs->clear();

	CustomGameModeConfig dumbConfig = CustomGameModeConfig( configType );
	std::vector<std::string> files = dumbConfig.GetAllConfigFileNames();

	for ( const auto &file : files ) {
		CustomGameModeConfig config( configType );
		config.ReadFile( file.c_str() );

		configs->push_back( config );
	}
}

void GameModeGUI_RunCustomGameMode( CustomGameModeConfig &config ) {
	if ( config.error.length() > 0 ) {
		return;
	}

	// Prepare game mode and try to launch [start_map]
	// Launching the map then will execute CustomGameModeConfig constructor on server,
	// where the file will be parsed again.
	gEngfuncs.Cvar_Set( "gamemode_config", ( char * ) config.configName.c_str() );
	gEngfuncs.Cvar_Set( "gamemode", ( char * ) CustomGameModeConfig::ConfigTypeToGameModeCommand( config.configType ).c_str() );

	const std::string startMap = config.GetStartMap();
	char mapCmd[64];
	sprintf( mapCmd, "map %s", startMap.c_str() );
	gEngfuncs.pfnClientCmd( mapCmd );
}

void GameModeGUI_SelectableButton( bool isSelected ) {
	ImGui::PushStyleColor( ImGuiCol_Button, isSelected ? ImVec4( 0.4f, 0.4f, 1.0f, 1.0f ) : ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, isSelected ? ImVec4( 0.6f, 0.6f, 1.0f, 1.0f ) : ImVec4( 1.0f, 0.6f, 0.6f, 1.0f ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, isSelected ? ImVec4( 0.3f, 0.3f, 1.0f, 1.0f ) : ImVec4( 1.0f, 0.3f, 0.3f, 1.0f ) );
}

std::vector<CustomGameModeConfig> *GameModeGUI_GameModeConfigVectorFromType( CONFIG_TYPE configType ) {
	switch ( configType ) {
		case CONFIG_TYPE_CGM:
			return &cgmConfigs;

		case CONFIG_TYPE_BMM:
			return &bmmConfigs;

		case CONFIG_TYPE_SAGM:
			return &sagmConfigs;
	}
}

void GameModeGUI_DrawMainWindow() {

	//static bool showTestWindow = false;
	//ImGui::SetNextWindowPos( ImVec2( 0, 0 ), ImGuiSetCond_FirstUseEver );
	//ImGui::ShowTestWindow( &showTestWindow );

	static bool showGameModeWindow = false;
	const int GAME_MODE_WINDOW_WIDTH  = 570;
	const int GAME_MODE_WINDOW_HEIGHT = 330;
	int RENDERED_WIDTH;
	SDL_GetWindowSize( window, &RENDERED_WIDTH, NULL );
	ImGui::SetNextWindowPos( ImVec2( RENDERED_WIDTH - GAME_MODE_WINDOW_WIDTH, 0 ), ImGuiSetCond_FirstUseEver );
	ImGui::SetNextWindowSize( ImVec2( GAME_MODE_WINDOW_WIDTH, GAME_MODE_WINDOW_HEIGHT ), ImGuiSetCond_FirstUseEver );
	ImGui::Begin( "Custom Game Modes", &showGameModeWindow );

	
	{
		// PROGRESSION
		char progressLabel[64];
		sprintf( progressLabel, "Completed %d/%d (%.1f%%)", configCompleted, configAmount, configCompletedPercent * 100 );
		ImGui::ProgressBar( configCompletedPercent, ImVec2( -100.0f, 0.0f ), progressLabel );

		ImGui::SameLine();

		// REFRESH BUTTON
		if ( ImGui::Button( "Refresh", ImVec2( -1, 0 ) ) ) {
			GameModeGUI_RefreshConfigFiles();
		}
	}

	// TABLES
	{
		ImGui::BeginChild( "gamemode_scrollable_data_child", ImVec2( -1, -20 ) );
		if ( ImGui::CollapsingHeader( "Custom Game Modes" ) ) {
			GameModeGUI_DrawGamemodeConfigTable( CONFIG_TYPE_CGM );
			ImGui::Columns( 1 );
		}

		if ( ImGui::CollapsingHeader( "Black Mesa Minute" ) ) {
			GameModeGUI_DrawGamemodeConfigTable( CONFIG_TYPE_BMM );
			ImGui::Columns( 1 );
		}

		if ( ImGui::CollapsingHeader( "Score Attack" ) ) {
			GameModeGUI_DrawGamemodeConfigTable( CONFIG_TYPE_SAGM );
			ImGui::Columns( 1 );
		}

		ImGui::EndChild();
	}

	const char *launchHint = "Check out half_payne/README_GAME_MODES.txt for customization\n";
	ImGui::SetCursorPosX( ImGui::GetWindowWidth() / 2.0f - ImGui::CalcTextSize( launchHint ).x / 2.0f );
	ImGui::Text( launchHint );

	ImGui::End();
}

void GameModeGUI_DrawGamemodeConfigTable( CONFIG_TYPE configType ) {
	
	const std::vector<CustomGameModeConfig> *configs = GameModeGUI_GameModeConfigVectorFromType( configType );

	{
		ImGui::Columns( 1, "gamemode_hint_column" );

		const char *launchHint = "Double click on the row to launch game mode\n";
		ImGui::SetCursorPosX( ImGui::GetWindowWidth() / 2.0f - ImGui::CalcTextSize( launchHint ).x / 2.0f );
		ImGui::Text( launchHint );

		ImGui::NextColumn();
	}

	// TABLE HEADERS
	{
		ImGui::Columns( 3, "gamemode_header_columns" );
		ImGui::TextColored( ImVec4( 1.0, 1.0, 1.0, 0.0 ), "%s", ICON_FA_CHECK ); ImGui::SameLine(); ImGui::Text( "File" ); ImGui::NextColumn();
		ImGui::Text( "Name" ); ImGui::NextColumn();
		ImGui::Text( "" ); ImGui::NextColumn();
		ImGui::SetColumnOffset( 2, ImGui::GetWindowWidth() - 70 );
		ImGui::Separator();
	}

	// TABLE ITSELF
	{

		for ( int i = 0; i < configs->size(); i++ ) {

			CustomGameModeConfig config = configs->at( i );

			const char *file = config.configName.c_str();
			const std::string name = config.GetName();
			
			// COMPLETED?
			ImGui::TextColored( ImVec4( 1.0, 1.0, 1.0, config.gameFinishedOnce ? 1.0 : 0.0 ), "%s", ICON_FA_CHECK );
			if (
				config.gameFinishedOnce &&
				( config.configType == CONFIG_TYPE_BMM || config.configType == CONFIG_TYPE_SAGM ) &&
				ImGui::IsItemHovered()
			) {
				ImGui::BeginTooltip();

				if ( config.configType == CONFIG_TYPE_BMM ) {
					ImGui::Text( "Time score: %s", GameModeGUI_GetFormattedTime( config.record.time ).c_str() );
					ImGui::Text( "Real time: %s", GameModeGUI_GetFormattedTime( config.record.realTime ).c_str() );
					ImGui::Text( "Real time minus score: %s", GameModeGUI_GetFormattedTime( config.record.realTimeMinusTime ).c_str() );
				} else if ( config.configType == CONFIG_TYPE_SAGM ) {
					ImGui::Text( "Score: %d", config.record.score );
				}

				ImGui::EndTooltip();
			}
			ImGui::SameLine();

			// FILE NAME + ROW
			ImGui::Selectable( file, false, ImGuiSelectableFlags_SpanAllColumns );
			ImGui::SetItemAllowOverlap();
			if ( config.error.length() > 0 ) {
				ImGui::SameLine();
				ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "error" );
			}
			if ( ImGui::IsItemHovered() ) {
				if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
					GameModeGUI_RunCustomGameMode( config );
				}
			}
			ImGui::NextColumn();

			// CONFIG NAME
			ImGui::Text( "%s", name.c_str() ); ImGui::NextColumn();

			// CONFIG FILE INFO
			{
				ImGui::Text( "%s", ICON_FA_INFO_CIRCLE );
				if ( ImGui::IsItemHovered() ) {
					ImGui::BeginTooltip();

					GameModeGUI_DrawConfigFileInfo( config );

					ImGui::EndTooltip();
				}

				const std::string modalKey = name.length() > 0 ? name : std::string( file ) + std::to_string( i );
				if ( ImGui::IsItemClicked() ) {
					ImGui::OpenPopup( modalKey.c_str() );
				}
				if ( ImGui::BeginPopupModal( modalKey.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
					GameModeGUI_DrawConfigFileInfo( config );

					if ( ImGui::Button( "Close", ImVec2( -1, 0 ) ) ) {
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}

			// LAUNCH BUTTON
			ImGui::SameLine();
			{
				ImGui::Text( "%s", ICON_FA_ARROW_RIGHT );
				if ( ImGui::IsItemHovered() ) {

					ImGui::BeginTooltip();

					ImGui::Text( "Launch" );
					
					ImGui::EndTooltip();
				}
				if ( ImGui::IsItemClicked() ) {
					GameModeGUI_RunCustomGameMode( config );
				}
			}

			ImGui::NextColumn();

		}
	}
}

void GameModeGUI_DrawConfigFileInfo( CustomGameModeConfig &config ) {
	const std::string description = config.GetDescription();
	const std::string startMap = config.GetStartMap();
	
	if ( config.error.length() > 0 ) {
		ImGui::PushTextWrapPos(300.0f);
		ImGui::Text( config.error.c_str() );
		ImGui::PopTextWrapPos();
	} else {
		ImGui::Text( "Start map                   " );
		ImGui::SameLine();

		const std::string sha1 = config.sha1.substr( 0, 7 );
		ImGui::SetCursorPosX( ImGui::GetWindowWidth() - ImGui::CalcTextSize( sha1.c_str() ).x - 12 );
		ImGui::Text( sha1.c_str() );

		ImGui::Text( startMap.c_str() );

		if ( description.size() > 0 ) {
			ImGui::Text( "\nDescription\n" );
			ImGui::Text( description.c_str() );
		}

		if ( config.configType == CONFIG_TYPE_BMM ) {
			ImGui::TextColored( ImVec4( 1, 0.66, 0, 1 ), "\n\nBlack Mesa Minute\n" );
			ImGui::Text( "Time-based game mode - rush against a minute, kill enemies to get more time.\n" );
		} else if ( config.configType == CONFIG_TYPE_SAGM ) {
			ImGui::TextColored( ImVec4( 1, 0.66, 0, 1 ), "\n\nScore Attack\n" );
			ImGui::Text( "Kill enemies to get as much score as possible. Build combos to get even more score.\n" );
		}

		for ( const GameplayMod &mod : config.mods ) {
			ImGui::TextColored( ImVec4( 1, 0.66, 0, 1 ), ( "\n" + mod.name + "\n" ).c_str() );
			ImGui::Text( mod.description.c_str() );
			for ( const auto &argDescription : mod.argDescriptions ) {
				ImGui::TextColored( ImVec4( 1, 0.66, 0, 1 ), "   %s", ICON_FA_WRENCH ); ImGui::SameLine();
				ImGui::Text( argDescription.c_str() );
			}
		}

	}
}

const std::string GameModeGUI_GetFormattedTime( float time ) {
	float minutes = time / 60.0f;
	int actualMinutes = floor( minutes );
	float seconds = fmod( time, 60.0f );
	float secondsIntegral;
	float secondsFractional = modf( seconds, &secondsIntegral );

	int actualSeconds = secondsIntegral;
	int actualMilliseconds = secondsFractional * 100;

	std::string result;

	if ( actualMinutes < 10 ) {
		result += "0";
	}
	result += std::to_string( actualMinutes ) + ":";

	if ( actualSeconds < 10 ) {
		result += "0";
	}
	result += std::to_string( actualSeconds ) + ".";

	if ( actualMilliseconds < 10 ) {
		result += "0";
	}
	result += std::to_string( actualMilliseconds );

	return result;
}