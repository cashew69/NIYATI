//#include "camera_base.h"
#include "engine/engine.h"
#include "platform.h"
#include "structs.h"
#include "utils/scenegraph.h"
#include <cstdio>

#include "tempScene.cpp"

void projectInit() {
    LOG_I("Project Initializing...");

    scene1Init();
    InitCustomCameras();

}

void projectUpdate() {

    scene1Update();

}
void projectRender() {

    scene1Display();

}

void projectCleanup()
{



}
