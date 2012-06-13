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

#include "Model_local.h"
#include "tr_local.h"	// just for R_FreeWorldInteractions and R_CreateWorldInteractions


class idRenderModelManagerLocal : public idRenderModelManager {
public:
							idRenderModelManagerLocal();
	virtual					~idRenderModelManagerLocal() {}

	virtual void			Init();
	virtual void			Shutdown();
	virtual idRenderModel *	AllocModel();
	virtual void			FreeModel( idRenderModel *model );
	virtual idRenderModel *	FindModel( const char *modelName );
	virtual idRenderModel *	CheckModel( const char *modelName );
	virtual idRenderModel *	DefaultModel();
	virtual void			AddModel( idRenderModel *model );
	virtual void			RemoveModel( idRenderModel *model );
	virtual void			ReloadModels( bool forceAll = false );
	virtual void			FreeModelVertexCaches();
	virtual void			WritePrecacheCommands( idFile *file );
	virtual void			BeginLevelLoad();
	virtual void			EndLevelLoad();

	virtual	void			PrintMemInfo( MemInfo_t *mi );

private:
	idList<idRenderModel*>	models;
	idHashIndex				hash;
	idRenderModel *			defaultModel;
	idRenderModel *			beamModel;
	idRenderModel *			spriteModel;
	idRenderModel *			trailModel;
	bool					insideLevelLoad;		// don't actually load now

	idRenderModel *			GetModel( const char *modelName, bool createIfNotFound );

	static void				PrintModel_f( const idCmdArgs &args );
	static void				ListModels_f( const idCmdArgs &args );
	static void				ReloadModels_f( const idCmdArgs &args );
	static void				TouchModel_f( const idCmdArgs &args );
};


idRenderModelManagerLocal	localModelManager;
idRenderModelManager *		renderModelManager = &localModelManager;

/*
==============
idRenderModelManagerLocal::idRenderModelManagerLocal
==============
*/
idRenderModelManagerLocal::idRenderModelManagerLocal() {
	defaultModel = NULL;
	beamModel = NULL;
	spriteModel = NULL;
	insideLevelLoad = false;
	trailModel = NULL;
}

/*
==============
idRenderModelManagerLocal::PrintModel_f
==============
*/
void idRenderModelManagerLocal::PrintModel_f( const idCmdArgs &args ) {
	idRenderModel	*model;

	if ( args.Argc() != 2 ) {
		common->Printf( "usage: printModel <modelName>\n" );
		return;
	}

	model = renderModelManager->CheckModel( args.Argv( 1 ) );
	if ( !model ) {
		common->Printf( "model \"%s\" not found\n", args.Argv( 1 ) );
		return;
	}

	model->Print();
}

/*
==============
idRenderModelManagerLocal::ListModels_f
==============
*/
void idRenderModelManagerLocal::ListModels_f( const idCmdArgs &args ) {
	int		totalMem = 0;
	int		inUse = 0;

	common->Printf( " mem   srf verts tris\n" );
	common->Printf( " ---   --- ----- ----\n" );

	for ( int i = 0 ; i < localModelManager.models.Num() ; i++ ) {
		idRenderModel	*model = localModelManager.models[i];

		if ( !model->IsLoaded() ) {
			continue;
		}
		model->List();
		totalMem += model->Memory();
		inUse++;
	}

	common->Printf( " ---   --- ----- ----\n" );
	common->Printf( " mem   srf verts tris\n" );

	common->Printf( "%i loaded models\n", inUse );
	common->Printf( "total memory: %4.1fM\n", (float)totalMem / (1024*1024) );
}

/*
==============
idRenderModelManagerLocal::ReloadModels_f
==============
*/
void idRenderModelManagerLocal::ReloadModels_f( const idCmdArgs &args ) {
	if ( idStr::Icmp( args.Argv(1), "all" ) == 0 ) {
		localModelManager.ReloadModels( true );
	} else {
		localModelManager.ReloadModels( false );
	}
}


/*
=================
idRenderModelManagerLocal::WritePrecacheCommands
=================
*/
void idRenderModelManagerLocal::WritePrecacheCommands( idFile *f ) {
	for ( int i = 0 ; i < models.Num() ; i++ ) {
		idRenderModel	*model = models[i];

		if ( !model ) {
			continue;
		}
		if ( !model->IsReloadable() ) {
			continue;
		}

		char	str[1024];
		sprintf( str, "touchModel %s\n", model->Name() );
		common->Printf( "%s", str );
		f->Printf( "%s", str );
	}
}

/*
=================
idRenderModelManagerLocal::Init
=================
*/
void idRenderModelManagerLocal::Init() {
	insideLevelLoad = false;
}

/*
=================
idRenderModelManagerLocal::Shutdown
=================
*/
void idRenderModelManagerLocal::Shutdown() {
	models.DeleteContents( true );
	hash.Free();
}

