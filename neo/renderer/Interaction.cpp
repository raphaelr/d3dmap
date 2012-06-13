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

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

/*
===========================================================================

idInteraction implementation

===========================================================================
*/

// FIXME: use private allocator for srfCullInfo_t

/*
================
R_CalcInteractionFacing

Determines which triangles of the surface are facing towards the light origin.

The facing array should be allocated with one extra index than
the number of surface triangles, which will be used to handle dangling
edge silhouettes.
================
*/
void R_CalcInteractionFacing( const idRenderEntityLocal *ent, const srfTriangles_t *tri, const idRenderLightLocal *light, srfCullInfo_t &cullInfo ) {
	idVec3 localLightOrigin;

	if ( cullInfo.facing != NULL ) {
		return;
	}

	R_GlobalPointToLocal( ent->modelMatrix, light->globalLightOrigin, localLightOrigin );

	int numFaces = tri->numIndexes / 3;

	if ( !tri->facePlanes || !tri->facePlanesCalculated ) {
		R_DeriveFacePlanes( const_cast<srfTriangles_t *>(tri) );
	}

	cullInfo.facing = (byte *) R_StaticAlloc( ( numFaces + 1 ) * sizeof( cullInfo.facing[0] ) );

	// calculate back face culling
	float *planeSide = (float *) _alloca16( numFaces * sizeof( float ) );

	// exact geometric cull against face
	SIMDProcessor->Dot( planeSide, localLightOrigin, tri->facePlanes, numFaces );
	SIMDProcessor->CmpGE( cullInfo.facing, planeSide, 0.0f, numFaces );

	cullInfo.facing[ numFaces ] = 1;	// for dangling edges to reference
}

/*
=====================
R_CalcInteractionCullBits

We want to cull a little on the sloppy side, because the pre-clipping
of geometry to the lights in dmap will give many cases that are right
at the border we throw things out on the border, because if any one
vertex is clearly inside, the entire triangle will be accepted.
=====================
*/
void R_CalcInteractionCullBits( const idRenderEntityLocal *ent, const srfTriangles_t *tri, const idRenderLightLocal *light, srfCullInfo_t &cullInfo ) {
	int i, frontBits;

	if ( cullInfo.cullBits != NULL ) {
		return;
	}

	frontBits = 0;

	// cull the triangle surface bounding box
	for ( i = 0; i < 6; i++ ) {

		R_GlobalPlaneToLocal( ent->modelMatrix, -light->frustum[i], cullInfo.localClipPlanes[i] );

		// get front bits for the whole surface
		if ( tri->bounds.PlaneDistance( cullInfo.localClipPlanes[i] ) >= LIGHT_CLIP_EPSILON ) {
			frontBits |= 1<<i;
		}
	}

	// if the surface is completely inside the light frustum
	if ( frontBits == ( ( 1 << 6 ) - 1 ) ) {
		cullInfo.cullBits = LIGHT_CULL_ALL_FRONT;
		return;
	}

	cullInfo.cullBits = (byte *) R_StaticAlloc( tri->numVerts * sizeof( cullInfo.cullBits[0] ) );
	SIMDProcessor->Memset( cullInfo.cullBits, 0, tri->numVerts * sizeof( cullInfo.cullBits[0] ) );

	float *planeSide = (float *) _alloca16( tri->numVerts * sizeof( float ) );

	for ( i = 0; i < 6; i++ ) {
		// if completely infront of this clipping plane
		if ( frontBits & ( 1 << i ) ) {
			continue;
		}
		SIMDProcessor->Dot( planeSide, cullInfo.localClipPlanes[i], tri->verts, tri->numVerts );
		SIMDProcessor->CmpLT( cullInfo.cullBits, i, planeSide, LIGHT_CLIP_EPSILON, tri->numVerts );
	}
}

/*
================
R_FreeInteractionCullInfo
================
*/
void R_FreeInteractionCullInfo( srfCullInfo_t &cullInfo ) {
	if ( cullInfo.facing != NULL ) {
		R_StaticFree( cullInfo.facing );
		cullInfo.facing = NULL;
	}
	if ( cullInfo.cullBits != NULL ) {
		if ( cullInfo.cullBits != LIGHT_CULL_ALL_FRONT ) {
			R_StaticFree( cullInfo.cullBits );
		}
		cullInfo.cullBits = NULL;
	}
}

