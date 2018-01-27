#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>

#include <imgui.h>
#include <GLFW/glfw3.h>

#include "animation.h"
#include "uicontrollers.h"

using namespace gnilk;

namespace gnilk {

	class UIBaseWindow {
	public:
		virtual void Render() = 0;
	};

	class UIWindow {
	private:
		GLFWwindow* window;
		std::vector<UIBaseWindow *> controllers;
	public:
		void Open();
		void Render();
		void Close();
		void SetPos(int x, int y);
		void AttachController(UIBaseWindow *controller);	
		void MakeCurrent();
	};


	class UIRenderView : public UIBaseWindow {
	private:
		AnimController *controller;
	public:
		UIRenderView(AnimController *controller);
		virtual void Render();
	};


	class UIGenerateView : public UIBaseWindow {
	private:
		GenerateController *controller;
	public:
		UIGenerateView(GenerateController *controller);
		virtual void Render();

	};

	class UIImageView : public UIBaseWindow {
	private:
		ImageController *controller;
	public:
		UIImageView(ImageController *controller);
		virtual void Render();
	};

}
