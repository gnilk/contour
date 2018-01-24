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

using namespace gnilk;

static Animation animation;
static AnimationRenderVars animRenderVars = {
	.numStrips = 255,
	.idxFrame = 0,
	.highLightFirstInStrip = true,
	.highLightLineInStrip = true,
	.idxStrip = 0,
	.idxLineInStrip = 0,
};


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

class UIWindow {
private:
	GLFWwindow* window;
public:
	void Open();
	void Render();
	void Close();
	void SetPos(int x, int y);

};

void UIWindow::Open() {
    window = glfwCreateWindow(1280, 720, "ImGui OpenGL3 example", NULL, NULL);
    glfwMakeContextCurrent(window);    
    glfwSwapInterval(1); // Enable vsync

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // Setup style
    ImGui::StyleColorsClassic();
}

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

void UIWindow::Render() {
    glfwMakeContextCurrent(window);    
    ImGui_ImplGlfwGL2_NewFrame();

    // 1. Show a simple window.
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets automatically appears in a window called "Debug".
    {
        static float f = 0.0f;
        static int nStrips = 0;
        ImGui::Text("Hello, world!");                           // Some text (you can use a format string too)
        // Do frame control
        ImGui::Text("Frame");
        ImGui::SameLine();
        ImGui::SliderInt("##Frame", &animRenderVars.idxFrame, 0, animation.Frames()-1);
        ImGui::SameLine();
        if (ImGui::Button("Prev##Frame")) {
        	if (animRenderVars.idxFrame > 0) {
        		animRenderVars.idxFrame -= 1;
        		animRenderVars.idxLineInStrip = 0;
        	}
        }
        ImGui::SameLine();
        if (ImGui::Button("Next##Frame")) {
        	if (animRenderVars.idxFrame <= animation.Frames()) {
        		animRenderVars.idxFrame += 1;
        		animRenderVars.idxLineInStrip = 0;
        	}
        }
        //printf("NStrips\n");
        int numStrips = animation.At(animRenderVars.idxFrame)->Strips();
        if (numStrips > 0) {
        	//printf("bla\n");
	        // Strip control
	        ImGui::Text("Strips in Frame: %d", animation.At(animRenderVars.idxFrame)->Strips());
	        ImGui::Text("Strips");
			ImGui::SameLine();
	        ImGui::SliderInt("##Strip", &animRenderVars.numStrips, 0, animation.At(animRenderVars.idxFrame)->Strips()-1);
			ImGui::SameLine();
	        if (ImGui::Button("Prev##Strip")) {
	        	printf("StripPrev\n");
	        	if (animRenderVars.numStrips > 0) {
	        		animRenderVars.numStrips -= 1;
	        	}
	        }
	        ImGui::SameLine();
	        if (ImGui::Button("Next##Strip")) {
	        	if (animRenderVars.numStrips <= animation.At(animRenderVars.idxFrame)->Strips()) {
	        		animRenderVars.numStrips += 1;
	        	}
	        }

	        // Line Control
	        ImGui::Text("Strip");
			ImGui::SameLine();
	        ImGui::SliderInt("##StripFocus", &animRenderVars.idxStrip, 0, animation.At(animRenderVars.idxFrame)->Strips()-1);
			ImGui::SameLine();
	        if (ImGui::Button("Prev##StripFocus")) {
	        	if (animRenderVars.idxStrip > 0) {
	        		animRenderVars.idxStrip -= 1;
	        		animRenderVars.idxLineInStrip = 0;
	        	}
	        }
	        ImGui::SameLine();
	        if (ImGui::Button("Next##StripFocus")) {
	        	if (animRenderVars.idxStrip <= animation.At(animRenderVars.idxFrame)->Strips()) {
	        		animRenderVars.idxStrip += 1;
	        		animRenderVars.idxLineInStrip = 0;
	        	}
	        }

	        int idxStrip = animRenderVars.idxStrip;
	        if (idxStrip > animation.At(animRenderVars.idxFrame)->Strips()) {
	        	idxStrip = animation.At(animRenderVars.idxFrame)->Strips()-1;
	        }

	        //printf("idxStrip: %d\n", idxStrip);


	        int numLinesInStrip = animation.At(animRenderVars.idxFrame)->At(idxStrip).Points();
	        ImGui::Text("Points in Strip: %d", numLinesInStrip);
	        ImGui::Text("Lines");
			ImGui::SameLine();
	        ImGui::SliderInt("##Line", &animRenderVars.idxLineInStrip, 0, numLinesInStrip);
			ImGui::SameLine();
	        if (ImGui::Button("Prev##Line")) {
	        	if (animRenderVars.idxLineInStrip > 0) {
	        		animRenderVars.idxLineInStrip -= 1;
	        	}
	        }
	        ImGui::SameLine();
	        if (ImGui::Button("Next##Line")) {
	        	if (animRenderVars.idxLineInStrip <= numLinesInStrip) {
	        		animRenderVars.idxLineInStrip += 1;
	        	}
	        }

	        // Point ptStart = animation.At(animRenderVars.idxFrame)->At(idxStrip).At(animRenderVars.idxLineInStrip);
	        // Point ptEnd = animation.At(animRenderVars.idxFrame)->At(idxStrip).At(animRenderVars.idxLineInStrip+1);

	        float vLen = animation.At(animRenderVars.idxFrame)->At(idxStrip).Len(animRenderVars.idxLineInStrip, animRenderVars.idxLineInStrip+1);
	        float iLen = animation.At(animRenderVars.idxFrame)->At(idxStrip).FixLen(animRenderVars.idxLineInStrip, animRenderVars.idxLineInStrip+1);

	        ImGui::Text("Line, %d -> %d, length: %f (%f)", 0,0,vLen,iLen);

        }


        // Other stuff
        ImGui::Checkbox("Highlight First Point", &animRenderVars.highLightFirstInStrip);



        // ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats as a color
        // if (ImGui::Button("Demo Window"))                       // Use buttons to toggle our bools. We could use Checkbox() as well.
        //     show_demo_window ^= 1;
        // if (ImGui::Button("Another Window")) 
        //     show_another_window ^= 1;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }

    // 2. Show another simple window. In most cases you will use an explicit Begin/End pair to name the window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello from another window!");
        ImGui::End();
    }

    // 3. Show the ImGui demo window. Most of the sample code is in ImGui::ShowDemoWindow().
    if (show_demo_window)
    {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // Rendering
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui::Render();
    glfwSwapBuffers(window);
}
void UIWindow::Close() {
    // Cleanup
    ImGui_ImplGlfwGL2_Shutdown();	
}

void UIWindow::SetPos(int x, int y) {
	glfwSetWindowPos(window, x, y);
}


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

void testFrameLoader(char *filename) {
	//animation.LoadFromFile("strips_full_movie_0.95_opt.db");
	animation.LoadFromFile(filename);
	animRenderVars.numStrips = animation.MaxStrips();

}

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
	testFrameLoader(filename);
	animation.At(0)->Dump();
	Initialize();
	window.OpenWindow(1280,720,"render", false);
	window.SetWindowPos(0,0);
	ui.Open();
	ui.SetPos(1280,0);
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

}


static int frameCounter = 0;

// Callback from UpdateWindow
void RenderAnimation(float tRender) {
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