#define	MAX_CLIPPED_POINTS	20
typedef struct {
	int		numVerts;
	idVec3	verts[MAX_CLIPPED_POINTS];
} clipTri_t;

/*
=============
R_ChopWinding

Clips a triangle from one buffer to another, setting edge flags
The returned buffer may be the same as inNum if no clipping is done
If entirely clipped away, clipTris[returned].numVerts == 0

I have some worries about edge flag cases when polygons are clipped
multiple times near the epsilon.
=============
*/
static int R_ChopWinding( clipTri_t clipTris[2], int inNum, const idPlane plane ) {
	clipTri_t	*in, *out;
	float	dists[MAX_CLIPPED_POINTS];
	int		sides[MAX_CLIPPED_POINTS];
	int		counts[3];
	float	dot;
	int		i, j;
	idVec3	mid;
	bool	front;

	in = &clipTris[inNum];
	out = &clipTris[inNum^1];
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	front = false;
	for ( i = 0; i < in->numVerts; i++ ) {
		dot = in->verts[i] * plane.Normal() + plane[3];
		dists[i] = dot;
		if ( dot < LIGHT_CLIP_EPSILON ) {	// slop onto the back
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_FRONT;
			if ( dot > LIGHT_CLIP_EPSILON ) {
				front = true;
			}
		}
		counts[sides[i]]++;
	}

	// if none in front, it is completely clipped away
	if ( !front ) {
		in->numVerts = 0;
		return inNum;
	}
	if ( !counts[SIDE_BACK] ) {
		return inNum;		// inout stays the same
	}

	// avoid wrapping checks by duplicating first value to end
	sides[i] = sides[0];
	dists[i] = dists[0];
	in->verts[in->numVerts] = in->verts[0];

	out->numVerts = 0;
	for ( i = 0 ; i < in->numVerts ; i++ ) {
		idVec3 &p1 = in->verts[i];
		
		if ( sides[i] == SIDE_FRONT ) {
			out->verts[out->numVerts] = p1;
			out->numVerts++;
		}

		if ( sides[i+1] == sides[i] ) {
			continue;
		}
			
		// generate a split point
		idVec3 &p2 = in->verts[i+1];
		
		dot = dists[i] / ( dists[i] - dists[i+1] );
		for ( j = 0; j < 3; j++ ) {
			mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}
			
		out->verts[out->numVerts] = mid;

		out->numVerts++;
	}

	return inNum ^ 1;
}

/*
===================
R_ClipTriangleToLight

Returns false if nothing is left after clipping
===================
*/
static bool	R_ClipTriangleToLight( const idVec3 &a, const idVec3 &b, const idVec3 &c, int planeBits, const idPlane frustum[6] ) {
	int			i;
	clipTri_t	pingPong[2];
	int			p;

	pingPong[0].numVerts = 3;
	pingPong[0].verts[0] = a;
	pingPong[0].verts[1] = b;
	pingPong[0].verts[2] = c;

	p = 0;
	for ( i = 0 ; i < 6 ; i++ ) {
		if ( planeBits & ( 1 << i ) ) {
			p = R_ChopWinding( pingPong, p, frustum[i] );
			if ( pingPong[p].numVerts < 1 ) {
				return false;
			}
		}
	}

	return true;
}

