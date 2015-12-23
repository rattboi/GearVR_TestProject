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
#include "App.h"

#if 0
#define GL( func )		func; EglCheckErrors();
#else
#define GL( func )		func;
#endif

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
    static const char VERTEX_SHADER[] =
            "#version 300 es\n"
            "in vec3 Position;\n"
            "in vec4 VertexColor;\n"
            "uniform mat4 Viewm;\n"
            "uniform mat4 Projectionm;\n"
            "out vec4 fragmentColor;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = Projectionm * ( Viewm * vec4( Position, 1.0 ) );\n"
            "   fragmentColor = VertexColor;\n"
            "}\n";

    static const char FRAGMENT_SHADER[] =
            "#version 300 es\n"
            "in lowp vec4 fragmentColor;\n"
            "out lowp vec4 outColor;\n"
            "void main()\n"
            "{\n"
            "   outColor = fragmentColor;\n"
            "}\n";

OvrApp::OvrApp()
	: SoundEffectContext( NULL )
	, SoundEffectPlayer( NULL )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( NULL )
{
	CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
}

OvrApp::~OvrApp()
{
	OvrGuiSys::Destroy( GuiSys );
}

void OvrApp::Configure( ovrSettings & settings )
{
	settings.PerformanceParms.CpuLevel = 2;
	settings.PerformanceParms.GpuLevel = 3;
    settings.EyeBufferParms.multisamples = 4;
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

    myShaderProgram = BuildProgram( VERTEX_SHADER, FRAGMENT_SHADER );

    VertexAttribs attribs;
    Array< TriangleIndex > indices;
    int index_counter = 0;
    for (int i = 0; i < 500; i++) {
        float x1 = (drand48() - 0.5f) * 50.0f;
        float y1 = (drand48() - 0.5f) * 50.0f;
        float z1 = (drand48() - 0.5f) * 50.0f;

        float x2 = x1 + drand48() * 0.7;
        float y2 = y1 + drand48() * 0.7;
        float z2 = z1 + drand48() * 0.7;

        float x3 = x1 + drand48() * 0.7;
        float y3 = y1 + drand48() * 0.7;
        float z3 = z1 + drand48() * 0.7;

        attribs.position.PushBack(Vector3f(x1, y1, z1));
        attribs.position.PushBack(Vector3f(x2, y2, z2));
        attribs.position.PushBack(Vector3f(x3, y3, z3));

        attribs.color.PushBack(Vector4f(1.0, 0.5, 0.3, 1.0));
        attribs.color.PushBack(Vector4f(0.2, 0.5, 1.0, 1.0));
        attribs.color.PushBack(Vector4f(0.3, 1.0, 0.3, 1.0));

        indices.PushBack(index_counter++);
        indices.PushBack(index_counter++);
        indices.PushBack(index_counter++);
    }

    myMesh.Create( attribs, indices );
}

void OvrApp::OneTimeShutdown()
{
	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;

    DeleteProgram(myShaderProgram);
    myMesh.Free();
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
	CenterEyeViewMatrix = vrapi_GetCenterEyeViewMatrix( &app->GetHeadModelParms(), &vrFrame.Tracking, NULL );

	// Update GUI systems after the app frame, but before rendering anything.
	GuiSys->Frame( vrFrame, CenterEyeViewMatrix);

	return CenterEyeViewMatrix;
}

Matrix4f OvrApp::DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms )
{
	OVR_UNUSED( frameParms );

	const Matrix4f eyeViewMatrix = vrapi_GetEyeViewMatrix( &app->GetHeadModelParms(), &CenterEyeViewMatrix, eye );
	const Matrix4f eyeProjectionMatrix = ovrMatrix4f_CreateProjectionFov( fovDegreesX, fovDegreesY, 0.0f, 0.0f, 1.0f, 0.0f );
	const Matrix4f eyeViewProjection = eyeProjectionMatrix * eyeViewMatrix;

    GL( glClearColor( 0.125f, 0.0f, 0.125f, 1.0f ) );
    GL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
    GL( glUseProgram( myShaderProgram.program ) );
    GL( glUniformMatrix4fv( myShaderProgram.uView, 1, GL_TRUE, eyeViewMatrix.M[0]) );
    GL( glUniformMatrix4fv( myShaderProgram.uProjection, 1, GL_TRUE, eyeProjectionMatrix.M[0]) );

    GL( glBindVertexArray(myMesh.vertexArrayObject) );
    GL( glDrawElements(GL_TRIANGLES, myMesh.indexCount, GL_UNSIGNED_SHORT, NULL) );

    GL( glBindVertexArray(0) );
    GL( glUseProgram(0) );

	GuiSys->RenderEyeView( CenterEyeViewMatrix, eyeViewMatrix, eyeProjectionMatrix );
	frameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	return eyeViewProjection;
}

} // namespace OvrTemplateApp
