//
// Simplistic vector animation renderer for OpenGL
//
// Note: This depends on imgui in ext/imgui
//
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>

#include <imgui.h>
#include "imgui_impl_glfw_gl2.h"
#include <GLFW/glfw3.h>
#include <OpenGl/glu.h>

#include "bass.h"
#include "animation.h"
#include "RenderWindow.h"
#include "ui.h"
#include "logger.h"
#include "process.h"

using namespace gnilk;

void reloadFrame();

// static Animation animation;
// static AnimationRenderVars animRenderVars = {
// 	.numStrips = 255,
// 	.idxFrame = 0,
// 	.highLightFirstInStrip = true,
// 	.highLightLineInStrip = true,
// 	.idxStrip = 0,
// 	.idxLineInStrip = 0,
// };



// Time releated variables
static float tLast;
static bool pausePlayer = false;
static double tPause;
static bool quitPlayer = false;

// Rendering related variables
static float fov = 65.0;

void Initialize();
void SetupRenderContext();
void Render(float tRender);
void RenderFx(float tRender);
void DrawAxis();


void perror(const char *err) {
	printf("FATAL: %s\n", err);
	exit(1);
}

class BassPlayer {
private:
	char *filename;
	HSTREAM chan;
public:
	void Initialize();
	void StartPlaying();
};


void BassPlayer::Initialize() {
	

	if (HIWORD(BASS_GetVersion())!=BASSVERSION) {
		perror("An incorrect version of BASS was loaded");
	}

	// initialize default output device
	if (!BASS_Init(-1,44100,0,NULL,NULL)) {
		perror("BASS can't initialize device");
	}
}
void BassPlayer::StartPlaying() {
	filename = "music.mp3";
	chan=BASS_StreamCreateFile(FALSE,filename,0,0,BASS_STREAM_PRESCAN);
	if (!chan) {
		perror("BASS Unable to open file");
	}

	// BASS_INFO info;
	// if (BASS_GetInfo(&info)) {
	// 	pLogger->Debug("BASS Info");
	// 	pLogger->Debug("  Min buffer size: %d", info.minbuf);
	// 	pLogger->Debug("  Latency: %d", info.latency);
	// }

	if (!BASS_ChannelPlay(chan, true)) {
		//Error("BASS_ChannelPlay");
		exit(1);
	}

}

/////////////////////////
static RenderWindow window;
static UIWindow ui;
static char *segmentFileName = NULL;

// Controllers - link between UI and Data
static AnimController animController;
static GenerateController generateController;
static ImageController imageController;


// void testFrameLoader(char *filename) {
// 	//animation.LoadFromFile("strips_full_movie_0.95_opt.db");
// 	segmentFileName = filename;
// 	animation.LoadFromFile(filename);
// 	animRenderVars.numStrips = animation.MaxStrips();
// }

int main(int argc, char **argv) {
	// TODO: ARGS!
	char *filename = NULL;
//	printf("ARGS: %d\n", argc);
	if (argc > 1) {
		filename = argv[1];
	} else {
		printf("Usage: player <db file>\n");
		exit(1);
	}

	Initialize();

	window.OpenWindow(640,360,"render", false);
	window.SetWindowPos(0,0);
	ui.Open();
	ui.SetPos(640,0);
	ui.MakeCurrent();


	animController.LoadData(filename);
	UIRenderView uiRenderView(&animController);
	UIGenerateView uiGenerateView(&generateController);
	UIImageView uiImageView(&imageController);
	imageController.LoadImage("tanks/icbm_2.png");
	imageController.LoadImage("ui_strips_tmp.db_pp_contrast.png");
	imageController.LoadImage("ui_strips_tmp.db_pp_contour.png");
	imageController.LoadImage("ui_strips_tmp.db.png");


	// Attach UI Windows
	ui.AttachController(dynamic_cast<UIBaseWindow*>(&uiGenerateView));
	ui.AttachController(dynamic_cast<UIBaseWindow*>(&uiRenderView));
	ui.AttachController(dynamic_cast<UIBaseWindow*>(&uiImageView));
	glfwSetTime(0);	// Reset timer on first frame..
	printf("Entering rendering loop\n");
	while(!window.ShouldWindowClose()) {
		glfwPollEvents();		
		//printf("UI Render\n");
		ui.Render();

		window.BeginRender();
		Render(glfwGetTime());
		window.EndRender();
	}
	window.CloseWindow();
	ui.Close();
	glfwTerminate();
	return 0;
}

void Initialize() {
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exit(EXIT_FAILURE);
	} else {
		int major, minor, rev;
		glfwGetVersion(&major, &minor, &rev);

		printf("GLFW Version %d.%d.%d\n",major,minor,rev);
	}
	int logLevel = Logger::kMCDebug;
	Logger::Initialize();
	if (logLevel != Logger::kMCNone) {
		Logger::AddSink(Logger::CreateSink("LogConsoleSink"), "console", 0, NULL);
	}
	Logger::SetAllSinkDebugLevel(logLevel);	

}


static int frameCounter = 0;

// Callback from UpdateWindow
void RenderAnimation(float tRender) {
	auto animRenderVars = animController.GetRenderVars();
	auto animation = animController.Animation();

	frameCounter = animRenderVars.idxFrame;//(int)(tRender * 30.0);
	if (frameCounter > animation.Frames()) {
		frameCounter = 0;
	}
	char buffer[256];
	sprintf(buffer,"frame: %d", frameCounter);
	window.SetWindowTitle(buffer);

	//glRotatef(0,1,0,0);
	//glRotatef(48*sin(tRender),0,1,0);
	//glRotatef(rotation->v->vector[2],0,0,1);

	glBegin(GL_LINE_LOOP);
		glVertex3f(-1,0.5,0);
		glVertex3f( 1,0.5,0);
		glVertex3f( 1,-1,0);
		glVertex3f(-1,-1, 0);
	glEnd();
	glLineWidth(2.0);
	animation.Render(frameCounter, animRenderVars);
}


void Render(float tRender) {
	SetupRenderContext();
	glColor3f(1,1,1);
	RenderAnimation(tRender);
	// RenderFx(tRender);
	// DrawAxis();
}

void SetupRenderContext() {

	glViewport(0, 0, window.GetWidth(), window.GetHeight());
	// Clear color buffer to black
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	

	// Select and setup the projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, (GLfloat) window.GetWidth() / (GLfloat) window.GetHeight(), 1.0f, 1000.0f);

	// Select and setup the modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0f, 0.0f, -1.75f,    // Eye-position
			0.0f, 0.0f, 0.0f,   // View-point
			0.0f, -1.0f, 0.0f);  // Up-vector

}

void RenderFx(float tRender) {
	glColor3f(1,1,1);
	glBegin(GL_LINES);
		glVertex3f(0,0,0);
		glVertex3f(1*sin(tRender),1*cos(tRender),0);
	glEnd();	
}

void DrawAxis() {
	glColor3f(1,0,0);
	glBegin(GL_LINES);
		glVertex3f(0,0,0);
		glVertex3f(1,0,0);
	glEnd();	
	glColor3f(0,1,0);
	glBegin(GL_LINES);
		glVertex3f(0,0,0);
		glVertex3f(0,1,0);
	glEnd();	
	glColor3f(0,0,1);	
	glBegin(GL_LINES);
		glVertex3f(0,0,0);
		glVertex3f(0,0,1);
	glEnd();	

}