/*
====================
R_CreateLightTris

The resulting surface will be a subset of the original triangles,
it will never clip triangles, but it may cull on a per-triangle basis.
====================
*/
static srfTriangles_t *R_CreateLightTris( const idRenderEntityLocal *ent, 
									 const srfTriangles_t *tri, const idRenderLightLocal *light,
									 const idMaterial *shader, srfCullInfo_t &cullInfo ) {
	int			i;
	int			numIndexes;
	glIndex_t	*indexes;
	srfTriangles_t	*newTri;
	int			c_backfaced;
	int			c_distance;
	idBounds	bounds;
	bool		includeBackFaces;
	int			faceNum;

	c_backfaced = 0;
	c_distance = 0;

	numIndexes = 0;
	indexes = NULL;

	// it is debatable if non-shadowing lights should light back faces. we aren't at the moment
	if ( r_lightAllBackFaces.GetBool() || light->lightShader->LightEffectsBackSides()
			|| shader->ReceivesLightingOnBackSides()
				|| ent->parms.noSelfShadow || ent->parms.noShadow  ) {
		includeBackFaces = true;
	} else {
		includeBackFaces = false;
	}

	// allocate a new surface for the lit triangles
	newTri = R_AllocStaticTriSurf();

	// save a reference to the original surface
	newTri->ambientSurface = const_cast<srfTriangles_t *>(tri);

	// the light surface references the verts of the ambient surface
	newTri->numVerts = tri->numVerts;
	R_ReferenceStaticTriSurfVerts( newTri, tri );

	// calculate cull information
	if ( !includeBackFaces ) {
		R_CalcInteractionFacing( ent, tri, light, cullInfo );
	}
	R_CalcInteractionCullBits( ent, tri, light, cullInfo );

	// if the surface is completely inside the light frustum
	if ( cullInfo.cullBits == LIGHT_CULL_ALL_FRONT ) {

		// if we aren't self shadowing, let back facing triangles get
		// through so the smooth shaded bump maps light all the way around
		if ( includeBackFaces ) {

			// the whole surface is lit so the light surface just references the indexes of the ambient surface
			R_ReferenceStaticTriSurfIndexes( newTri, tri );
			numIndexes = tri->numIndexes;
			bounds = tri->bounds;

		} else {

			// the light tris indexes are going to be a subset of the original indexes so we generally
			// allocate too much memory here but we decrease the memory block when the number of indexes is known
			R_AllocStaticTriSurfIndexes( newTri, tri->numIndexes );

			// back face cull the individual triangles
			indexes = newTri->indexes;
			const byte *facing = cullInfo.facing;
			for ( faceNum = i = 0; i < tri->numIndexes; i += 3, faceNum++ ) {
				if ( !facing[ faceNum ] ) {
					c_backfaced++;
					continue;
				}
				indexes[numIndexes+0] = tri->indexes[i+0];
				indexes[numIndexes+1] = tri->indexes[i+1];
				indexes[numIndexes+2] = tri->indexes[i+2];
				numIndexes += 3;
			}

			// get bounds for the surface
			SIMDProcessor->MinMax( bounds[0], bounds[1], tri->verts, indexes, numIndexes );

			// decrease the size of the memory block to the size of the number of used indexes
			R_ResizeStaticTriSurfIndexes( newTri, numIndexes );
		}

	} else {

		// the light tris indexes are going to be a subset of the original indexes so we generally
		// allocate too much memory here but we decrease the memory block when the number of indexes is known
		R_AllocStaticTriSurfIndexes( newTri, tri->numIndexes );

		// cull individual triangles
		indexes = newTri->indexes;
		const byte *facing = cullInfo.facing;
		const byte *cullBits = cullInfo.cullBits;
		for ( faceNum = i = 0; i < tri->numIndexes; i += 3, faceNum++ ) {
			int i1, i2, i3;

			// if we aren't self shadowing, let back facing triangles get
			// through so the smooth shaded bump maps light all the way around
			if ( !includeBackFaces ) {
				// back face cull
				if ( !facing[ faceNum ] ) {
					c_backfaced++;
					continue;
				}
			}

			i1 = tri->indexes[i+0];
			i2 = tri->indexes[i+1];
			i3 = tri->indexes[i+2];

			// fast cull outside the frustum
			// if all three points are off one plane side, it definately isn't visible
			if ( cullBits[i1] & cullBits[i2] & cullBits[i3] ) {
				c_distance++;
				continue;
			}

			if ( r_usePreciseTriangleInteractions.GetBool() ) {
				// do a precise clipped cull if none of the points is completely inside the frustum
				// note that we do not actually use the clipped triangle, which would have Z fighting issues.
				if ( cullBits[i1] && cullBits[i2] && cullBits[i3] ) {
					int cull = cullBits[i1] | cullBits[i2] | cullBits[i3];
					if ( !R_ClipTriangleToLight( tri->verts[i1].xyz, tri->verts[i2].xyz, tri->verts[i3].xyz, cull, cullInfo.localClipPlanes ) ) {
						continue;
					}
				}
			}

			// add to the list
			indexes[numIndexes+0] = i1;
			indexes[numIndexes+1] = i2;
			indexes[numIndexes+2] = i3;
			numIndexes += 3;
		}

		// get bounds for the surface
		SIMDProcessor->MinMax( bounds[0], bounds[1], tri->verts, indexes, numIndexes );

		// decrease the size of the memory block to the size of the number of used indexes
		R_ResizeStaticTriSurfIndexes( newTri, numIndexes );
	}

	if ( !numIndexes ) {
		R_ReallyFreeStaticTriSurf( newTri );
		return NULL;
	}

	newTri->numIndexes = numIndexes;

	newTri->bounds = bounds;

	return newTri;
}

