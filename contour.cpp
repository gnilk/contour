#include <math.h>
#include <map>
#include <vector>

#include "contour.h"
#include "bitmap.h"
#include "vec2d.h"
#include "timer.h"

using namespace gnilk;
using namespace gnilk::contour;

static Config glbConfig;

static float VecLen(Point *a, Point *b);


Trace::Trace() {
	SetDefaultConfig();
}

void Trace::SetDefaultConfig() {
	glbConfig.GreyThresholdLevel = 128;
	glbConfig.ClusterCutOffDistance = 5.0f;
	glbConfig.LineCutOffDistance = 5.0f;
	glbConfig.LineCutOffAngle = 0.9f;
	glbConfig.LongLineDistance = 3.0f;
	glbConfig.OptimizationCutOffAngle = 0.95f;
	glbConfig.ContrastFactor = 4; // NOT USED
	glbConfig.ContrastScale = 2; // NOT USED
	glbConfig.BlockSize = 8;
	glbConfig.FilledBlockLevel = 0.5; // NOT USED
	glbConfig.Width = 0;	// Set by initialization to with/height of bitmap
	glbConfig.Height = 0;	// Set by initialization to with/height of bitmap
	glbConfig.Optimize = true;
	glbConfig.Verbose = false;
}


//
// the input image should be processed and contain the contour of the image
//
void Trace::ProcessImage(unsigned char *data, int width, int height) {
	// I believe this is bogus
	glbConfig.Width = width;
	glbConfig.Height = height;

	intermediateWidth = width;
	intermediateHeight = height;

	Bitmap *bitmap = gnilk::Bitmap::FromRGBA(width, height, data);
	BlockMap blockmap(bitmap);

	std::vector<Strip *> optStrips;
	std::vector<Strip *> strips;
	std::vector<LineSegment *> optSegments;

	Timer timer;
	double tStart = timer.GetTime();
	auto points = blockmap.ExtractContourPoints();
	double tContourCluster = tStart - timer.GetTime();
	ContourCluster cluster(points, &blockmap);
	auto lineSegments = cluster.ExtractVectors();
	double tExtractVector = timer.GetTime();

	OptimizeLineSegments(optSegments, lineSegments);
	LineSegmentsToStrips(optStrips, optSegments);
	float tEnd = timer.GetTime();

	// Draw cluster points to image - this is for intermediate imagery
	//RescaleLineSegments(lineSegments, 255, 191);	// hard coded for now..
	LineSegmentsToStrips(strips, lineSegments);

	WriteStrips("player_strips.db", strips);
	WriteStrips("player_opt_strips.db", optStrips);

	auto imgLineSegments = DrawLineSegments(lineSegments);
	imgLineSegments->SaveToFile("player_linesegments.png");

//	DumpStrips("Optimized Strips", optStrips);


	auto imgCluster = DrawCluster(points);
	imgCluster->SaveToFile("player_contourpoints.png");
	printf("AlgoTime: %f\n", tEnd - tStart);
}

Bitmap *Trace::DrawLineSegments(std::vector<LineSegment *> &lineSegments) {
	Bitmap *dst = new Bitmap(intermediateWidth, intermediateHeight);
	for (int i=0;i<lineSegments.size();i++) {
		auto ls = lineSegments[i];
		if (i == 0) {
			DrawLine(dst, ls->Start(), ls->End(), 0,255,0);			
		} else {
			DrawLine(dst, ls->Start(), ls->End(), 255,0,0);			
		}
	}
	return dst;
}
void Trace::DrawLine(Bitmap *dst, Point a, Point b, uint8_t cr, uint8_t cg, uint8_t cb) {
	float len = VecLen(&b,&a);
	float xStep = (float)(b.x - a.x) / len;
	float yStep = (float)(b.y - a.y) / len;

	float xPos = (float)a.x;
	float yPos = (float)a.y;
	for (int i=0;i<len;i++) {
		dst->SetRGBA((int)xPos, (int)yPos, cr,cg,cb,255);
		xPos += xStep;
		yPos += yStep;
	}

}
Bitmap *Trace::DrawCluster(std::vector<ContourPoint *> &points) {
	Bitmap *dst = new Bitmap(intermediateWidth, intermediateHeight);
	for (int i = 0;i<points.size();i++) {
		int x = points[i]->Pt().X();
		int y = points[i]->Pt().Y();

		dst->SetRGBA(x,y,0,0,0,255);
	}
	return dst;
}

