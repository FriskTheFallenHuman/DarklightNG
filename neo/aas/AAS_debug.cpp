/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.
This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

DarklightNG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DarklightNG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "AAS_local.h"

static idRenderWorld *AASDebugRenderWorld( void ) {
	return session ? session->rw : NULL;
}

static bool AASDebugPlayerViewAxis( idMat3 &viewAxis ) {
	if ( !gameEdit || !gameEdit->PlayerIsValid() ) {
		return false;
	}
	gameEdit->PlayerGetViewAxis( viewAxis );
	return true;
}


/*
============
idAASLocal::DrawCone
============
*/
void idAASLocal::DrawCone( const idVec3 &origin, const idVec3 &dir, float radius, const idVec4 &color ) const {
	int i;
	idMat3 axis;
	idVec3 center, top, p, lastp;

	axis[2] = dir;
	axis[2].NormalVectors( axis[0], axis[1] );
	axis[1] = -axis[1];

	center = origin + dir;
	top = center + dir * (3.0f * radius);
	lastp = center + radius * axis[1];

	for ( i = 20; i <= 360; i += 20 ) {
		p = center + sin( DEG2RAD(i) ) * radius * axis[0] + cos( DEG2RAD(i) ) * radius * axis[1];
		AASDebugRenderWorld()->DebugLine( color, lastp, p, 0 );
		AASDebugRenderWorld()->DebugLine( color, p, top, 0 );
		lastp = p;
	}
}

/*
============
idAASLocal::DrawReachability
============
*/
void idAASLocal::DrawReachability( const idReachability *reach ) const {
	AASDebugRenderWorld()->DebugArrow( colorCyan, reach->start, reach->end, 2 );

	idMat3 viewAxis;
	if ( AASDebugPlayerViewAxis( viewAxis ) ) {
		AASDebugRenderWorld()->DrawText( va( "%d", reach->edgeNum ), ( reach->start + reach->end ) * 0.5f, 0.1f, colorWhite, viewAxis );
	}

	switch( reach->travelType ) {
		case TFL_WALK: {
			const idReachability_Walk *walk = static_cast<const idReachability_Walk *>(reach);
			break;
		}
		default: {
			break;
		}
	}
}

/*
============
idAASLocal::DrawEdge
============
*/
void idAASLocal::DrawEdge( int edgeNum, bool arrow ) const {
	const aasEdge_t *edge;
	idVec4 *color;

	if ( !file ) {
		return;
	}

	edge = &file->GetEdge( edgeNum );
	color = &colorRed;
	if ( arrow ) {
		AASDebugRenderWorld()->DebugArrow( *color, file->GetVertex( edge->vertexNum[0] ), file->GetVertex( edge->vertexNum[1] ), 1 );
	} else {
		AASDebugRenderWorld()->DebugLine( *color, file->GetVertex( edge->vertexNum[0] ), file->GetVertex( edge->vertexNum[1] ) );
	}

	idMat3 viewAxis;
	if ( AASDebugPlayerViewAxis( viewAxis ) ) {
		AASDebugRenderWorld()->DrawText( va( "%d", edgeNum ), ( file->GetVertex( edge->vertexNum[0] ) + file->GetVertex( edge->vertexNum[1] ) ) * 0.5f + idVec3(0,0,4), 0.1f, colorRed, viewAxis );
	}
}

/*
============
idAASLocal::DrawFace
============
*/
void idAASLocal::DrawFace( int faceNum, bool side ) const {
	int i, j, numEdges, firstEdge;
	const aasFace_t *face;
	idVec3 mid, end;

	if ( !file ) {
		return;
	}

	face = &file->GetFace( faceNum );
	numEdges = face->numEdges;
	firstEdge = face->firstEdge;

	mid = vec3_origin;
	for ( i = 0; i < numEdges; i++ ) {
		DrawEdge( abs( file->GetEdgeIndex( firstEdge + i ) ), ( face->flags & FACE_FLOOR ) != 0 );
		j = file->GetEdgeIndex( firstEdge + i );
		mid += file->GetVertex( file->GetEdge( abs( j ) ).vertexNum[ j < 0 ] );
	}

	mid /= numEdges;
	if ( side ) {
		end = mid - 5.0f * file->GetPlane( file->GetFace( faceNum ).planeNum ).Normal();
	} else {
		end = mid + 5.0f * file->GetPlane( file->GetFace( faceNum ).planeNum ).Normal();
	}
	AASDebugRenderWorld()->DebugArrow( colorGreen, mid, end, 1 );
}

