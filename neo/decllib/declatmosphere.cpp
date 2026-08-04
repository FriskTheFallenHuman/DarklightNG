
/*
===========================================================================

	ETQW atmosphere declaration, adapted to Darklight.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "declAtmosphere.h"
#include "../framework/DeclFX.h"
#include "../renderer/Image.h"
#include "../renderer/ModelManager.h"

void sdPrecipitationParameters::Default() {
	material = declManager ? declManager->FindMaterial( "_default", true ) : NULL;
	model = NULL;
	effect = NULL;
	timeMin = 0.0f;
	timeMax = 500.0f;
	precipitationDistance = 1000.0f;
	switch ( preType ) {
	case PT_RAIN:
		maxParticles = 4000; heightMin = 50.0f; heightMax = 250.0f; weightMin = 1.5f; weightMax = 2.5f;
		windScale = 70.0f; gustWindScale = 100.0f; fallMin = 700.0f; fallMax = 900.0f; tumbleStrength = 0.0f;
		break;
	case PT_SNOW:
		maxParticles = 4000; heightMin = 3.0f; heightMax = 5.0f; weightMin = 1.5f; weightMax = 2.5f;
		windScale = 36.0f; gustWindScale = 40.0f; fallMin = 55.0f; fallMax = 105.0f; tumbleStrength = 24.0f;
		break;
	case PT_SPLASH:
		maxParticles = 4000; heightMin = 2.0f; heightMax = 3.0f; weightMin = 15.0f; weightMax = 20.0f;
		windScale = gustWindScale = 0.0f; fallMin = 5.0f; fallMax = 10.0f; tumbleStrength = 0.0f;
		break;
	default:
		preType = PT_NONE; maxParticles = 0; heightMin = heightMax = weightMin = weightMax = 0.0f;
		windScale = gustWindScale = fallMin = fallMax = tumbleStrength = 0.0f;
		break;
	}
}

bool sdPrecipitationParameters::Parse( idLexer &src ) {
	idToken token;
	Default();
	if ( !src.ExpectTokenString( "{" ) ) return false;
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) return true;
		if ( !token.Icmp( "type" ) ) preType = static_cast<precipitationType_e>( src.ParseInt() );
		else if ( !token.Icmp( "maxParticles" ) ) maxParticles = src.ParseInt();
		else if ( !token.Icmp( "heightMin" ) ) heightMin = src.ParseFloat();
		else if ( !token.Icmp( "heightMax" ) ) heightMax = src.ParseFloat();
		else if ( !token.Icmp( "weightMin" ) ) weightMin = src.ParseFloat();
		else if ( !token.Icmp( "weightMax" ) ) weightMax = src.ParseFloat();
		else if ( !token.Icmp( "timeMin" ) ) timeMin = src.ParseFloat();
		else if ( !token.Icmp( "timeMax" ) ) timeMax = src.ParseFloat();
		else if ( !token.Icmp( "windScale" ) ) windScale = src.ParseFloat();
		else if ( !token.Icmp( "gustWindScale" ) ) gustWindScale = src.ParseFloat();
		else if ( !token.Icmp( "fallMin" ) ) fallMin = src.ParseFloat();
		else if ( !token.Icmp( "fallMax" ) ) fallMax = src.ParseFloat();
		else if ( !token.Icmp( "tumbleStrength" ) ) tumbleStrength = src.ParseFloat();
		else if ( !token.Icmp( "precipitationDistance" ) ) precipitationDistance = src.ParseFloat();
		else if ( !token.Icmp( "material" ) ) { src.ReadToken( &token ); material = declManager->FindMaterial( token, false ); }
		else if ( !token.Icmp( "effect" ) ) { src.ReadToken( &token ); effect = static_cast<const idDeclFX *>( declManager->FindType( DECL_FX, token, false ) ); }
		else if ( !token.Icmp( "model" ) ) { src.ReadToken( &token ); model = renderModelManager->FindModel( token ); }
		else { src.Warning( "sdPrecipitationParameters::Parse: unknown token '%s'", token.c_str() ); return false; }
	}
	return false;
}

void sdPrecipitationParameters::Save( idFile_Memory &file ) const {
	file.WriteFloatString( "\tprecipitation {\n\t\ttype %i\n\t\tmaxParticles %i\n", preType, maxParticles );
	file.WriteFloatString( "\t\theightMin %f\n\t\theightMax %f\n", heightMin, heightMax );
	file.WriteFloatString( "\t\tweightMin %f\n\t\tweightMax %f\n", weightMin, weightMax );
	file.WriteFloatString( "\t\twindScale %f\n\t\tgustWindScale %f\n", windScale, gustWindScale );
	file.WriteFloatString( "\t\tfallMin %f\n\t\tfallMax %f\n", fallMin, fallMax );
	file.WriteFloatString( "\t\ttimeMin %f\n\t\ttimeMax %f\n", timeMin, timeMax );
	file.WriteFloatString( "\t\ttumbleStrength %f\n\t\tprecipitationDistance %f\n\t}\n", tumbleStrength, precipitationDistance );
}

sdDeclAtmosphere::sdDeclAtmosphere() { FreeData(); }

const char *sdDeclAtmosphere::DefaultDefinition() const { return "{}"; }

void sdDeclAtmosphere::FreeData() {
	modified = false;
	sunMaterial = declManager ? declManager->FindMaterial( "atmospheres/lights/default", true ) : NULL;
	sunDir.Zero(); sunAzimuth = sunZenith = 0.0f; sunColor.Zero(); sunHaloScale = 0.4f; sunHaloBias = 0.0f;
	sunSpriteMaterial = declManager ? declManager->FindMaterial( "atmospheres/sprites/sundisc", true ) : NULL;
	sunSpriteSize = 12600.0f;
	sunFlareMaterial = declManager ? declManager->FindMaterial( "atmospheres/sprites/sundisk_flare", true ) : NULL;
	sunFlareSize = sunFlareTime = sunFlareAzi = sunFlareZen = 0.0f; enableSunFlareAziZen = false;
	defaultPostProcessParms.tint.Set( 1.0f, 1.0f, 1.0f );
	defaultPostProcessParms.saturation = defaultPostProcessParms.contrast = 1.0f;
	defaultPostProcessParms.glareParms.Set( 0.84f, 1.0f, 0.0f, 1.0f );
	defaultPostProcessParms.glareBases.Set( 0.3f, 0.3f, 0.0f, 1.0f );
	postProcessParms = defaultPostProcessParms;
	fogDistHalf = 8000.0f; fogHeightHalf = 400.0f; fogHeightOffset = 1000.0f; fogColor.Zero();
	fogStart = 30000.0f; fogEnd = 40000.0f;
	atmosphereMaterial = declManager ? declManager->FindMaterial( "atmospheres/default", true ) : NULL;
	ambientCubeMap = NULL; skyGradientImage = globalImages ? globalImages->whiteImage : NULL;
	farClip = 0.0f; isNight = false; drawAtmosphereLast = true; minSpecShadowColor.Set( 0.75f, 0.75f, 0.75f );
	windAngle = 0.0f; windAngleDev = 10.0f; windStrength = 100.0f; windStrengthDev = 20.0f;
	cloudLayers.Clear(); numPrecipLayers = 0;
	for ( int i = 0; i < NUM_PRECIP_LAYERS; i++ ) { precipitation[i].preType = sdPrecipitationParameters::PT_NONE; precipitation[i].Default(); }
}

void sdDeclAtmosphere::CacheFromDict( const idDict &dict ) {
	const idKeyValue *keyValue = NULL;
	while ( ( keyValue = dict.MatchPrefix( "atmosphere", keyValue ) ) != NULL ) {
		if ( keyValue->GetValue().Length() ) declManager->FindType( DECL_ATMOSPHERE, keyValue->GetValue(), false );
	}
}

bool sdDeclAtmosphere::ParsePostProcessParms( idLexer &src ) {
	idToken token;
	if ( !src.ExpectTokenString( "{" ) ) return false;
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) { postProcessParms = defaultPostProcessParms; return true; }
		if ( !token.Icmp( "tint" ) ) src.Parse1DMatrix( 3, defaultPostProcessParms.tint.ToFloatPtr() );
		else if ( !token.Icmp( "saturation" ) ) defaultPostProcessParms.saturation = src.ParseFloat();
		else if ( !token.Icmp( "contrast" ) ) defaultPostProcessParms.contrast = src.ParseFloat();
		else if ( !token.Icmp( "glareParms" ) ) src.Parse1DMatrix( 4, defaultPostProcessParms.glareParms.ToFloatPtr() );
		else if ( !token.Icmp( "glareBases" ) ) src.Parse1DMatrix( 4, defaultPostProcessParms.glareBases.ToFloatPtr() );
		else { src.Warning( "sdDeclAtmosphere::ParsePostProcessParms: unknown token '%s'", token.c_str() ); return false; }
	}
	return false;
}

bool sdDeclAtmosphere::ParseCloudLayer( idLexer &src ) {
	idToken token;
	if ( !src.ReadToken( &token ) ) return false;
	sdCloudLayer layer;
	layer.material = declManager->FindMaterial( token, true );
	if ( !src.ExpectTokenString( "{" ) ) return false;
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			if ( cloudLayers.Num() >= MAX_CLOUD_LAYERS ) { src.Warning( "too many cloud layers" ); return false; }
			cloudLayers.Append( layer ); return true;
		}
		if ( !token.Icmp( "style" ) ) {
			src.ReadToken( &token );
			if ( !token.Icmp( "old" ) ) layer.style = 0;
			else if ( !token.Icmp( "skybox" ) ) layer.style = 1;
			else { src.Warning( "unknown cloud style '%s'", token.c_str() ); return false; }
		} else if ( !token.Icmp( "parms" ) ) {
			const int count = idMath::ClampInt( 0, NUM_CLOUD_LAYER_PARAMETERS, src.ParseInt() );
			if ( count && !src.Parse1DMatrix( count, layer.parms ) ) return false;
		} else { src.Warning( "sdDeclAtmosphere::ParseCloudLayer: unknown token '%s'", token.c_str() ); return false; }
	}
	return false;
}

bool sdDeclAtmosphere::ParsePrecipitationLayer( idLexer &src ) {
	if ( numPrecipLayers >= NUM_PRECIP_LAYERS ) return false;
	if ( !precipitation[numPrecipLayers].Parse( src ) ) return false;
	numPrecipLayers++;
	return true;
}

bool sdDeclAtmosphere::Parse( const char *text, const int textLength ) {
	idLexer src;
	idToken token;
	src.LoadMemory( text, textLength, GetFileName(), GetLineNum() );
	src.SetFlags( DECL_LEXER_FLAGS );
	src.SkipUntilString( "{" );
	FreeData();
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			if ( ambientCubeMap ) const_cast<sdDeclAmbientCubeMap *>( ambientCubeMap )->SetSunParameters( sunDir, sunColor );
			modified = false; return true;
		}
		if ( !token.Icmp( "sunMaterial" ) ) { src.ReadToken( &token ); sunMaterial = declManager->FindMaterial( token, true ); }
		else if ( !token.Icmp( "sunDirection" ) || !token.Icmp( "sunDir" ) ) src.Parse1DMatrix( 3, sunDir.ToFloatPtr() );
		else if ( !token.Icmp( "sunAzimuth" ) ) { sunAzimuth = src.ParseFloat(); UpdateSunDirFromAziZen(); }
		else if ( !token.Icmp( "sunZenith" ) ) { sunZenith = src.ParseFloat(); UpdateSunDirFromAziZen(); }
		else if ( !token.Icmp( "sunColor" ) ) src.Parse1DMatrix( 3, sunColor.ToFloatPtr() );
		else if ( !token.Icmp( "sunHaloScale" ) ) sunHaloScale = src.ParseFloat();
		else if ( !token.Icmp( "sunHaloBias" ) ) sunHaloBias = src.ParseFloat();
		else if ( !token.Icmp( "sunSpriteMaterial" ) ) { src.ReadToken( &token ); sunSpriteMaterial = declManager->FindMaterial( token, true ); }
		else if ( !token.Icmp( "sunSpriteSize" ) ) sunSpriteSize = src.ParseFloat();
		else if ( !token.Icmp( "sunFlareMaterial" ) ) { src.ReadToken( &token ); sunFlareMaterial = declManager->FindMaterial( token, true ); }
		else if ( !token.Icmp( "sunFlareSize" ) ) sunFlareSize = src.ParseFloat();
		else if ( !token.Icmp( "sunFlareTime" ) ) sunFlareTime = src.ParseFloat();
		else if ( !token.Icmp( "enableSunFlareAziZen" ) ) enableSunFlareAziZen = src.ParseBool();
		else if ( !token.Icmp( "sunFlareAzi" ) ) sunFlareAzi = src.ParseFloat();
		else if ( !token.Icmp( "sunFlareZen" ) ) sunFlareZen = src.ParseFloat();
		else if ( !token.Icmp( "postProcess" ) || !token.Icmp( "postProcessParms" ) ) { if ( !ParsePostProcessParms( src ) ) return false; }
		else if ( !token.Icmp( "fogDistHalf" ) ) fogDistHalf = src.ParseFloat();
		else if ( !token.Icmp( "fogHeightHalf" ) ) fogHeightHalf = src.ParseFloat();
		else if ( !token.Icmp( "fogHeightOffset" ) ) fogHeightOffset = src.ParseFloat();
		else if ( !token.Icmp( "fogColor" ) ) src.Parse1DMatrix( 3, fogColor.ToFloatPtr() );
		else if ( !token.Icmp( "fogStart" ) ) fogStart = src.ParseFloat();
		else if ( !token.Icmp( "fogEnd" ) ) fogEnd = src.ParseFloat();
		else if ( !token.Icmp( "atmosphereMaterial" ) ) { src.ReadToken( &token ); atmosphereMaterial = declManager->FindMaterial( token, true ); }
		else if ( !token.Icmp( "ambientCubeMap" ) ) { src.ReadToken( &token ); ambientCubeMap = R_FindAmbientCubeMap( token, true ); }
		else if ( !token.Icmp( "skyGradientImage" ) ) { src.ReadToken( &token ); SetSkyGradientImage( token ); }
		else if ( !token.Icmp( "farClip" ) ) farClip = src.ParseFloat();
		else if ( !token.Icmp( "isNight" ) ) isNight = src.ParseBool();
		else if ( !token.Icmp( "drawAtmosphereLast" ) ) drawAtmosphereLast = src.ParseBool();
		else if ( !token.Icmp( "minSpecShadowColor" ) ) src.Parse1DMatrix( 3, minSpecShadowColor.ToFloatPtr() );
		else if ( !token.Icmp( "cloudLayer" ) ) { if ( !ParseCloudLayer( src ) ) return false; }
		else if ( !token.Icmp( "precipitation" ) ) { if ( !ParsePrecipitationLayer( src ) ) return false; }
		else if ( !token.Icmp( "windAngle" ) ) windAngle = src.ParseFloat();
		else if ( !token.Icmp( "windAngleDev" ) ) windAngleDev = src.ParseFloat();
		else if ( !token.Icmp( "windStrength" ) ) windStrength = src.ParseFloat();
		else if ( !token.Icmp( "windStrengthDev" ) ) windStrengthDev = src.ParseFloat();
		else { src.Warning( "sdDeclAtmosphere::Parse: unknown token '%s'", token.c_str() ); return false; }
	}
	return false;
}

bool sdDeclAtmosphere::SetSkyGradientImage( const char *imageName ) {
	if ( !globalImages || !imageName || !imageName[0] ) { skyGradientImage = NULL; return false; }
	skyGradientImage = globalImages->ImageFromFile( imageName, TF_LINEAR, true, TR_REPEAT, TD_HIGH_QUALITY, CF_2D );
	modified = true;
	return skyGradientImage != NULL;
}

void sdDeclAtmosphere::UpdateSunDirFromAziZen() {
	sunDir.x = idMath::Cos( DEG2RAD( sunAzimuth ) ) * idMath::Sin( DEG2RAD( sunZenith ) );
	sunDir.y = idMath::Sin( DEG2RAD( sunAzimuth ) ) * idMath::Sin( DEG2RAD( sunZenith ) );
	sunDir.z = idMath::Cos( DEG2RAD( sunZenith ) );
}

void sdDeclAtmosphere::RebuildTextSource( idFile_Memory &file ) const {
	file.WriteFloatString( "atmosphere %s {\n", GetName() );
	file.WriteFloatString( "\tsunDir ( %f %f %f )\n\tsunColor ( %f %f %f )\n", sunDir.x, sunDir.y, sunDir.z, sunColor.x, sunColor.y, sunColor.z );
	file.WriteFloatString( "\tfogDistHalf %f\n\tfogHeightHalf %f\n\tfogHeightOffset %f\n", fogDistHalf, fogHeightHalf, fogHeightOffset );
	file.WriteFloatString( "\tfogColor ( %f %f %f )\n\tfogStart %f\n\tfogEnd %f\n", fogColor.x, fogColor.y, fogColor.z, fogStart, fogEnd );
	file.WriteFloatString( "\tfarClip %f\n\tisNight %i\n\tdrawAtmosphereLast %i\n", farClip, isNight, drawAtmosphereLast );
	file.WriteFloatString( "\twindAngle %f\n\twindAngleDev %f\n\twindStrength %f\n\twindStrengthDev %f\n", windAngle, windAngleDev, windStrength, windStrengthDev );
	for ( int i = 0; i < numPrecipLayers; i++ ) precipitation[i].Save( file );
	file.WriteFloatString( "}\n" );
}

void sdDeclAtmosphere::Save( idFile_Memory &file ) const { RebuildTextSource( file ); }

void sdDeclAtmosphere::Save() {
	idFile_Memory file( va( "atmosphere %s", GetName() ) );
	RebuildTextSource( file ); SetText( file.GetDataPtr() ); ReplaceSourceFileText(); modified = false;
}

const sdDeclAtmosphere *R_FindAtmosphere( const char *name, bool makeDefault ) {
	if ( !name || !name[0] ) return NULL;
	return static_cast<const sdDeclAtmosphere *>( declManager->FindType( DECL_ATMOSPHERE, name, makeDefault ) );
}
