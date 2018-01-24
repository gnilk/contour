#pragma once

namespace gnilk {

class RenderWindow {
private:
	GLFWwindow *window = NULL;
	int preferred_monitor = 0;
	int px_width = 0;
	int px_height = 0;

	bool quit;


public:
	RenderWindow();
	void OpenWindow(int width, int height, const char *title, bool fullScreen);
	void CloseWindow();
	bool ShouldWindowClose();
	void SetWindowPos(int xp, int yp);
	void UpdateWindow();
	void SetupRenderContext();
	void SetWindowTitle(const char *title);
	void MakeWindowCurrent();
	void BeginRender();
	void EndRender();

// callbacks
	void OnKeyDown(int key, int scancode, int mods);


// getters/setters
	GLFWwindow *GetWindow() { return window; }
	int GetWidth() { return px_width; }
	int GetHeight() { return px_height; }
};	
}