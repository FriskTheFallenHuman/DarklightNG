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
 
#ifndef __AASCLUSTER_H__
#define __AASCLUSTER_H__

/*
===============================================================================

	Area Clustering

===============================================================================
*/

class idAASCluster {

public:
	bool					Build( idAASFileLocal *file );
	bool					BuildSingleCluster( idAASFileLocal *file );

private:
	idAASFileLocal *		file;
	bool					noFaceFlood;

private:
	bool					UpdatePortal( int areaNum, int clusterNum );
	bool					FloodClusterAreas_r( int areaNum, int clusterNum );
	void					RemoveAreaClusterNumbers( void );
	void					NumberClusterAreas( int clusterNum );
	bool					FindClusters( void );
	void					CreatePortals( void );
	bool					TestPortals( void );
	void					ReportEfficiency( void );
	void					RemoveInvalidPortals( void );
};

#endif /* !__AASCLUSTER_H__ */
