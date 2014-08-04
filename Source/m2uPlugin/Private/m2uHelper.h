#ifndef _M2UHELPER_H_
#define _M2UHELPER_H_

#include "AssetSelection.h"
#include "m2uAssetHelper.h"
// Provides functions that are used by most likely more than one command or action


namespace m2uHelper
{

	const FString M2U_GENERATED_NAME(TEXT("m2uGeneratedName"));


/**
   Parse a python-style list from a string to an array containing the contents
   of that list.
   The input string should look like this:
   [name1,name2,name3,name4]
 */
	TArray<FString> ParseList(FString Str)
	{
		FString Chopped = Str.Mid(1,Str.Len()-2); // remove the brackets
		TArray<FString> Result;
		Chopped.ParseIntoArray( &Result, TEXT(","), false);
		return Result;
	}

/**
   tries to find an Actor by name and makes sure it is valid.
   @param Name The name to look for
   @param OutActor This will be the found Actor or NULL
   @param InWorld The world in which to search for the Actor

   @return true if found and valid, false otherwise
   TODO: narrow searching to the InWorld or current world if not set.
 */
bool GetActorByName( const TCHAR* Name, AActor** OutActor, UWorld* InWorld = NULL)
{
	if( InWorld == NULL)
	{
		InWorld = GEditor->GetEditorWorldContext().World();
	}
	AActor* Actor;
	Actor = FindObject<AActor>( InWorld->GetCurrentLevel(), Name, false );
	//Actor = FindObject<AActor>( ANY_PACKAGE, Name, false );
	// TODO: check if StaticFindObject or StaticFindObjectFastInternal is better
	// and if searching in current world gives a perfo boost, if thats possible
	if( Actor == NULL ) // actor with that name cannot be found
	{
		return false;
	}
	else if( ! Actor->IsValidLowLevel() )
	{
		//UE_LOG(LogM2U, Log, TEXT("Actor is NOT valid"));
		return false;
	}
	else
	{
		//UE_LOG(LogM2U, Log, TEXT("Actor is valid"));
		*OutActor=Actor;
		return true;
	}
}


/**
 * FName RenameActor( AActor* Actor, const FString& Name)
 *
 * Tries to set the Actor's FName to the desired name, while also setting the Label
 * to the exact same name as the FName has resulted in.
 * The returned FName may differ from the desired name, if that was not valid or
 * already in use.
 *
 * @param Actor The Actor which to edit
 * @param Name The desired new name as a string
 *
 * @return FName The resulting ID (and Label-string)
 *
 * The Label is a friendly name that is displayed everywhere in the Editor and it
 * can take special characters the FName can not. The FName is referred to as the
 * object ID in the Editor. Labels need not be unique, the ID must.
 *
 * There are a few functions which are used to set Actor Labels and Names, but all
 * allow a desync between Label and FName, and sometimes don't change the FName
 * at all if deemed not necessary.
 *
 * But we want to be sure that the name we provide as FName is actually set *if*
 * the name is still available.
 * And we want to be sure the Label is set to represent the FName exactly. It might
 * be very confusing to the user if the Actor's "Name" as seen in the Outliner in
 * the Editor is different from the name seen in the Program, but the objects still
 * are considered to have the same name, because the FName is correct, but the
 * Label is desynced.
 *
 * What we have is:
 * 'SetActorLabel', which is the recommended way of changing the name. This function
 * will set the Label immediately and then try to set the ID using the Actor's
 * 'Rename' method, with names generated by 'MakeObjectNameFromActorLabel' or
 * 'MakeUniqueObjectName'. The Actor's Label and ID are not guaranteed to match
 * when using 'SetActorLabel'.
 * 'MakeObjectNameFromActorLabel' will return a name, stripped of all invalid
 * characters. But if the names are the same, and the ID has a number suffix and
 * the label not, the returned name will not be changed.
 * (rename "Chair_5" to "Chair" will return "Chair_5" although I wanted "Chair")
 * So even using 'SetActorLabel' to set the FName to something unique, based
 * on the Label, and then setting the Label to what ID was returned, is not an
 * option, because the ID might not result in what we provided, even though the
 * name is free and valid.
 *
 * TODO:
 * These functions have the option to create unique names within a specified Outer
 * Object, which might be 'unique within a level'. I don't know if we should
 * generally use globally unique names, or how that might change if we use maya-
 * namespaces for levels.
 */
	FName RenameActor( AActor* Actor, const FString& Name)
	{
		// 1. Generate a valid FName from the String

		FString GeneratedName = Name;
		// create valid object name from the string. (remove invalid characters)
		for( int32 BadCharacterIndex = 0; BadCharacterIndex < ARRAY_COUNT( INVALID_OBJECTNAME_CHARACTERS ) - 1; ++BadCharacterIndex )
		{
			const TCHAR TestChar[2] = { INVALID_OBJECTNAME_CHARACTERS[ BadCharacterIndex ], 0 };
			const int32 NumReplacedChars = GeneratedName.ReplaceInline( TestChar, TEXT( "" ) );
		}
		// is there still a name, or was it stripped completely (pure invalid name)
		// we don't change the name then. The calling function should check
		// this and maybe print an error-message or so.
		if( GeneratedName.IsEmpty() )
		{
			return Actor->GetFName();
		}
		FName NewFName( *GeneratedName );

		// check if name is "None", NAME_None, that is a valid name to assign
		// but in maya the name will be something like "_110" while here it will
		// be "None" with no number. So althoug renaming "succeeded" the names 
		// differ.
		if( NewFName == NAME_None )
		{
			NewFName = FName( *M2U_GENERATED_NAME );
		}


		// 2. Rename the object

		if( Actor->GetFName() == NewFName )
		{
			// the new name and current name are the same. Either the input was
			// the same, or they differed by invalid chars.
			return Actor->GetFName();
		}

		UObject* NewOuter = NULL; // NULL = use the current Outer
		ERenameFlags RenFlags = REN_DontCreateRedirectors;
		bool bCanRename = Actor->Rename( *NewFName.ToString(), NewOuter, REN_Test | REN_DoNotDirty | REN_NonTransactional | RenFlags );
		if( bCanRename )
		{
			Actor->Rename( *NewFName.ToString(), NewOuter, RenFlags);
		}
		else
		{
			// unable to rename the Actor to that name
			return Actor->GetFName();
		}

		// 3. Get the resulting name
		const FName ResultFName = Actor->GetFName();
		// 4. Set the actor label to represent the ID
		//Actor->ActorLabel = ResultFName.ToString(); // ActorLabel is private :(
		Actor->SetActorLabel(ResultFName.ToString()); // this won't change the ID
		return ResultFName;
	}// FName RenameActor()



/**
 * FName GetFreeName(const FString& Name)
 *
 * Will find a free (unused) name based on the Name string provided.
 * This will be achieved by increasing or adding a number-suffix until the
 * name is unique.
 *
 * @param Name A name string with or without a number suffix on which to build onto.
 */
	FName GetFreeName(const FString& Name)
	{
		// Generate a valid FName from the String

		FString GeneratedName = Name;
		// create valid object name from the string. (remove invalid characters)
		for( int32 BadCharacterIndex = 0; BadCharacterIndex < ARRAY_COUNT(
				 INVALID_OBJECTNAME_CHARACTERS ) - 1; ++BadCharacterIndex )
		{
			const TCHAR TestChar[2] = { INVALID_OBJECTNAME_CHARACTERS[
					BadCharacterIndex ], 0 };
			const int32 NumReplacedChars = GeneratedName.ReplaceInline( TestChar,
																		TEXT( "" ) );
		}

		FName TestName( *GeneratedName );
		if( TestName == NAME_None )
		{
			TestName = FName( *M2U_GENERATED_NAME );
		}

		// TODO: maybe check only inside the current level or so?
		//UObject* Outer = ANY_PACKAGE;
		UObject* Outer = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();
		UObject* ExistingObject;

		// increase the suffix until there is no ExistingObject found
		for(;;)
		{
			if (Outer == ANY_PACKAGE)
			{
				ExistingObject = StaticFindObject( NULL, ANY_PACKAGE,
												   *TestName.ToString() );
			}
			else
			{
				ExistingObject = StaticFindObjectFastInternal( NULL, Outer, 
															   TestName );
			}
			
			if( ! ExistingObject ) // current name is not in use
				break;
			// increase suffix
			//TestName = FName(TestName.GetIndex(), TestName.GetNumber() + 1 );
			TestName.SetNumber( TestName.GetNumber() + 1 );
		}
		return TestName;

	}// FName GetFreeName()

	
/**
 * void SetActorTransformRelativeFromText(AActor* Actor, const TCHAR* Stream)
 *
 * Set the Actors relative transformations to the values provided in text-form
 * T=(x y z) R=(x y z) S=(x y z)
 * If one or more of T, R or S is not present in the String, they will be ignored.
 *
 * Relative transformations are the actual transformation values you see in the 
 * Editor. They are equivalent to object-space transforms in maya for example.
 *
 * Setting world-space transforms using SetActorLocation or so will yield fucked
 * up results when using nested transforms (parenting actors).
 *
 * The Actor has to be valid, so check before calling this function!
 */
	void SetActorTransformRelativeFromText(AActor* Actor, const TCHAR* Str)
	{
		const TCHAR* Stream; // used for searching in Str
		//if( Stream != NULL )
		//{	++Stream;  } // skip a space

		// get location
		FVector Loc;
		if( (Stream =  FCString::Strfind(Str,TEXT("T="))) )
		{
			Stream += 3; // skip "T=("
			Stream = GetFVECTORSpaceDelimited( Stream, Loc );
			//UE_LOG(LogM2U, Log, TEXT("Loc %s"), *(Loc.ToString()) );
			Actor->SetActorRelativeLocation( Loc, false );
		}

		// get rotation
		FRotator Rot;
		if( (Stream =  FCString::Strfind(Str,TEXT("R="))) )
		{
			Stream += 3; // skip "R=("
			Stream = GetFROTATORSpaceDelimited( Stream, Rot, 1.0f );
			//UE_LOG(LogM2U, Log, TEXT("Rot %s"), *(Rot.ToString()) );
			Actor->SetActorRelativeRotation( Rot, false );
		}

		// get scale
		FVector Scale;
		if( (Stream =  FCString::Strfind(Str,TEXT("S="))) )
		{
			Stream += 3; // skip "S=("
			Stream = GetFVECTORSpaceDelimited( Stream, Scale );
			//UE_LOG(LogM2U, Log, TEXT("Scc %s"), *(Scale.ToString()) );
			Actor->SetActorRelativeScale3D( Scale );
		}

		Actor->InvalidateLightingCache();
		// Call PostEditMove to update components, etc.
		Actor->PostEditMove( true );
		Actor->CheckDefaultSubobjects();
		// Request saves/refreshes.
		Actor->MarkPackageDirty();

	}// void SetActorTransformRelativeFromText()


/**
 * Spawn a new Actor in the Level. Automatically find the type of Actor to create 
 * based on the type of Asset. 
 * 
 * @param AssetPath The full path to the asset "/Game/Meshes/MyStaticMesh"
 * @param InLevel The Level to add the Actor to
 * @param Name The Name to assign to the Actor (should be a valid FName) or NAME_None
 * @param bSelectActor Select the Actor after it is created
 * @param Location Where to place the Actor
 *
 * @return The newly created Actor
 *
 * Inspired by the DragDrop functionality of the Viewports, see
 * LevelEditorViewport::AttemptDropObjAsActors and
 * SLevelViewport::HandlePlaceDraggedObjects
 */
	AActor* AddNewActorFromAsset( FString AssetPath, 
								  ULevel* InLevel, 
								  FName Name = NAME_None,
								  bool bSelectActor = true, 
								  const FVector& Location = FVector(0,0,0),
								  EObjectFlags ObjectFlags = RF_Transactional)
	{

		UObject* Asset = GetAssetFromPath(AssetPath);
		if( Asset == NULL)
			return NULL;

		UClass* AssetClass = Asset->GetClass();
		
		if( Name == NAME_None)
		{
			Name = FName(TEXT("GeneratedName"));
		}
		AActor* Actor = FActorFactoryAssetProxy::AddActorForAsset( Asset, &Location, false, bSelectActor, ObjectFlags, NULL, Name );
		// The Actor will sometimes receive the Name, but not if it is a blueprint?
		// It will never receive the name as Label, so we set the name explicitly 
		// again here.
		Actor->SetActorLabel(Actor->GetFName().ToString());
		
		return Actor;
	}// AActor* AddNewActorFromAsset()

} // namespace m2uHelper
#endif /* _M2UHELPER_H_ */
