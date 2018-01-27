#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>

#include <imgui.h>
#include "imgui_impl_glfw_gl2.h"
#include "animation.h"
#include "ui.h"

using namespace gnilk;

void UIWindow::Open() {
    window = glfwCreateWindow(1280, 1020, "ImGui OpenGL3 example", NULL, NULL);
    glfwMakeContextCurrent(window);    
    glfwSwapInterval(1); // Enable vsync

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // Setup style
    ImGui::StyleColorsClassic();
}

void UIWindow::AttachController(UIBaseWindow *controller) {
    controllers.push_back(controller);
}

bool show_demo_window = true;
bool show_another_window = false;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

void UIWindow::MakeCurrent() {
    glfwMakeContextCurrent(window);        
}
void UIWindow::Render() {
    glfwMakeContextCurrent(window);    
    ImGui_ImplGlfwGL2_NewFrame();


    for (int i=0;i<controllers.size();i++) {
        controllers[i]->Render();
    }

        // ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats as a color
        // if (ImGui::Button("Demo Window"))                       // Use buttons to toggle our bools. We could use Checkbox() as well.
        //     show_demo_window ^= 1;
        // if (ImGui::Button("Another Window")) 
        //     show_another_window ^= 1;
//        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

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


///////////
//
// Implementation of the render control window
//
UIRenderView::UIRenderView(AnimController *controller) {
    this->controller = controller;
}
void UIRenderView::Render() {
    // 1. Show a simple window.
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets automatically appears in a window called "Debug".
    ImGui::Begin("Render Control");
    {
        static float f = 0.0f;
        static int nStrips = 0;
        ImGui::Text("Hello, world!");                           // Some text (you can use a format string too)

        // Other stuff
        ImGui::Checkbox("Highlight First Point", &controller->animRenderVars.highLightFirstInStrip);
        if (ImGui::Button("Reload Data")) {
            if (this->controller != NULL) {
                this->controller->ReloadData();

            }
        }

        // Do frame control
        ImGui::Text("Frame");
        ImGui::SameLine();
        ImGui::SliderInt("##Frame", &controller->animRenderVars.idxFrame, 0, this->controller->GetMaxFrames()-1);
        ImGui::SameLine();
        if (ImGui::Button("Prev##Frame")) {
            if (controller->animRenderVars.idxFrame > 0) {
                controller->animRenderVars.idxFrame -= 1;
                controller->animRenderVars.idxLineInStrip = 0;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Next##Frame")) {
            if (controller->animRenderVars.idxFrame <= this->controller->GetMaxFrames()) {
                controller->animRenderVars.idxFrame += 1;
                controller->animRenderVars.idxLineInStrip = 0;
            }
        }
        //printf("NStrips\n");
        int numStrips = this->controller->GetMaxStrips(controller->animRenderVars.idxFrame);
        if (numStrips > 0) {
            //printf("bla\n");
            // Strip control
            ImGui::Text("Strips in Frame: %d", numStrips);
            ImGui::Text("Strips");
            ImGui::SameLine();
            ImGui::SliderInt("##Strip", &controller->animRenderVars.numStrips, 0, numStrips-1);
            ImGui::SameLine();
            if (ImGui::Button("Prev##Strip")) {
                printf("StripPrev\n");
                if (controller->animRenderVars.numStrips > 0) {
                    controller->animRenderVars.numStrips -= 1;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Next##Strip")) {
                if (controller->animRenderVars.numStrips <= numStrips) {
                    controller->animRenderVars.numStrips += 1;
                }
            }

            // Line Control
            ImGui::Text("Strip");
            ImGui::SameLine();
            ImGui::SliderInt("##StripFocus", &controller->animRenderVars.idxStrip, 0, numStrips-1);
            ImGui::SameLine();
            if (ImGui::Button("Prev##StripFocus")) {
                if (controller->animRenderVars.idxStrip > 0) {
                    controller->animRenderVars.idxStrip -= 1;
                    controller->animRenderVars.idxLineInStrip = 0;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Next##StripFocus")) {
                if (controller->animRenderVars.idxStrip <= numStrips) {
                    controller->animRenderVars.idxStrip += 1;
                    controller->animRenderVars.idxLineInStrip = 0;
                }
            }

            int idxStrip = controller->animRenderVars.idxStrip;
            if (idxStrip > numStrips) {
                idxStrip = numStrips-1;
            }

            //printf("idxStrip: %d\n", idxStrip);


            int numLinesInStrip = this->controller->GetMaxPoints(controller->animRenderVars.idxFrame, idxStrip); 
            ImGui::Text("Points in Strip: %d", numLinesInStrip);
            ImGui::Text("Lines");
            ImGui::SameLine();
            ImGui::SliderInt("##Line", &controller->animRenderVars.idxLineInStrip, 0, numLinesInStrip);
            ImGui::SameLine();
            if (ImGui::Button("Prev##Line")) {
                if (controller->animRenderVars.idxLineInStrip > 0) {
                    controller->animRenderVars.idxLineInStrip -= 1;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Next##Line")) {
                if (controller->animRenderVars.idxLineInStrip <= numLinesInStrip) {
                    controller->animRenderVars.idxLineInStrip += 1;
                }
            }

            // Point ptStart = animation.At(animRenderVars.idxFrame)->At(idxStrip).At(animRenderVars.idxLineInStrip);
            // Point ptEnd = animation.At(animRenderVars.idxFrame)->At(idxStrip).At(animRenderVars.idxLineInStrip+1);

            // float vLen = animation.At(animRenderVars.idxFrame)->At(idxStrip).Len(animRenderVars.idxLineInStrip, animRenderVars.idxLineInStrip+1);
            // float iLen = animation.At(animRenderVars.idxFrame)->At(idxStrip).FixLen(animRenderVars.idxLineInStrip, animRenderVars.idxLineInStrip+1);

            // ImGui::Text("Line, %d -> %d, length: %f (%f)", 0,0,vLen,iLen);

        }
    }
    ImGui::End();

}

///////////////////

UIGenerateView::UIGenerateView(GenerateController *controller) {
    this->controller = controller;
}

#define TAGNAME(_n_) ("##" # _n_)
static void MakeIntSlider(const char *name, int *var, int minval, int maxval, const char *helptext = NULL);
static void MakeFloatSlider(const char *name, float *var, float minval, float maxval, const char *helptext = NULL);


static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void MakeIntSlider(const char *name, int *var, int minval, int maxval, const char *helptext) {
    ImGui::Text("%s",name);
    if (helptext != NULL) {
        ImGui::SameLine(); 
        ShowHelpMarker(helptext);        
    }

    char tmp[256];
    snprintf(tmp,256,"##%s_SliderInt",name);

    ImGui::SameLine();
    ImGui::SliderInt(tmp, var, minval, maxval);
    ImGui::SameLine();

    snprintf(tmp,256,"<##%sPrev",name);
    if (ImGui::Button(tmp)) {
        if (*var > minval) {
            *var -= 1;
        }
    }
    ImGui::SameLine();
    snprintf(tmp,256,">##%sNext",name);
    if (ImGui::Button(tmp)) {
        if (*var < maxval) {
            *var += 1;
        }
    }
}

static void MakeFloatSlider(const char *name, float *var, float minval, float maxval, const char *helptext) {
    ImGui::Text("%s",name);
    if (helptext != NULL) {
        ImGui::SameLine(); 
        ShowHelpMarker(helptext);        
    }

    char tmp[256];
    snprintf(tmp,256,"##%s_SliderFloat",name);

    ImGui::SameLine();
    ImGui::SliderFloat(tmp, var, minval, maxval);
    ImGui::SameLine();

    snprintf(tmp,256,"<##%s_Prev",name);
    if (ImGui::Button(tmp)) {
        if (*var > minval) {
            *var -= 1;
        }
    }
    ImGui::SameLine();
    snprintf(tmp,256,">##%s_Next",name);
    if (ImGui::Button(tmp)) {
        if (*var < maxval) {
            *var += 1;
        }
    }

}


void UIGenerateView::Render() {
    ImGui::Begin("Generate Data");

    if (ImGui::Button("Read defaults")) {
        this->controller->ReadDefaultSettings();
    }

    if (ImGui::Button("Reset Options")) {
        this->controller->ResetParameters();
    }

    ImGui::Text("Options:");
    // this should be pretty simple to make generic
    MakeIntSlider("gl", &controller->gl, 0, 255, "Grey threshold Level\nDetection level when scanning bitmap for contour/outline");
    MakeIntSlider("bs", &controller->bs, 8, 128, "Blocksize\nLimit neighbour search within surrounding blocks");   // This should be in steps of 8, but don't care
    MakeFloatSlider("cnt", &controller->cnt,0.0,255.0, "Contrast Factor\nImage contrast enhanced when loading, like:\n  luma = pow(luma,cnt) * cns\nSet to '1' to disable");
    MakeFloatSlider("cns", &controller->cns,0.0,255.0, "Contrast Scale\nImage contrast enhanced when loading, like:\n  luma = pow(luma,cnt) * cns\nSet to '1' to disable");
    MakeFloatSlider("lcd", &controller->lcd,0.0,255.0, "Line Cutoff Distance\nBreak condition for new line segments when traversing contour");
    MakeFloatSlider("lca", &controller->lca,0.0,1.0, "Line Cutoff Angle\nBreak condition for new segment: default 0.5");
    MakeFloatSlider("lld", &controller->lld,0.0,255.0, "Long Line Distance\nBreak condition searching for reference vector");
    MakeFloatSlider("ccd", &controller->ccd,0.0,255.0, "Cluster Cutoff Distance\nBreak condition for new cluster");
    MakeFloatSlider("oca", &controller->oca,0.0,1.0, "Optimization Cutoff Angle\nBreak condition when merging line segments to strips");

    if (ImGui::Button("Generate")) {
        this->controller->GenerateData();
    }
    ImGui::End();
}

UIImageView::UIImageView(ImageController *controller) {
    this->controller = controller;
}
void UIImageView::Render() {
    ImGui::Begin("Images");
    for(int i=0;i<controller->ImageCount();i++) {
        OpenGLImage *image = controller->GetImage(i);
        ImGui::Image((ImTextureID)image->GetTextureID(), ImVec2(image->Width(), image->Height()));
            //, ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(255,255,255,128));        
    }
    ImGui::End();
}























