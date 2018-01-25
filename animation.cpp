#include "animation.h"
#include <OpenGl/glu.h>
#include <math.h>
#include <vector>

using namespace gnilk;

static AnimationRenderVars glbRenderVars = {
	.numStrips = 0,	
	.idxFrame = 0,
	.highLightFirstInStrip = true,
	.highLightLineInStrip = true,
	.idxLineInStrip = 0,
	.idxStrip = 0,
};



void Strip::Normalize() {
	for (int i=0;i<points.size();i++) {
		points[i].fx = (float(points[i].x) - 128.0) / 128.0;
		points[i].fy = (float(points[i].y) - 128.0) / 128.0;

		// printf("      %f, %f\n", points[i].fx, points[i].fy);
	}
}
float Strip::Len(int idxStart, int idxEnd) {
	float dx = points[idxEnd].fx - points[idxStart].fx;
	float dy = points[idxEnd].fy - points[idxStart].fy;

	return sqrt(dx*dx + dy*dy);
}

float Strip::FixLen(int idxStart, int idxEnd) {
	float dx = points[idxEnd].x - points[idxStart].x;
	float dy = points[idxEnd].y - points[idxStart].y;

	return sqrt(dx*dx + dy*dy);
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
void Strip::Render(bool highlight) {
	glBegin(GL_LINE_STRIP);
	for (int i=0;i<points.size();i++) {
		glVertex3f(points[i].fx, points[i].fy, 0);
	}
	glEnd();

	if (glbRenderVars.highLightFirstInStrip) {
		glColor3f(1,0,0);
		glBegin(GL_LINES);
		glVertex3f(points[0].fx, points[0].fy, 0);
		glVertex3f(points[1].fx, points[1].fy, 0);
		glEnd();
		glColor3f(1,1,1);
	}
	if (highlight) {
		int idx = glbRenderVars.idxLineInStrip;
		glDisable(GL_DEPTH_TEST);
		glColor3f(0,1,0);
		glBegin(GL_LINES);
		glVertex3f(points[idx].fx, points[idx].fy, 0);
		glVertex3f(points[idx+1].fx, points[idx+1].fy, 0);
		glEnd();
		glColor3f(1,1,1);		
		glEnable(GL_DEPTH_TEST);
	}

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
	int nStrips = glbRenderVars.numStrips >= strips.size()? strips.size() : glbRenderVars.numStrips;

	for (int i=0;i<nStrips;i++) {
		bool highlight = false;
		if (glbRenderVars.highLightLineInStrip && (glbRenderVars.idxStrip == i)) 
		{
			highlight = true;
		}
		strips[i].Render(highlight);
	}
}

void Frame::Dump() {
	int totalPoints = 0;
	printf("  Strips In Frame: %d\n", strips.size());
	for(int i=0;i<strips.size();i++) {
		printf("    %d, Points: %d\n", i, strips[i].Points());
		totalPoints += strips[i].Points();
	}
	printf("  Total Num Points: %d\n", totalPoints);
}


void Animation::LoadFromFile(const char *filename) {
	FILE *f = fopen(filename,"r");
	if (f == NULL) {
		perror("Unable to open strips file");
	}

	this->frames.clear();

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
int Animation::MaxStrips() {
	int nStrips = 0;
	for (int i=0;i<frames.size();i++) {
		if (frames[i]->Strips() > nStrips) {
			nStrips = frames[i]->Strips();
		}
	}
	return nStrips;
}
void Animation::Render(int frame, AnimationRenderVars renderVars) {
	glbRenderVars = renderVars;
	frames[frame]->Render();
}
