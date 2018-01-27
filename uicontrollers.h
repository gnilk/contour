#pragma once

#include <GLFW/glfw3.h>

#include "bitmap.h"
#include <string>
#include "process.h"
#include "animation.h"

namespace gnilk {
	class AnimController {
	public:
		Animation animation;
		AnimationRenderVars animRenderVars;
		char *animFilename;
	public:
		AnimController();
		void LoadData(char *filename);
		void ReloadData();
		void ResetRenderVars();
		int GetMaxFrames();
		int GetMaxStrips(int frameIndex);
		int GetMaxPoints(int frameIndex, int stripIndex);
		AnimationRenderVars GetRenderVars() { return animRenderVars; }
		void SetRenderVars(AnimationRenderVars newVars) { animRenderVars = newVars; }
		Animation Animation() { return animation; }
	};

	class ProcessReadDefaults : public ProcessCallbackInterface {
	private:
		std::string strSettings;
	public:
		void OnProcessStarted();
		void OnProcessExit();
		void OnStdOutData(std::string data);
		void OnStdErrData(std::string data);

		std::string GetSettings();
	};

	class GenerateController {
	public:
		// names corresponding to command line args
		int gl;	// greyscale
		int bs;	// blocksize
		float cnt;	// Image contrast factor
		float cns;	// Image contrast scale
		float lcd;	// Line Cut off distance, break condition for new segment
		float lca;	// Line Cut off Angle, break condition for new segment
		float lld;	// Long Line Distance, break condition for new cluster/strip
		float ccd;	// Cluster Cut off Distance
		float oca;	// Optimization Cut off Angle, break condition for strip contcatination 
	public:
		GenerateController();
		void ResetParameters();
		void ReadDefaultSettings();
		void GenerateData();	
	private:
		void ParseSettings(std::string strSettings);
		std::string GetArg(const char *format, ...);
	};	

	class OpenGLImage {
	private:
		Bitmap *bitmap;
		std::string filename;
		GLuint textureid;

		OpenGLImage(std::string filename);
	public:
		static OpenGLImage *LoadPNG(std::string filename);
		bool Reload();
		GLuint GetTextureID();
		int Width() { return bitmap->Width(); }
		int Height() { return bitmap->Height(); }
	};
	class ImageController {
	private:
		std::vector<OpenGLImage *> images;
	public:
		int ImageCount();
		GLuint GetImageTextureID(int index);
		OpenGLImage *GetImage(int index);
		void ReloadAllImages();
		bool LoadImage(std::string filename);

	};
}