/*
===============
idInteraction::idInteraction
===============
*/
idInteraction::idInteraction( void ) {
	numSurfaces				= 0;
	surfaces				= NULL;
	entityDef				= NULL;
	lightDef				= NULL;
	lightNext				= NULL;
	lightPrev				= NULL;
	entityNext				= NULL;
	entityPrev				= NULL;
	dynamicModelFrameCount	= 0;
	frustumState			= FRUSTUM_UNINITIALIZED;
	frustumAreas			= NULL;
}

/*
===============
idInteraction::AllocAndLink
===============
*/
idInteraction *idInteraction::AllocAndLink( idRenderEntityLocal *edef, idRenderLightLocal *ldef ) {
	if ( !edef || !ldef ) {
		common->Error( "idInteraction::AllocAndLink: NULL parm" );
	}

	idRenderWorldLocal *renderWorld = edef->world;

	idInteraction *interaction = renderWorld->interactionAllocator.Alloc();

	// link and initialize
	interaction->dynamicModelFrameCount = 0;

	interaction->lightDef = ldef;
	interaction->entityDef = edef;

	interaction->numSurfaces = -1;		// not checked yet
	interaction->surfaces = NULL;

	interaction->frustumState = idInteraction::FRUSTUM_UNINITIALIZED;
	interaction->frustumAreas = NULL;

	// link at the start of the entity's list
	interaction->lightNext = ldef->firstInteraction;
	interaction->lightPrev = NULL;
	ldef->firstInteraction = interaction;
	if ( interaction->lightNext != NULL ) {
		interaction->lightNext->lightPrev = interaction;
	} else {
		ldef->lastInteraction = interaction;
	}

	// link at the start of the light's list
	interaction->entityNext = edef->firstInteraction;
	interaction->entityPrev = NULL;
	edef->firstInteraction = interaction;
	if ( interaction->entityNext != NULL ) {
		interaction->entityNext->entityPrev = interaction;
	} else {
		edef->lastInteraction = interaction;
	}

	// update the interaction table
	if ( renderWorld->interactionTable ) {
		int index = ldef->index * renderWorld->interactionTableWidth + edef->index;
		if ( renderWorld->interactionTable[index] != NULL ) {
			common->Error( "idInteraction::AllocAndLink: non NULL table entry" );
		}
		renderWorld->interactionTable[ index ] = interaction;
	}

	return interaction;
}

/*
===============
idInteraction::FreeSurfaces

Frees the surfaces, but leaves the interaction linked in, so it
will be regenerated automatically
===============
*/
void idInteraction::FreeSurfaces( void ) {
	if ( this->surfaces ) {
		for ( int i = 0 ; i < this->numSurfaces ; i++ ) {
			surfaceInteraction_t *sint = &this->surfaces[i];

			if ( sint->lightTris ) {
				if ( sint->lightTris != LIGHT_TRIS_DEFERRED ) {
					R_FreeStaticTriSurf( sint->lightTris );
				}
				sint->lightTris = NULL;
			}
			if ( sint->shadowTris ) {
				// if it doesn't have an entityDef, it is part of a prelight
				// model, not a generated interaction
				if ( this->entityDef ) {
					R_FreeStaticTriSurf( sint->shadowTris );
					sint->shadowTris = NULL;
				}
			}
			R_FreeInteractionCullInfo( sint->cullInfo );
		}

		R_StaticFree( this->surfaces );
		this->surfaces = NULL;
	}
	this->numSurfaces = -1;
}

