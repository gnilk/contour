//
// Simplistic vector animation renderer for OpenGL
//
// TODO: 
// - Add render to texture
// - Add blur filter
// - Add blossom filter
// + more interesting stuff (try to highlight certain points in the line strip)
//  
//
#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <vector>

#include <math.h>

//#include <OpenGL/gl.h>
#include <OpenGl/glu.h>

#include "bass.h"



// Window related parameters
static GLFWwindow *window = NULL;
static int preferred_monitor = 0;
static int px_width = 0;
static int px_height = 0;

// Time releated variables
static float tLast;
static bool pausePlayer = false;
static double tPause;
static bool quitPlayer = false;

// Rendering related variables
static float fov = 65.0;

void Initialize();
void OpenWindow(int width, int height, const char *title, bool fullScreen);
void CloseWindow();
bool ShouldWindowClose();
void SetWindowPos(int xp, int yp);
void MakeWindowCurrent();
void UpdateWindow();
void SetupRenderContext();
void SetWindowTitle(const char *title);
void Render(float tRender);
void RenderFx(float tRender);
void DrawAxis();


static void glfwOnKey(GLFWwindow *window, int key, int scancode, int action, int mods);

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


struct Point {
	uint8_t x;
	uint8_t y;
	float fx;	// Normalized range (-1 .. 1
	float fy;	// Normalized range (-1 .. 1)
};

class Strip {
private:
	uint8_t nPoints;
	std::vector<Point> points;
private:
	void Normalize();
public:
	void Load(FILE *f);
	int Points() { return points.size(); }
	void Render();
};

class Frame {
private:
	uint8_t nStrips;
	std::vector<Strip> strips;
private:
public:
	void Load(FILE *f);
	int Strips() { return strips.size(); }
	void Render();
};

class Animation {
private:
	std::vector<Frame *> frames;
public:
	void LoadFromFile(const char *filename);
	int Frames() { return frames.size(); }
	void Render(int frame);	
};


void Strip::Normalize() {
	for (int i=0;i<points.size();i++) {
		points[i].fx = (float(points[i].x) - 128.0) / 128.0;
		points[i].fy = (float(points[i].y) - 128.0) / 128.0;

		// printf("      %f, %f\n", points[i].fx, points[i].fy);
	}
}
void Strip::Load(FILE *f) {	
	fread(&nPoints, sizeof(uint8_t), 1, f);
	//printf("Points: %d",nPoints);
	for (int p=0;p<nPoints;p++) {
		uint8_t x,y;
		Point pt;
		fread(&pt.x, sizeof(uint8_t), 1, f);
		fread(&pt.y, sizeof(uint8_t), 1, f);
		points.push_back(pt);
	}
	Normalize();
}
void Strip::Render() {
	glBegin(GL_LINE_STRIP);
	for (int i=0;i<points.size();i++) {
		glVertex3f(points[i].fx, points[i].fy, 0);
	}
	glEnd();
}

void Frame::Load(FILE *f) {
	fread(&nStrips, sizeof(uint8_t), 1, f);
	//printf("  Strips In Frame: %d\n", nStrips);
	for(int i=0;i<nStrips;i++) {
		Strip s;
		//printf("    Strip: %d, ", i);
		s.Load(f);
		//printf("\n");
		strips.push_back(s);
	}
}

void Frame::Render() {
	for (int i=0;i<strips.size();i++) {
		strips[i].Render();
	}
}


void Animation::LoadFromFile(const char *filename) {
	FILE *f = fopen(filename,"r");
	if (f == NULL) {
		perror("Unable to open strips file");
	}

	int frameCounter = 0;
	while(!feof(f)) {
		//printf("Frame: %d, array: %d\n", frameCounter, frames.size());
		Frame *frame = new Frame();
		frame->Load(f);
		this->frames.push_back(frame);
		// This is a frame!
		frameCounter++;
		// if (frameCounter > 143) {
		// 	break;
		// }
	}	
	printf("Frames loaded: %d\n", this->Frames());

	fclose(f);
}

void Animation::Render(int frame) {
	frames[frame]->Render();
}


/////////////////////////
static Animation animation;

void testFrameLoader() {
	animation.LoadFromFile("strips_full_movie_0.95_opt.db");
}

int main(int argc, char **argv) {
	BassPlayer music;
	testFrameLoader();
	music.Initialize();
	//exit(1);
	Initialize();
	OpenWindow(1280,720,"render", false);
	music.StartPlaying();
	glfwSetTime(0);	// Reset timer on first frame..
	while(!ShouldWindowClose()) {
		UpdateWindow();
		glfwPollEvents();
	}
	CloseWindow();
	glfwTerminate();
	return 0;
}

static int frameCounter = 0;

// Callback from UpdateWindow
void RenderAnimation(float tRender) {
	frameCounter = (int)(tRender * 30.0);
	if (frameCounter > animation.Frames()) {
		frameCounter = 0;
	}
	char buffer[256];
	sprintf(buffer,"frame: %d", frameCounter);
	SetWindowTitle(buffer);



	glRotatef(0,1,0,0);
	glRotatef(48*sin(tRender),0,1,0);
	//glRotatef(rotation->v->vector[2],0,0,1);

	glBegin(GL_LINE_LOOP);
		glVertex3f(-1,0.5,0);
		glVertex3f( 1,0.5,0);
		glVertex3f( 1,-1,0);
		glVertex3f(-1,-1, 0);
	glEnd();
	glLineWidth(6.0);
	animation.Render(frameCounter);
}