void Trace::DumpStrips(const char *title, std::vector<Strip *> &strips) {
	printf("Dumpstrips: %s\n", title);
	for (int i=0;i<strips.size();i++) {
		printf("%d points\n", strips[i]->size());
		for(int j=0;j<strips[i]->size();j++) {
			printf("  %d: (%d,%d)\n", j, strips[i]->at(j).x, strips[i]->at(j).y);
		}
	}
}



void Trace::RescaleLineSegments(std::vector<LineSegment *> &lineSegments, int w, int h) {
	float xFactor = 1.0f / (float)w;
	float yFactor = 1.0f / (float)h;

	for (int i=0;i<lineSegments.size();i++) {
		auto ls = lineSegments[i];

		int xs = (int)(glbConfig.Width * xFactor * (float)ls->Start().X());
		int ys = (int)(glbConfig.Height * yFactor * (float)ls->Start().Y());

		int xe = (int)(glbConfig.Width * xFactor * (float)ls->End().X());
		int ye = (int)(glbConfig.Height * yFactor * (float)ls->End().Y());

		ls->SetStart(xs,ys);
		ls->SetEnd(xe,ye);
	}
}



void Trace::OptimizeLineSegments(std::vector<LineSegment *> &newSegments, std::vector<LineSegment *> &lineSegments) {

	Vec2D vStart;
	Vec2D vEnd;


	for (int i=1;i<lineSegments.size();i++) {
		auto lsStart = lineSegments[i-1];
		auto lsEnd = lineSegments[i];

		if (lsStart->End().IsEqual(lsEnd->Start())) {
			lsStart->AsVector(&vStart);
			lsEnd->AsVector(&vEnd);

			vStart.Norm();
			vEnd.Norm();

			float dev = vStart.Dot(&vEnd);

			if (glbConfig.Verbose) {
				printf("%d, (%d,%d):(%d:%d) -> (%d,%d):(%d:%d) - dev: %f\n",i,
					lsStart->Start().x, lsStart->Start().y, lsStart->End().x, lsStart->End().y,
					lsEnd->Start().x, lsEnd->Start().y, lsEnd->End().x, lsEnd->End().y, 
					dev);
			}

			// line segments aligned?
			if (dev > glbConfig.OptimizationCutOffAngle) {
				if (glbConfig.Verbose) {
					printf("  -> Opt\n");
				}

				LineSegment *lsPrev = lsStart;
				for (; dev > glbConfig.OptimizationCutOffAngle; i++) {
					if (i == lineSegments.size()) {
						break;
					}
					lsEnd = lineSegments[i];		
					if (glbConfig.Verbose) {
						printf("   %d, (%d,%d):(%d:%d) - dev: %f\n",i,
							lsEnd->Start().x, lsEnd->Start().y, lsEnd->End().x, lsEnd->End().y, dev);				
					}



					// cluster check, if this does not belong to the same cluster (discontinuation), break loop and stop line optimization
					if (!lsPrev->End().IsEqual(lsEnd->Start())) {
						if (glbConfig.Verbose) {
							printf("    Break Opt, new cluster detected\n");
						}
						break;
					}

					lsPrev = lsEnd;
					lsEnd->AsVector(&vEnd);
					vEnd.Norm();
					dev = vStart.Dot(&vEnd);
				} // for dev > glbConfig.OptimizationCutOffAngle

				// New linesegment here, contour.go:1589

				if (glbConfig.Verbose) {
					printf("  <- Opt\n");
					printf("NewSeg: (%d,%d):(%d:%d) - dev: %f\n",
								lsStart->Start().x, lsStart->Start().y, lsPrev->End().x, lsPrev->End().y, dev);				
				}
				auto lsNew = new LineSegment(lsStart->Start(), lsPrev->End());
				if (lsNew->Len() < 2.0f) {
					if (glbConfig.Verbose) {
						printf("  OPT: WARNING SHORT LS DETECTED\n");
					}
				}
				newSegments.push_back(lsNew);
			} else {
				if (lsStart->Len() < 2.0f) {
					if (glbConfig.Verbose) {
						printf("NO-OPT: short line segments: %f, skipping\n", lsStart->Len());
					}
				} else {
					newSegments.push_back(lsStart);
				}
			}
		}
	}
	printf("Optimization, segments before: %d, after: %d\n",lineSegments.size(), newSegments.size());
}


