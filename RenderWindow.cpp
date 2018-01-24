#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <vector>

#include <math.h>

#include <OpenGl/glu.h>
#include "RenderWindow.h"

using namespace gnilk;

static void glfwOnKey(GLFWwindow *window, int key, int scancode, int action, int mods);


RenderWindow::RenderWindow() {
	window = NULL;
	preferred_monitor = 0;
	px_width = 0;
	px_height = 0;
	quit = false;
}

void RenderWindow::OpenWindow(int width, int height, const char *name, bool fullScreen) {
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
		printf("Creating window\n");
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
	glfwSetWindowUserPointer(window, (void *)this);
//	glfwSetCharCallback(window, glfwOnChar);
//	glfwSetKeyCallback(window, glfwOnKey);
	printf("Init done\n");

}

void RenderWindow::CloseWindow() {
	glfwDestroyWindow(window);
}


void RenderWindow::SetWindowTitle(const char *title) {
	glfwSetWindowTitle(window, title);
}

bool RenderWindow::ShouldWindowClose() {
	bool shouldClose = glfwWindowShouldClose(window);
	return (shouldClose | quit);
}
void RenderWindow::SetWindowPos(int xp, int yp)
{
	glfwSetWindowPos(window, xp, yp);
}
void RenderWindow::MakeWindowCurrent()
{
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &px_width, &px_height);
}
void RenderWindow::BeginRender() {
	MakeWindowCurrent();
	glViewport(0, 0, px_width, px_height);	
}
void RenderWindow::EndRender() {
	glfwSwapBuffers(window);
}


void RenderWindow::OnKeyDown(int key, int scancode, int mods) {
	float dt = 1;	// default is jump one second
	if ((mods & GLFW_MOD_ALT)) {
		dt = 10;		// jump 10 seconds if ALT is pressed
	}
	switch(key) {
		case GLFW_KEY_ESCAPE :
			quit = true;
			break;
/*			
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
*/			
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
	printf("glfwOnKey: key:%d, scancode: %d, action: %d, mods: %d\n", key, scancode, action, mods);
	RenderWindow *pThis = (RenderWindow *)glfwGetWindowUserPointer(window);
	if (pThis == NULL) {
		return;
	}
	switch(action) {
		case GLFW_PRESS   : pThis->OnKeyDown(key, scancode, mods); break;
//		case GLFW_RELEASE : OnKeyUp(key, scancode, mods); break;
//		case GLFW_REPEAT  : OnKeyDown(key, scancode, mods); break;
	}

}