/*
===============
idInteraction::Unlink
===============
*/
void idInteraction::Unlink( void ) {

	// unlink from the entity's list
	if ( this->entityPrev ) {
		this->entityPrev->entityNext = this->entityNext;
	} else {
		this->entityDef->firstInteraction = this->entityNext;
	}
	if ( this->entityNext ) {
		this->entityNext->entityPrev = this->entityPrev;
	} else {
		this->entityDef->lastInteraction = this->entityPrev;
	}
	this->entityNext = this->entityPrev = NULL;

	// unlink from the light's list
	if ( this->lightPrev ) {
		this->lightPrev->lightNext = this->lightNext;
	} else {
		this->lightDef->firstInteraction = this->lightNext;
	}
	if ( this->lightNext ) {
		this->lightNext->lightPrev = this->lightPrev;
	} else {
		this->lightDef->lastInteraction = this->lightPrev;
	}
	this->lightNext = this->lightPrev = NULL;
}

/*
===============
idInteraction::UnlinkAndFree

Removes links and puts it back on the free list.
===============
*/
void idInteraction::UnlinkAndFree( void ) {

	// clear the table pointer
	idRenderWorldLocal *renderWorld = this->lightDef->world;
	if ( renderWorld->interactionTable ) {
		int index = this->lightDef->index * renderWorld->interactionTableWidth + this->entityDef->index;
		if ( renderWorld->interactionTable[index] != this ) {
			common->Error( "idInteraction::UnlinkAndFree: interactionTable wasn't set" );
		}
		renderWorld->interactionTable[index] = NULL;
	}

	Unlink();

	FreeSurfaces();

	// free the interaction area references
	areaNumRef_t *area, *nextArea;
	for ( area = frustumAreas; area; area = nextArea ) {
		nextArea = area->next;
		renderWorld->areaNumRefAllocator.Free( area );
	}

	// put it back on the free list
	renderWorld->interactionAllocator.Free( this );
}

/*
===============
idInteraction::MakeEmpty

Makes the interaction empty and links it at the end of the entity's and light's interaction lists.
===============
*/
void idInteraction::MakeEmpty( void ) {

	// an empty interaction has no surfaces
	numSurfaces = 0;

	Unlink();

	// relink at the end of the entity's list
	this->entityNext = NULL;
	this->entityPrev = this->entityDef->lastInteraction;
	this->entityDef->lastInteraction = this;
	if ( this->entityPrev ) {
		this->entityPrev->entityNext = this;
	} else {
		this->entityDef->firstInteraction = this;
	}

	// relink at the end of the light's list
	this->lightNext = NULL;
	this->lightPrev = this->lightDef->lastInteraction;
	this->lightDef->lastInteraction = this;
	if ( this->lightPrev ) {
		this->lightPrev->lightNext = this;
	} else {
		this->lightDef->firstInteraction = this;
	}
}

/*
===============
idInteraction::HasShadows
===============
*/
ID_INLINE bool idInteraction::HasShadows( void ) const {
	return ( !lightDef->parms.noShadows && !entityDef->parms.noShadow && lightDef->lightShader->LightCastsShadows() );
}

/*
===============
idInteraction::MemoryUsed

Counts up the memory used by all the surfaceInteractions, which
will be used to determine when we need to start purging old interactions.
===============
*/
int idInteraction::MemoryUsed( void ) {
	int		total = 0;

	for ( int i = 0 ; i < numSurfaces ; i++ ) {
		surfaceInteraction_t *inter = &surfaces[i];

		total += R_TriSurfMemory( inter->lightTris );
		total += R_TriSurfMemory( inter->shadowTris );
	}

	return total;
}