int Trace::LineSegmentsToStrips(std::vector<Strip *> &strips, std::vector<LineSegment *> &lineSegments) {

	if (lineSegments.size() == 0) {
		return 0;
	}
	Strip *strip = new Strip();

	LineSegment *lsPrev;
	for (int i=0;i<(lineSegments.size()-1);i++) {
		auto ls = lineSegments[i];
		if (strip->size() > 1) {
			auto dist = VecLen(&strip->at(strip->size()-1), ls->StartPtr());
			if (dist < 2) {
				if (glbConfig.Verbose) {
					printf("  Warninig: At %d, short (%f) line segment detected, skipping\n", i, dist);
				}
			} else {
				strip->push_back(ls->Start());
			}
		} else {
			strip->push_back(ls->Start());
		}

		auto lsNext = lineSegments[i+1];
		if (ls->End().IsEqual(lsNext->Start())) {
			// If we are exceeding max 8bit length, create new strip and continue
			if (strip->size() > (255-2)) {
				if (glbConfig.Verbose) {
					printf("Strip exceeding 255 items, splitting");
				}
				strip->push_back(ls->End());
				strips.push_back(strip);
				strip = new Strip();
			}
		} else {
			// Append ending point and push forward
			strip->push_back(ls->End());
			if (glbConfig.Verbose) {
				//printf("Store strip with %d points\n", strip->size());
			}
			strips.push_back(strip);
			strip = new Strip();
		}
		lsPrev = ls;
	}

	if (lineSegments.size() > 1) {
		auto ls = lineSegments[lineSegments.size() -1];
		if (lsPrev->End().IsEqual(ls->Start())) {
			if (glbConfig.Verbose) {
				printf("Last LS append to current\n");
			}
			strip->push_back(ls->Start());
			strip->push_back(ls->End());
		} else {
			if (glbConfig.Verbose) {
				printf("Last LS require new Strip [not implemented]\n");
			}
			if (strip->size() > 0) {
				strip->push_back(lsPrev->End());
				strips.push_back(strip);
				strip = new Strip();
			}
			strip->push_back(ls->Start());
			strip->push_back(ls->End());
		}
		strips.push_back(strip);
	} else {
		if (glbConfig.Verbose) {
			printf("Only one segment, creating special strip [not implemented]\n");
		}
	}

	// printf("Dumping strips:\n");
	// for(int i=0;i<strips.size();i++) {
	// 	printf("  %d: %d points\n", i, strips[i]->size());
	// }
	return 1;
}

void Trace::WriteStrips(std::string filename, std::vector<Strip *> &strips) {
	FILE *f = fopen(filename.c_str(), "w");
	printf("Strips: %d\n", strips.size());
	uint8_t nStrips = (uint8_t)strips.size();
	fwrite(&nStrips,1,1,f);
	for (int i=0;i<strips.size();i++) {
		auto strip = strips[i];
		//printf("  %d, Points: %d\n", i, strip->size());
		if (strip->size() > 255) {
			printf("WriteStrips failed, more (%d) than 255 points in on strip!!!", strip->size());
		}
		uint8_t nPoints = (uint8_t)strip->size();
		fwrite(&nPoints, 1,1, f);
		for (int j =0;j<strip->size();j++) {
			auto pt = strip->at(j);
			uint8_t x = (uint8_t)pt.X();
			uint8_t y = (uint8_t)pt.Y();
			fwrite(&x,1,1,f);
			fwrite(&y,1,1,f);
		}
	}
	fclose(f);
}

