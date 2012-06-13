/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
===============================================================================

	Trace model vs. polygonal model collision detection.

===============================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "CollisionModel_local.h"


/*
===============================================================================

Visualisation code

===============================================================================
*/

const char *cm_contentsNameByIndex[] = {
	"none",							// 0
	"solid",						// 1
	"opaque",						// 2
	"water",						// 3
	"playerclip",					// 4
	"monsterclip",					// 5
	"moveableclip",					// 6
	"ikclip",						// 7
	"blood",						// 8
	"body",							// 9
	"corpse",						// 10
	"trigger",						// 11
	"aas_solid",					// 12
	"aas_obstacle",					// 13
	"flashlight_trigger",			// 14
	NULL
};

int cm_contentsFlagByIndex[] = {
	-1,								// 0
	CONTENTS_SOLID,					// 1
	CONTENTS_OPAQUE,				// 2
	CONTENTS_WATER,					// 3
	CONTENTS_PLAYERCLIP,			// 4
	CONTENTS_MONSTERCLIP,			// 5
	CONTENTS_MOVEABLECLIP,			// 6
	CONTENTS_IKCLIP,				// 7
	CONTENTS_BLOOD,					// 8
	CONTENTS_BODY,					// 9
	CONTENTS_CORPSE,				// 10
	CONTENTS_TRIGGER,				// 11
	CONTENTS_AAS_SOLID,				// 12
	CONTENTS_AAS_OBSTACLE,			// 13
	CONTENTS_FLASHLIGHT_TRIGGER,	// 14
	0
};

static idVec4 cm_color;

/*
================
idCollisionModelManagerLocal::ContentsFromString
================
*/
int idCollisionModelManagerLocal::ContentsFromString( const char *string ) const {
	int i, contents = 0;
	idLexer src( string, idStr::Length( string ), "ContentsFromString" );
	idToken token;

	while( src.ReadToken( &token ) ) {
		if ( token == "," ) {
			continue;
		}
		for ( i = 1; cm_contentsNameByIndex[i] != NULL; i++ ) {
			if ( token.Icmp( cm_contentsNameByIndex[i] ) == 0 ) {
				contents |= cm_contentsFlagByIndex[i];
				break;
			}
		}
	}

	return contents;
}

/*
================
idCollisionModelManagerLocal::StringFromContents
================
*/
const char *idCollisionModelManagerLocal::StringFromContents( const int contents ) const {
	int i, length = 0;
	static char contentsString[MAX_STRING_CHARS];

	contentsString[0] = '\0';

	for ( i = 1; cm_contentsFlagByIndex[i] != 0; i++ ) {
		if ( contents & cm_contentsFlagByIndex[i] ) {
			if ( length != 0 ) {
				length += idStr::snPrintf( contentsString + length, sizeof( contentsString ) - length, "," );
			}
			length += idStr::snPrintf( contentsString + length, sizeof( contentsString ) - length, cm_contentsNameByIndex[i] );
		}
	}

	return contentsString;
}

/*
================
idCollisionModelManagerLocal::DrawEdge
================
*/
void idCollisionModelManagerLocal::DrawEdge( cm_model_t *model, int edgeNum, const idVec3 &origin, const idMat3 &axis ) {
	int side;
	cm_edge_t *edge;
	idVec3 start, end, mid;
	bool isRotated;

	isRotated = axis.IsRotated();

	edge = model->edges + abs(edgeNum);
	side = edgeNum < 0;

	start = model->vertices[edge->vertexNum[side]].p;
	end = model->vertices[edge->vertexNum[!side]].p;
	if ( isRotated ) {
		start *= axis;
		end *= axis;
	}
	start += origin;
	end += origin;
}

/*
================
idCollisionModelManagerLocal::DrawPolygon
================
*/
void idCollisionModelManagerLocal::DrawPolygon( cm_model_t *model, cm_polygon_t *p, const idVec3 &origin, const idMat3 &axis, const idVec3 &viewOrigin ) {
}

/*
================
idCollisionModelManagerLocal::DrawNodePolygons
================
*/
void idCollisionModelManagerLocal::DrawNodePolygons( cm_model_t *model, cm_node_t *node,
										   const idVec3 &origin, const idMat3 &axis,
										   const idVec3 &viewOrigin, const float radius ) {
}

/*
================
idCollisionModelManagerLocal::DrawModel
================
*/
void idCollisionModelManagerLocal::DrawModel( cmHandle_t handle, const idVec3 &modelOrigin, const idMat3 &modelAxis,
					const idVec3 &viewOrigin, const float radius ) {
}

void idCollisionModelManagerLocal::DebugOutput(const idVec3 &)
{
}
