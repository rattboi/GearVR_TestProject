/************************************************************************************

Filename    :   OvrApp.cpp
Content     :   Trivial use of the application framework.
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OvrApp.h"
#include "GuiSys.h"
#include "OVR_Locale.h"

using namespace OVR;

#if defined( OVR_OS_ANDROID )
extern "C" {

jlong Java_oculus_MainActivity_nativeSetAppInterface( JNIEnv * jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	LOG( "nativeSetAppInterface" );
	return (new OvrTemplateApp::OvrApp())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

#endif

namespace OvrTemplateApp
{

OvrApp::OvrApp()
	: SoundEffectContext( NULL )
	, SoundEffectPlayer( NULL )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( NULL )
{
}

OvrApp::~OvrApp()
{
	OvrGuiSys::Destroy( GuiSys );
}

void OvrApp::Configure( ovrSettings & settings )
{
	settings.PerformanceParms.CpuLevel = 2;
	settings.PerformanceParms.GpuLevel = 2;
}

void OvrApp::OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI )
{
	OVR_UNUSED( fromPackage );
	OVR_UNUSED( launchIntentJSON );
	OVR_UNUSED( launchIntentURI );

	const ovrJava * java = app->GetJava();
	SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
	SoundEffectContext->Initialize();
	SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

	Locale = ovrLocale::Create( *app, "default" );

	String fontName;
	GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
	GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );
        
	const OvrStoragePaths & paths = app->GetStoragePaths();

	Array<String> SearchPaths;
	paths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	paths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );
	paths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	paths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );

	const char * scenePath = "Oculus/tuscany.ovrscene";
	String SceneFile;
	if ( GetFullPath( SearchPaths, scenePath, SceneFile ) )
	{
		MaterialParms materialParms;
		materialParms.UseSrgbTextureFormats = false;
		Scene.LoadWorldModel( SceneFile.ToCStr(), materialParms );
		Scene.SetYawOffset( -Mathf::Pi * 0.5f );
	}
	else
	{
		LOG( "OvrApp::OneTimeInit SearchPaths failed to find %s", scenePath );
	}
}

void OvrApp::OneTimeShutdown()
{
	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;
}

bool OvrApp::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}
	return false;
}

Matrix4f OvrApp::Frame( const VrFrame & vrFrame )
{
	// Player movement.
	Scene.Frame( vrFrame, app->GetHeadModelParms() );

	// Update GUI systems after the app frame, but before rendering anything.
	GuiSys->Frame( vrFrame, Scene.GetCenterEyeViewMatrix() );

	return Scene.GetCenterEyeViewMatrix();
}

Matrix4f OvrApp::DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms )
{
	const Matrix4f viewMatrix = Scene.GetEyeViewMatrix( eye );
	const Matrix4f projectionMatrix = Scene.GetEyeProjectionMatrix( eye, fovDegreesX, fovDegreesY );
	const Matrix4f eyeViewProjection = Scene.DrawEyeView( eye, fovDegreesX, fovDegreesY );

	frameParms.ExternalVelocity = Scene.GetExternalVelocity();
	frameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	GuiSys->RenderEyeView( Scene.GetCenterEyeViewMatrix(), viewMatrix, projectionMatrix );

	return eyeViewProjection;
}

} // namespace OvrTemplateApp