//
// ContourCluster
//
ContourCluster::ContourCluster(std::vector<ContourPoint *> &_points, BlockMap *map) :
	points(_points)
{
	this->blockmap = map;	
}

std::vector<LineSegment *> ContourCluster::ExtractVectors() {
	int idxStart = 0;
	std::vector<LineSegment *> lineSegments;

	// printf("ContourCluster::ExtractVectors\n");
	// Block *block = NULL;
	// block = blockmap->GetBlockForExtraction(block);
	// int blockCount = 0;
	// while (block != NULL) {
	// 	int idxStart = block->points[0]->PIndex();
	// 	for (int i=0;i<block->points.size();i++) {
	// 		auto ls = NextSegment(idxStart);
	// 		if (ls == NULL) {
	// 			if (glbConfig.Verbose) {
	// 				printf("NextSegment, returned NULL - block is processed: %i of %d\n", i, block->points.size());
	// 				break;
	// 			}
	// 		}		
	// 		lineSegments.push_back(ls);
	// 		idxStart = ls->IdxEnd();
	// 	}
	// 	block->SetExtracted();
	// 	block = blockmap->GetBlockForExtraction(block);		
	// 	blockCount++;		
	// }
	// printf("Searched: %d blocks of %d\n", blockCount, blockmap->NumBlocks());
	auto block = blockmap->GetBlockForExtraction(NULL);
	if (block == NULL) {
		printf("No blocks...");
		exit(1);
	}
	idxStart = block->points[0]->PIndex();

	for (int i=0;i<points.size();i++) {
		auto ls = NextSegment(idxStart);
		if (ls == NULL) {
			if (glbConfig.Verbose) {
				printf("NextSegment, returned NULL - looking for new cluster!\n");
			}
			// Local search fail - cluster is somewhere else in picture
			// Initiate search for a new block!
			block = blockmap->GetBlockForExtraction(NULL);
			if (block == NULL) {
				if (glbConfig.Verbose) {
					printf("No unprocessed cluster found, leaving!\n");
				}
				break;
			}
			block->SetExtracted();
			idxStart = block->points[0]->PIndex();
		} else {
			lineSegments.push_back(ls);
			idxStart = ls->IdxEnd();			
		}
	}
	// printf("Number of line segments: %d\n", lineSegments.size());
	return lineSegments;
}

