#include <stdio.h>
#include <string>

#include <GLFW/glfw3.h>
#include <OpenGL/glu.h>

#include "logger.h"
#include "process.h"
#include "animation.h"
#include "uicontrollers.h"
#include "inifile.h"

using namespace gnilk;

AnimController::AnimController() {
	ResetRenderVars();
}
void AnimController::LoadData(char *filename) {
	this->animFilename = strdup(filename);
	ReloadData();
}

void AnimController::ReloadData() {
	if (animFilename == NULL) {
		return;
	}
	printf("Reloading: %s\n", animFilename);
	animation.LoadFromFile(animFilename);
	animRenderVars.numStrips = animation.MaxStrips();
}
void AnimController::ResetRenderVars() {
	animRenderVars = {
	.numStrips = 255,
	.idxFrame = 0,
	.highLightFirstInStrip = true,
	.highLightLineInStrip = true,
	.idxStrip = 0,
	.idxLineInStrip = 0,
	};
}

int AnimController::GetMaxFrames() {
	return (animation.Frames());
}
int AnimController::GetMaxStrips(int frameIndex) {
	return animation.At(frameIndex)->Strips();
}
int AnimController::GetMaxPoints(int frameIndex, int stripIndex) {
	return animation.At(frameIndex)->At(stripIndex).Points();
}

//
// Generate controller, responsible for generation of data
//
GenerateController::GenerateController() {
	ResetParameters();
}
void GenerateController::ResetParameters() {
	this->gl = 64;	// set default values
	this->bs = 16;

	this->cnt = 4.0;
	this->cns = 2.0;
	this->lcd = 1.0;
	this->lca = 0.5;
	this->lld = 3.0;
	this->ccd = 50.0;
	this->oca = 0.95;	
}

void GenerateController::ReadDefaultSettings() {
	ProcessReadDefaults defaultReader;
	Process proc("./contour");
	proc.AddArgument("-defaults");
	proc.SetCallback(dynamic_cast<ProcessCallbackInterface *>(&defaultReader));
	proc.ExecuteAndWait();

	ParseSettings(defaultReader.GetSettings());
}

void GenerateController::ParseSettings(std::string strSettings) {
	// ILogger *logger = Logger::GetLogger("Defaults");
	// logger->Debug("'%s'", strSettings.c_str());

	inifile::SectionContainer settings;
	settings.FromBuffer(strSettings);


	this->gl = std::stoi(settings.GetValue("defaults", "gl", "32"));
	this->bs = std::stoi(settings.GetValue("defaults", "bs", "16"));
	this->cnt = std::stof(settings.GetValue("defaults", "cnt", "4.0"));
	this->cns = std::stof(settings.GetValue("defaults", "cns", "2.0"));
	this->lcd = std::stof(settings.GetValue("defaults", "lcd", "1.0"));
	this->lca = std::stof(settings.GetValue("defaults", "lca", "0.5"));
	this->lld = std::stof(settings.GetValue("defaults", "lld", "3.0"));
	this->ccd = std::stof(settings.GetValue("defaults", "ccd", "50.0"));
	this->oca = std::stof(settings.GetValue("defaults", "oca", "0.95"));
}

void GenerateController::GenerateData() {
	printf("Generate New Data!\n");
	// This works!
	Process proc("./contour");
	proc.AddArgument(GetArg("-m"));
	proc.AddArgument(GetArg("-gl %d", this->gl));
	proc.AddArgument(GetArg("-bs %d", this->bs));
	proc.AddArgument(GetArg("-cnt %f", this->cnt));
	proc.AddArgument(GetArg("-cns %f", this->cns));
	proc.AddArgument(GetArg("-lcd %f", this->lcd));
	proc.AddArgument(GetArg("-lca %f", this->lca));
	proc.AddArgument(GetArg("-lld %f", this->lld));
	proc.AddArgument(GetArg("-ccd %f", this->ccd));
	proc.AddArgument(GetArg("-oca %f", this->oca));
	proc.AddArgument("tanks/icbm_2.png");
	proc.AddArgument("ui_strips_tmp.db");
	proc.ExecuteAndWait();
}

std::string GenerateController::GetArg(const char *format, ...) {
	va_list	values;
	int szBuffer = 1024;
	char * newstr = NULL;
	int res;
	do
	{
		newstr = (char *)alloca(szBuffer);
		va_start( values, format );
		res = vsnprintf(newstr, szBuffer, format, values);
		va_end(values);
		if (res < 0) {
			szBuffer += 1024;
		}
	} while(res < 0);
	return std::string(newstr);
}

std::string ProcessReadDefaults::GetSettings() {
	return strSettings;
}
void ProcessReadDefaults::OnProcessStarted() {
}
void ProcessReadDefaults::OnProcessExit() {
}
void ProcessReadDefaults::OnStdOutData(std::string data) {
	strSettings = strSettings.append(data.c_str());
	// ILogger *logger = Logger::GetLogger("Defaults");
	// logger->Debug("ReadData:\n%s", data.c_str());
	// logger->Debug("SoFar:\n%s", strSettings.c_str());
}
void ProcessReadDefaults::OnStdErrData(std::string data) {

}
/////////// Image handling
OpenGLImage *OpenGLImage::LoadPNG(std::string filename) {
	auto image = new OpenGLImage(filename);
	if (!image->Reload()) {
		return NULL;
	}
	return image;
}
OpenGLImage::OpenGLImage(std::string filename) {
	textureid = -1;
	bitmap = NULL;
	this->filename = filename;
}

bool OpenGLImage::Reload() {
	if (bitmap != NULL) {
		delete bitmap;
	}
	bitmap = Bitmap::LoadPNGImage(filename);
	if (bitmap == NULL) {
		Logger::GetLogger("OpenGLImage")->Debug("Failed to load image");
		return false;
	}

	auto err = glGetError();

//	glEnable(GL_TEXTURE_2D);
//	glActiveTexture(GL_TEXTURE0);	
	glGenTextures(1, &textureid);
	// printf("TextureID: %d\n", textureid);
	// err = glGetError();
	// printf("Err: %d\n", err);
	glBindTexture(GL_TEXTURE_2D, textureid);
	// err = glGetError();
	// printf("Err: %d\n", err);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// err = glGetError();
	// printf("Err: %d\n", err);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->Width(), bitmap->Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->Buffer());

	err = glGetError();
//	printf("Err: %d\n", err);
	if (err != GL_NO_ERROR) {
		Logger::GetLogger("OpenGLImage")->Debug("Error uploading image!");
	} else {
		Logger::GetLogger("OpenGLImage")->Debug("Image '%s' loaded, dimensions: %dx%d, textureid: %d\n", filename.c_str(), bitmap->Width(), bitmap->Height(), textureid);

	}

	return true;
}
GLuint OpenGLImage::GetTextureID() {
	return textureid;
}

/// Image controller
int ImageController::ImageCount() {
	return images.size();
}
GLuint ImageController::GetImageTextureID(int index) {
	return images[index]->GetTextureID();
}
OpenGLImage *ImageController::GetImage(int index) {
	return images[index];
}
void ImageController::ReloadAllImages() {
	Logger::GetLogger("ImageController")->Error("ReloadAllImages not implemented!");
}

bool ImageController::LoadImage(std::string filename) {
	auto image = OpenGLImage::LoadPNG(filename);
	if (image == NULL) {
		return false;
	}
	images.push_back(image);
	return true;
}