/*
============
idAASLocal::DrawArea
============
*/
void idAASLocal::DrawArea( int areaNum ) const {
	int i, numFaces, firstFace;
	const aasArea_t *area;
	idReachability *reach;

	if ( !file ) {
		return;
	}

	area = &file->GetArea( areaNum );
	numFaces = area->numFaces;
	firstFace = area->firstFace;

	for ( i = 0; i < numFaces; i++ ) {
		DrawFace( abs( file->GetFaceIndex( firstFace + i ) ), file->GetFaceIndex( firstFace + i ) < 0 );
	}

	for ( reach = area->reach; reach; reach = reach->next ) {
		DrawReachability( reach );
	}
}

/*
============
idAASLocal::DefaultSearchBounds
============
*/
const idBounds &idAASLocal::DefaultSearchBounds( void ) const {
	return file->GetSettings().boundingBoxes[0];
}

/*
============
idAASLocal::ShowArea
============
*/
void idAASLocal::ShowArea( const idVec3 &origin ) const {
	static int lastAreaNum;
	int areaNum;
	const aasArea_t *area;
	idVec3 org;

	areaNum = PointReachableAreaNum( origin, DefaultSearchBounds(), (AREA_REACHABLE_WALK|AREA_REACHABLE_FLY) );
	org = origin;
	PushPointIntoAreaNum( areaNum, org );

	const int goalArea = cvarSystem->GetCVarInteger( "aas_goalArea" );
	if ( goalArea ) {
		int travelTime;
		idReachability *reach;
		
		RouteToGoalArea( areaNum, org, goalArea, TFL_WALK|TFL_AIR, travelTime, &reach );
		common->Printf( "\rtt = %4d", travelTime );
		if ( reach ) {
			common->Printf( " to area %4d", reach->toAreaNum );
			DrawArea( reach->toAreaNum );
		}
	}

	if ( areaNum != lastAreaNum ) {
		area = &file->GetArea( areaNum );
		common->Printf( "area %d: ", areaNum );
		if ( area->flags & AREA_LEDGE ) {
			common->Printf( "AREA_LEDGE " );
		}
		if ( area->flags & AREA_REACHABLE_WALK ) {
			common->Printf( "AREA_REACHABLE_WALK " );
		}
		if ( area->flags & AREA_REACHABLE_FLY ) {
			common->Printf( "AREA_REACHABLE_FLY " );
		}
		if ( area->contents & AREACONTENTS_CLUSTERPORTAL ) {
			common->Printf( "AREACONTENTS_CLUSTERPORTAL " );
		}
		if ( area->contents & AREACONTENTS_OBSTACLE ) {
			common->Printf( "AREACONTENTS_OBSTACLE " );
		}
		common->Printf( "\n" );
		lastAreaNum = areaNum;
	}

	if ( org != origin ) {
		idBounds bnds = file->GetSettings().boundingBoxes[ 0 ];
		bnds[ 1 ].z = bnds[ 0 ].z;
		AASDebugRenderWorld()->DebugBounds( colorYellow, bnds, org );
	}

	DrawArea( areaNum );
}