LineSegment *ContourCluster::NextSegment(int idxStart) {
	// Calculate distance from all points to this point and sort low to high
	// Note: The CalcPointDistance will discard any 'Used'/'Visisted' points in the cluster	
	std::vector<PointDistance *> pds;
	CalcPointDistance(idxStart, pds);

	// Sort pds
	if (pds.size() < 2) {
		if (glbConfig.Verbose) {
			printf("Too few points in cluster left\n");
		}
		return NULL;
	}
	std::sort(pds.begin(), pds.end(), PointDistance::Less);

	if (glbConfig.Verbose) {
		printf("NextSegment, idxStart: %d, number of pds: %d\n", idxStart, pds.size());
	}

	// for (int i=0;i<20;i++) {
	// 	if (i >= pds.size()) {
	// 		break;
	// 	}
	// 	printf("%d: %f\n", i, pds[i]->PIndex(), pds[i]->Distance());
	// }
	// exit(1);

	bool longLineMode = false;
	Vec2D *vPrev = NULL;
	float dp = 0.0f;
	int idxPrevious = -1;
	for (int i=0;i<pds.size();i++) {
		auto pd = pds[i];

		//printf("%d, pd.PIndex: %d, pd.Distance: %f\n", i, pd->PIndex(), pd->Distance());

		if ((idxPrevious == -1) && (pd->Distance() > glbConfig.ClusterCutOffDistance)) {
			At(pd->PIndex())->Use();
			if (glbConfig.Verbose) {
				printf("New Cluster Detected, restarting loop");
			}
			return NextSegment(pd->PIndex());
		}

		if (longLineMode) {
			auto vCurrent = NewVector(idxStart, pd->PIndex());
			vCurrent->Norm();
			dp = vPrev->Dot(vCurrent);
			if (glbConfig.Verbose) {
				printf("pd.PIndex: %d, dist: %f, dp: %f\n", pd->PIndex(), pd->Distance(), dp);
			}
		}

		//
		// Break conditions - when a new segment has been found
		//

		if (longLineMode && (dp < glbConfig.LineCutOffAngle)) {
			if (glbConfig.Verbose) {
				printf("NewSegment, angelCutOff, iter: %d, %d -> %d, dist: %f, dp: %f\n", i, idxStart, idxPrevious, pd->Distance(), dp);
				printf("            (%d:%d) -> (%d:%d)\n", At(idxStart)->X(), At(idxStart)->Y(), At(idxPrevious)->X(), At(idxPrevious)->Y());
			}
			return NewLineSegment(idxStart, idxPrevious);
		} else if ((idxPrevious != -1) && (pd->Distance() > glbConfig.LineCutOffDistance)) {
			if (glbConfig.Verbose) {
				printf("NewSegment, lineCutOff, iter: %d, %d -> %d, dist: %f, dp: %f\n", i, idxStart, idxPrevious, pd->Distance(), dp);
				printf("            (%d:%d) -> (%d:%d)\n", At(idxStart)->X(), At(idxStart)->Y(), At(idxPrevious)->X(), At(idxPrevious)->Y());
			}
			return NewLineSegment(idxStart, idxPrevious);
		} else if ((!longLineMode) && (pd->Distance() > glbConfig.LongLineDistance)) {
			longLineMode = true;
			vPrev = NewVector(idxStart, pd->PIndex());
			vPrev->Norm();
			if (glbConfig.Verbose) {
				printf("LongLingMode: %d (%d:%d) -> %d (%d:%d)\n", 
					idxStart, At(idxStart)->X(), At(idxStart)->Y(),
					pd->PIndex(), At(pd->PIndex())->X(), At(pd->PIndex())->Y());				
			}
		}
		//printf("Put to use\n");
		At(pd->PIndex())->Use();
		//printf("Put to use\n");
		idxPrevious = pd->PIndex();
	}

	if (idxPrevious == -1) {
		printf("No previous points, too few points left in cluster\n");
		return NULL;
	}
	if (glbConfig.Verbose) {
		printf("NewSegment, out of range, num dist: %d, %d -> %d, dp: %f\n", pds.size(), idxStart, idxPrevious, dp);
	}
	auto lsNew = NewLineSegment(idxStart, idxPrevious);
	return lsNew;
}

LineSegment *ContourCluster::NewLineSegment(int idxA, int idxB) {
	LineSegment *ls = new LineSegment(At(idxA)->Pt(), At(idxB)->Pt(), idxA, idxB);
	return ls;
}

void ContourCluster::CalcPointDistance(int pidx, std::vector<PointDistance *> &distances) {
	return LocalSearchPointDistance(pidx, distances);
	//return FullSearchPointDistance(pidx, distances);
}

void ContourCluster::FullSearchPointDistance(int pidx, std::vector<PointDistance *> &distances) {
	//	Full range search - no block optimization, all points still in cluster taken into account
	auto porigin = At(pidx);
	//distances := make([]PointDistance, 0)
	for (int i = 0; i < Len(); i++) {
		if (i == pidx) {
			continue;
		}

		if (At(i)->IsUsed()) {
			continue;
		}

		float dist = porigin->Distance(At(i));
		auto pdist = new PointDistance(porigin->Distance(At(i)), At(i)->PIndex());
		// if (i < 10) {
		// 	printf("%d, (%d,%d) -> (%d, %d), dist: %f\n", i, porigin->X(), porigin->Y(),At(i)->X(), At(i)->Y(), dist);
		// }
		//pdist := PointDistance{
		// 	Distance: porigin.Distance(cluster.At(i)),
		// 	PIndex:   cluster.At(i).PIndex,
		// }
		// distances = append(distances, pdist)
		distances.push_back(pdist);
	}
}