/*
=================
idRenderModelManagerLocal::GetModel
=================
*/
idRenderModel *idRenderModelManagerLocal::GetModel( const char *modelName, bool createIfNotFound ) {
	idStr		canonical;
	idStr		extension;

	if ( !modelName || !modelName[0] ) {
		return NULL;
	}

	canonical = modelName;
	canonical.ToLower();

	// see if it is already present
	int key = hash.GenerateKey( modelName, false );
	for ( int i = hash.First( key ); i != -1; i = hash.Next( i ) ) {
		idRenderModel *model = models[i];

		if ( canonical.Icmp( model->Name() ) == 0 ) {
			if ( !model->IsLoaded() ) {
				// reload it if it was purged
				model->LoadModel();
			} else if ( insideLevelLoad && !model->IsLevelLoadReferenced() ) {
				// we are reusing a model already in memory, but
				// touch all the materials to make sure they stay
				// in memory as well
				model->TouchData();
			}
			model->SetLevelLoadReferenced( true );
			return model;
		}
	}

	// see if we can load it

	// determine which subclass of idRenderModel to initialize

	idRenderModel	*model;

	canonical.ExtractFileExtension( extension );

	if ( ( extension.Icmp( "ase" ) == 0 ) || ( extension.Icmp( "lwo" ) == 0 ) || ( extension.Icmp( "flt" ) == 0 ) ) {
		model = new idRenderModelStatic;
		model->InitFromFile( modelName );
	} else if ( extension.Icmp( "ma" ) == 0 ) {
		model = new idRenderModelStatic;
		model->InitFromFile( modelName );
	} else {

		if ( extension.Length() ) {
			common->Warning( "unknown model type '%s'", canonical.c_str() );
		}

		if ( !createIfNotFound ) {
			return NULL;
		}

		idRenderModelStatic	*smodel = new idRenderModelStatic;
		smodel->InitEmpty( modelName );
		smodel->MakeDefaultModel();

		model = smodel;
	}

	model->SetLevelLoadReferenced( true );

	if ( !createIfNotFound && model->IsDefaultModel() ) {
		delete model;
		model = NULL;

		return NULL;
	}

	AddModel( model );

	return model;
}

/*
=================
idRenderModelManagerLocal::AllocModel
=================
*/
idRenderModel *idRenderModelManagerLocal::AllocModel() {
	return new idRenderModelStatic();
}

/*
=================
idRenderModelManagerLocal::FreeModel
=================
*/
void idRenderModelManagerLocal::FreeModel( idRenderModel *model ) {
	if ( !model ) {
		return;
	}
	if ( !dynamic_cast<idRenderModelStatic *>( model ) ) {
		common->Error( "idRenderModelManager::FreeModel: model '%s' is not a static model", model->Name() );
		return;
	}
	if ( model == defaultModel ) {
		common->Error( "idRenderModelManager::FreeModel: can't free the default model" );
		return;
	}
	if ( model == beamModel ) {
		common->Error( "idRenderModelManager::FreeModel: can't free the beam model" );
		return;
	}
	if ( model == spriteModel ) { 
		common->Error( "idRenderModelManager::FreeModel: can't free the sprite model" );
		return;
	}

	delete model;
}

/*
=================
idRenderModelManagerLocal::FindModel
=================
*/
idRenderModel *idRenderModelManagerLocal::FindModel( const char *modelName ) {
	return GetModel( modelName, true );
}

/*
=================
idRenderModelManagerLocal::CheckModel
=================
*/
idRenderModel *idRenderModelManagerLocal::CheckModel( const char *modelName ) {
	return GetModel( modelName, false );
}

/*
=================
idRenderModelManagerLocal::DefaultModel
=================
*/
idRenderModel *idRenderModelManagerLocal::DefaultModel() {
	return defaultModel;
}

/*
=================
idRenderModelManagerLocal::AddModel
=================
*/
void idRenderModelManagerLocal::AddModel( idRenderModel *model ) {
	hash.Add( hash.GenerateKey( model->Name(), false ), models.Append( model ) );
}

/*
=================
idRenderModelManagerLocal::RemoveModel
=================
*/
void idRenderModelManagerLocal::RemoveModel( idRenderModel *model ) {
	int index = models.FindIndex( model );
	hash.RemoveIndex( hash.GenerateKey( model->Name(), false ), index );
	models.RemoveIndex( index );
}

/*
=================
idRenderModelManagerLocal::ReloadModels
=================
*/
void idRenderModelManagerLocal::ReloadModels( bool forceAll ) {
}

/*
=================
idRenderModelManagerLocal::FreeModelVertexCaches
=================
*/
void idRenderModelManagerLocal::FreeModelVertexCaches() {
	for ( int i = 0 ; i < models.Num() ; i++ ) {
		idRenderModel *model = models[i];
		model->FreeVertexCache();
	}
}

/*
=================
idRenderModelManagerLocal::BeginLevelLoad
=================
*/
void idRenderModelManagerLocal::BeginLevelLoad() {
}

/*
=================
idRenderModelManagerLocal::EndLevelLoad
=================
*/
void idRenderModelManagerLocal::EndLevelLoad() {
}

/*
=================
idRenderModelManagerLocal::PrintMemInfo
=================
*/
void idRenderModelManagerLocal::PrintMemInfo( MemInfo_t *mi ) {
}