/*
============
idAASLocal::ShowWalkPath
============
*/
void idAASLocal::ShowWalkPath( const idVec3 &origin, int goalAreaNum, const idVec3 &goalOrigin ) const {
	int i, areaNum, curAreaNum, travelTime;
	idReachability *reach;
	idVec3 org, areaCenter;
	aasPath_t path;

	if ( !file ) {
		return;
	}

	org = origin;
	areaNum = PointReachableAreaNum( org, DefaultSearchBounds(), AREA_REACHABLE_WALK );
	PushPointIntoAreaNum( areaNum, org );
	curAreaNum = areaNum;

	for ( i = 0; i < 100; i++ ) {

		if ( !RouteToGoalArea( curAreaNum, org, goalAreaNum, TFL_WALK|TFL_AIR, travelTime, &reach ) ) {
			break;
		}

		if ( !reach ) {
			break;
		}

		AASDebugRenderWorld()->DebugArrow( colorGreen, org, reach->start, 2 );
		DrawReachability( reach );

		if ( reach->toAreaNum == goalAreaNum ) {
			break;
		}

		curAreaNum = reach->toAreaNum;
		org = reach->end;
	}

	if ( WalkPathToGoal( path, areaNum, origin, goalAreaNum, goalOrigin, TFL_WALK|TFL_AIR ) ) {
		AASDebugRenderWorld()->DebugArrow( colorBlue, origin, path.moveGoal, 2 );
	}
}

/*
============
idAASLocal::ShowFlyPath
============
*/
void idAASLocal::ShowFlyPath( const idVec3 &origin, int goalAreaNum, const idVec3 &goalOrigin ) const {
	int i, areaNum, curAreaNum, travelTime;
	idReachability *reach;
	idVec3 org, areaCenter;
	aasPath_t path;

	if ( !file ) {
		return;
	}

	org = origin;
	areaNum = PointReachableAreaNum( org, DefaultSearchBounds(), AREA_REACHABLE_FLY );
	PushPointIntoAreaNum( areaNum, org );
	curAreaNum = areaNum;

	for ( i = 0; i < 100; i++ ) {

		if ( !RouteToGoalArea( curAreaNum, org, goalAreaNum, TFL_WALK|TFL_FLY|TFL_AIR, travelTime, &reach ) ) {
			break;
		}

		if ( !reach ) {
			break;
		}

		AASDebugRenderWorld()->DebugArrow( colorPurple, org, reach->start, 2 );
		DrawReachability( reach );

		if ( reach->toAreaNum == goalAreaNum ) {
			break;
		}

		curAreaNum = reach->toAreaNum;
		org = reach->end;
	}

	if ( FlyPathToGoal( path, areaNum, origin, goalAreaNum, goalOrigin, TFL_WALK|TFL_FLY|TFL_AIR ) ) {
		AASDebugRenderWorld()->DebugArrow( colorBlue, origin, path.moveGoal, 2 );
	}
}

/*
============
idAASLocal::ShowWallEdges
============
*/
void idAASLocal::ShowWallEdges( const idVec3 &origin ) const {
	int i, areaNum, numEdges, edges[1024];
	idVec3 start, end;
	idMat3 viewAxis;
	if ( !AASDebugPlayerViewAxis( viewAxis ) ) {
		return;
	}

	areaNum = PointReachableAreaNum( origin, DefaultSearchBounds(), (AREA_REACHABLE_WALK|AREA_REACHABLE_FLY) );
	numEdges = GetWallEdges( areaNum, idBounds( origin ).Expand( 256.0f ), TFL_WALK, edges, 1024 );
	for ( i = 0; i < numEdges; i++ ) {
		GetEdge( edges[i], start, end );
		AASDebugRenderWorld()->DebugLine( colorRed, start, end );
		AASDebugRenderWorld()->DrawText( va( "%d", edges[i] ), ( start + end ) * 0.5f, 0.1f, colorWhite, viewAxis );
	}
}

/*
============
idAASLocal::ShowHideArea
============
*/
void idAASLocal::ShowHideArea( const idVec3 &origin, int targetAreaNum ) const {
	int areaNum;
	idVec3 target;
	aasGoal_t goal;

	areaNum = PointReachableAreaNum( origin, DefaultSearchBounds(), (AREA_REACHABLE_WALK|AREA_REACHABLE_FLY) );
	target = AreaCenter( targetAreaNum );

	DrawCone( target, idVec3(0,0,1), 16.0f, colorYellow );

	if ( gameEdit && gameEdit->AASFindHideArea( this, areaNum, origin, target, TFL_WALK | TFL_AIR, idBounds( target ).Expand( 16 ), goal.areaNum, goal.origin ) ) {
		DrawArea( goal.areaNum );
		ShowWalkPath( origin, goal.areaNum, goal.origin );
		DrawCone( goal.origin, idVec3(0,0,1), 16.0f, colorWhite );
	}
}