/*
  UL | U | UR
  -----------
   L | x | R
  -----------
  DL | D | DR
*/
void ContourCluster::LocalSearchPointDistance(int pidx, std::vector<PointDistance *> &distances) {
	auto porigin = At(pidx);
	auto blockOrigin = porigin->GetBlock();

	blockOrigin->CalcPointDistance(porigin, distances);

	auto left = blockmap->Left(blockOrigin);
	if (left != NULL) {
		left->CalcPointDistance(porigin, distances);
	}
	auto right = blockmap->Right(blockOrigin);
	if (right != NULL) {
		right->CalcPointDistance(porigin, distances);
	}

	auto up = blockmap->Up(blockOrigin);
	if (up != NULL) {
		up->CalcPointDistance(porigin,distances);

		// do UL
		left = blockmap->Left(up);
		if (left != NULL) {
			left->CalcPointDistance(porigin, distances);
		}

		// do UR
		right = blockmap->Right(up);
		if (right != NULL) {
			right->CalcPointDistance(porigin, distances);
		}
	}

	auto down = blockmap->Down(blockOrigin);
	if (down != NULL) {
		down->CalcPointDistance(porigin,distances);

		// DL
		left = blockmap->Left(down);
		if (left != NULL) {
			left->CalcPointDistance(porigin, distances);
		}
		// DR
		right = blockmap->Right(down);
		if (right != NULL) {
			right->CalcPointDistance(porigin, distances);
		}
	}
}

Vec2D *ContourCluster::NewVector(int idxA, int idxB) {
	return new Vec2D(At(idxA)->Pt(), At(idxB)->Pt());	
}



//
// Implementation of block class
//

Block::Block(Bitmap *bitmap, int x, int y) {
	this->bitmap = bitmap;
	this->x = x;
	this->y = y;
	this->visited = false;
	this->extracted = false;
}
uint8_t Block::ReadGreyPixel(int x, int y){
	return bitmap->Buffer(x,y)[0];
}
float Block::PixelAsFloat(int x, int y) {
	uint8_t c = bitmap->Buffer(x,y)[0];
	return (float)c;
}
int Block::HashFunc(int x, int y) {
	return x + y * 1024;
}
int Block::Hash() {
	return HashFunc(x,y);
}
int Block::Left() {
	return HashFunc(x - glbConfig.BlockSize, y);
}
int Block::Right() {
	return HashFunc(x + glbConfig.BlockSize, y);
}
int Block::Up() {
	return HashFunc(x, y - glbConfig.BlockSize);
}
int Block::Down() {
	return HashFunc(x, y + glbConfig.BlockSize);
}

void Block::Scan(std::vector<ContourPoint *> &pnts) {
	for (int y=0;y<(glbConfig.BlockSize + 1);y++) {
		for (int x=0;x<(glbConfig.BlockSize + 1);x++) {
			// Check if pixel's is within bounds
			if (!bitmap->Inside(this->x + x, this->y + y)) continue;
			if (!bitmap->Inside(this->x + x - 1, this->y + y)) continue;
			if (!bitmap->Inside(this->x + x, this->y + y - 1)) continue;

			float c = PixelAsFloat(this->x + x, this->y + y);
			float l = PixelAsFloat(this->x + x - 1, this->y + y);
			float u = PixelAsFloat(this->x + x, this->y+y -1);

			// delta_x := math.Abs(float64(l.Y) - float64(c.Y))
			// delta_y := math.Abs(float64(u.Y) - float64(c.Y))

			float delta_x = fabs(l - c);
			float delta_y = fabs(u - c);

			if ((delta_x > glbConfig.GreyThresholdLevel) || (delta_y > glbConfig.GreyThresholdLevel)) {
				ContourPoint *cpt = new ContourPoint(this->x + x, this->y + y, this);
				pnts.push_back(cpt);
				this->points.push_back(cpt);	// Need local copy for optimized search!
			}
		}
	}
}

void Block::CalcPointDistance(ContourPoint *cp, std::vector<PointDistance *> &distances) {
	for (int i=0;i<this->points.size();i++) {
		ContourPoint *pt = this->points[i];
		if (pt->PIndex() == cp->PIndex()) {
			continue;
		}

		if (pt->IsUsed()) {
			continue;
		}

		float dist = cp->Distance(pt);
		PointDistance *pd = new PointDistance(dist, pt->PIndex());
		distances.push_back(pd);
	}
}