void Render(float tRender) {
	SetupRenderContext();
	glColor3f(1,1,1);
	RenderAnimation(tRender);
	// RenderFx(tRender);
	// DrawAxis();
}

void SetupRenderContext() {
	glViewport(0, 0, px_width, px_height);
	// Clear color buffer to black
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	

	// Select and setup the projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, (GLfloat) px_width/ (GLfloat) px_height, 1.0f, 1000.0f);

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

void Initialize() {
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exit(EXIT_FAILURE);
	} else {
		int major, minor, rev;
		glfwGetVersion(&major, &minor, &rev);

		printf("GLFW Version %d.%d.%d",major,minor,rev);
	}

}

void OpenWindow(int width, int height, const char *name, bool fullScreen) {
	int count;
	GLFWmonitor **monitors = glfwGetMonitors(&count);
	for(int i=0;i<count;i++) {
		int wmm, hmm;
		printf("Monitor: %d\n", i);
		const GLFWvidmode *currentmode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPhysicalSize(monitors[i], &wmm, &hmm);

		const double dpi = currentmode->width / (wmm / 25.4);
		printf("  Width(mm) : %d\n", wmm);
		printf("  Height(mm): %d\n", hmm);
		printf("  DPI.......: %f\n", dpi);
		printf("  Resolution: %d x %d at %dhz\n", currentmode->width, currentmode->height, currentmode->refreshRate);
	}
 
	if (!fullScreen) {
		window = glfwCreateWindow(width, height, name, NULL, NULL);
	} else {
		GLFWmonitor *usemon = glfwGetPrimaryMonitor();

		if (preferred_monitor <= count) {
			printf("Selecting monitor: %d\n", preferred_monitor);
			usemon = monitors[preferred_monitor];
		}
		// if (count > 1) {
		//  	usemon = monitors[1];
		// }

		const GLFWvidmode *currentmode = glfwGetVideoMode(usemon);
		// in fullscreen w=0, h=0 means use default monitor resolution
		if ((width==0) || (height==0)) {
			width = currentmode->width;
			height = currentmode->height;
		}


		printf("Creating window, resolution: %d x %d\n", width, height);
		glfwWindowHint(GLFW_REFRESH_RATE, currentmode->refreshRate);
		window = glfwCreateWindow(width, height, name, usemon, NULL);
		//glfwSetWindowSize(window, width, height);

		const GLFWvidmode *selectedMode = glfwGetVideoMode(usemon);
		printf("Mode is: %d x %d at %dhz\n", selectedMode->width, selectedMode->height, selectedMode->refreshRate);

	}
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window\n");
		return;
	}
//	glfwSetWindowUserPointer(window, (void *)this);
//	glfwSetCharCallback(window, glfwOnChar);
	glfwSetKeyCallback(window, glfwOnKey);

}

void CloseWindow() {
	glfwDestroyWindow(window);
}


void SetWindowTitle(const char *title) {
	glfwSetWindowTitle(window, title);
}

bool ShouldWindowClose() {
	bool shouldClose = glfwWindowShouldClose(window);
	return (shouldClose | quitPlayer);
}
void SetWindowPos(int xp, int yp)
{
	glfwSetWindowPos(window, xp, yp);
}
void MakeWindowCurrent()
{
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &px_width, &px_height);
}
void UpdateWindow() {
	MakeWindowCurrent();
	glViewport(0, 0, px_width, px_height);	
	Render(glfwGetTime());
	glfwSwapBuffers(window);
}


void OnKeyDown(int key, int scancode, int mods) {
	float dt = 1;	// default is jump one second
	if ((mods & GLFW_MOD_ALT)) {
		dt = 10;		// jump 10 seconds if ALT is pressed
	}
	switch(key) {
		case GLFW_KEY_ESCAPE :
			quitPlayer = true;
			break;
		case GLFW_KEY_SPACE :	// pause replay
			if (pausePlayer) {
				glfwSetTime(tPause);
				pausePlayer = false;
			}
			else {
				tPause = glfwGetTime();
				pausePlayer = true;
			}
			break;
		case GLFW_KEY_LEFT :	// fast forward in time
			{				
				if (!pausePlayer) {
					float nt = glfwGetTime() - dt;
					if (nt < 0) nt = 0;
					glfwSetTime(nt); 					
				} else {
					tPause -= dt;
					if (tPause < 0) tPause = 0;
				}

			}
			break;
		case GLFW_KEY_RIGHT :	// reverse time
			{
				if (!pausePlayer) {
					float nt = glfwGetTime() + dt;
					glfwSetTime(nt); 									
				} else {
					tPause += dt;
				}
			}
			break;
	} // switch
}



// static void glfwOnChar(GLFWwindow *window, unsigned int code) {
// 	//printf("glfwOnChar, %c\n",code);
// 	BaseWindow *pThis = (BaseWindow *)glfwGetWindowUserPointer(window);
// 	if (pThis != NULL) {
// 		pThis->OnChar(code);
// 	}
// }
static void glfwOnKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
	//printf("glfwOnKey: key:%d, scancode: %d, action: %d, mods: %d\n", key, scancode, action, mods);
	switch(action) {
		case GLFW_PRESS   : OnKeyDown(key, scancode, mods); break;
//		case GLFW_RELEASE : OnKeyUp(key, scancode, mods); break;
//		case GLFW_REPEAT  : OnKeyDown(key, scancode, mods); break;
	}

}