/*
============
idAASLocal::PullPlayer
============
*/
bool idAASLocal::PullPlayer( const idVec3 &origin, int toAreaNum ) const {
	return gameEdit ? gameEdit->AASPullPlayer( this, origin, toAreaNum ) : true;
}

/*
============
idAASLocal::RandomPullPlayer
============
*/
void idAASLocal::RandomPullPlayer( const idVec3 &origin ) const {
	int rnd, i, n;

	const int pullArea = cvarSystem->GetCVarInteger( "aas_pullPlayer" );
	if ( !PullPlayer( origin, pullArea ) ) {

		rnd = ( gameEdit ? gameEdit->AASRandomFloat() : 0.0f ) * file->GetNumAreas();

		for ( i = 0; i < file->GetNumAreas(); i++ ) {
			n = (rnd + i) % file->GetNumAreas();
			if ( file->GetArea( n ).flags & (AREA_REACHABLE_WALK|AREA_REACHABLE_FLY) ) {
				cvarSystem->SetCVarInteger( "aas_pullPlayer", n );
			}
		}
	} else {
		ShowWalkPath( origin, pullArea, AreaCenter( pullArea ) );
	}
}

/*
============
idAASLocal::ShowPushIntoArea
============
*/
void idAASLocal::ShowPushIntoArea( const idVec3 &origin ) const {
	int areaNum;
	idVec3 target;

	target = origin;
	areaNum = PointReachableAreaNum( target, DefaultSearchBounds(), (AREA_REACHABLE_WALK|AREA_REACHABLE_FLY) );
	PushPointIntoAreaNum( areaNum, target );
	AASDebugRenderWorld()->DebugArrow( colorGreen, origin, target, 1 );
}

/*
============
idAASLocal::Test
============
*/
void idAASLocal::Test( const idVec3 &origin ) {

	if ( !file ) {
		return;
	}

	if ( !AASDebugRenderWorld() ) {
		return;
	}

	if ( cvarSystem->GetCVarBool( "aas_randomPullPlayer" ) ) {
		RandomPullPlayer( origin );
	}
	const int pullArea = cvarSystem->GetCVarInteger( "aas_pullPlayer" );
	if ( ( pullArea > 0 ) && ( pullArea < file->GetNumAreas() ) ) {
		ShowWalkPath( origin, pullArea, AreaCenter( pullArea ) );
		PullPlayer( origin, pullArea );
	}
	const int showPath = cvarSystem->GetCVarInteger( "aas_showPath" );
	if ( ( showPath > 0 ) && ( showPath < file->GetNumAreas() ) ) {
		ShowWalkPath( origin, showPath, AreaCenter( showPath ) );
	}
	const int showFlyPath = cvarSystem->GetCVarInteger( "aas_showFlyPath" );
	if ( ( showFlyPath > 0 ) && ( showFlyPath < file->GetNumAreas() ) ) {
		ShowFlyPath( origin, showFlyPath, AreaCenter( showFlyPath ) );
	}
	const int showHideArea = cvarSystem->GetCVarInteger( "aas_showHideArea" );
	if ( ( showHideArea > 0 ) && ( showHideArea < file->GetNumAreas() ) ) {
		ShowHideArea( origin, showHideArea );
	}
	if ( cvarSystem->GetCVarBool( "aas_showAreas" ) ) {
		ShowArea( origin );
	}
	if ( cvarSystem->GetCVarBool( "aas_showWallEdges" ) ) {
		ShowWallEdges( origin );
	}
	if ( cvarSystem->GetCVarBool( "aas_showPushIntoArea" ) ) {
		ShowPushIntoArea( origin );
	}
}