//
// Blockmap
//

BlockMap::BlockMap(Bitmap *bitmap) {
	this->bitmap = bitmap;
	BuildBlocks();
}
Block *BlockMap::GetBlockForExtraction(Block *previous /*= NULL*/) {
	// TODO: This should only be done for the first one, otherwise recursive travel
	if (previous == NULL) {
		auto it = blocks.begin();
		while (it != blocks.end()) {
			Block *b = it->second;
			if ((!b->IsExtracted()) && (b->NumPoints() > 0)) {
				return b;
			}
			it++;
		}		
	} else {
		return GetBlockForExtractionRecursive(previous);
	}
	return NULL;
}
Block *BlockMap::GetBlockForExtractionRecursive(Block *previous) {
	auto next = Right(previous);
	if ((next != NULL) && (next->IsExtracted() != true) && (next->NumPoints() > 0)) {
		return next;
	}
	next = Down(previous);
	if ((next != NULL) && (next->IsExtracted() != true) && (next->NumPoints() > 0)) {
		return next;
	}
	next = Left(previous);
	if ((next != NULL) && (next->IsExtracted() != true) && (next->NumPoints() > 0)) {
		return next;
	}
	return GetBlockForExtraction(NULL);
}

void BlockMap::BuildBlocks() {
	for (int y=0;y<this->bitmap->Height()/glbConfig.BlockSize;y++) {
		for (int x=0;x<this->bitmap->Width()/glbConfig.BlockSize;x++) {
			auto block = new Block(this->bitmap, x * glbConfig.BlockSize, y * glbConfig.BlockSize);
			blocks[block->Hash()] = block;
		}
	}
}


Block *BlockMap::GetBlock(int hashValue) {
	auto it = blocks.find(hashValue);
	if (it == blocks.end()) {
		return NULL;
	}
	return it->second; 
}
Block *BlockMap::Left(Block *block) {
	return GetBlock(block->Left());
}
Block *BlockMap::Right(Block *block) {
	return GetBlock(block->Right());
}
Block *BlockMap::Up(Block *block) {
	return GetBlock(block->Up());
}
Block *BlockMap::Down(Block *block) {
	return GetBlock(block->Down());
}

void BlockMap::Scan(std::vector<ContourPoint *> &points, Block *b) {
	if (b->IsVisited()) {
		return;
	}
	b->Visit();
	b->Scan(points);

	auto left = Left(b);
	if ((left != NULL) && (!left->IsVisited())) {
		Scan(points, left);
	}

	auto right = Right(b);
	if ((right != NULL) && (!right->IsVisited())) {
		Scan(points, right);
	}

	auto up = Up(b);
	if ((up != NULL) && (!up->IsVisited())) {
		Scan(points,up);
	}

	auto down = Down(b);
	if ((down != NULL) && (!down->IsVisited())) {
		Scan(points,down);
	}
}

std::vector<ContourPoint *> BlockMap::ExtractContourPoints()  {
	std::vector<ContourPoint *> points;

	auto it = blocks.begin();
	while (it != blocks.end()) {
		Block *b = it->second;
		if (!b->IsVisited()) {
			Scan(points, b);
		}
		it++;
	}
	// This can probably be done while iterating through a global variable...
	// But let's try to avoid globals, shall we...
	int i;
	for (i=0;i<points.size();i++) {
		points[i]->SetPIndex(i);
	}

	printf("ContourPoints: %d,%d\n", points.size(),i);
	return points;
}

//
// Contour Point
//

ContourPoint::ContourPoint(int x, int y, Block *b) :
	pt(x,y)
{
	this->pindex = -1;
	this->block = b;
	this->used = false;
}

static float VecLen(Point *a, Point *b) {
	float dx = (float)(b->X() - a->X());
	float dy = (float)(b->Y() - a->Y());
	return sqrt(dx*dx + dy*dy);
}


float ContourPoint::Distance(ContourPoint *pOther) {
	//printf("  ContourPoint::Distance, (%d:%d) -> (%d:%d)\n",X(), Y(), pOther->X(), pOther->Y());
	return VecLen(&pt, &pOther->pt);
}












































