#include "DummyPlugin.h"
#include "StimApp.h"
#include "StimGL_SpikeGL_Integration.h"

DummyPlugin::DummyPlugin()
    : StimPlugin("DummyPlugin"), grid_w(0)
{
    StimParams p;
    p["ftrackbox_w"] = "0";
    setParams(p);
}

DummyPlugin::~DummyPlugin() {}

bool DummyPlugin::init()
{
    grid_w = 0;
    getParam("grid_w", grid_w);
    return StimPlugin::init();
}

void DummyPlugin::cleanup()
{
    StimPlugin::cleanup();
}


void DummyPlugin::drawFrame()
{
    GLfloat c[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE,c);
    glClearColor(0.5,0.5,0.5,0.5);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(c[0],c[1],c[2],c[3]);

    if (grid_w > 0) {
        const int w = width(), h = height();
        glLineWidth(1);
        //glLineStipple(1,0xf0f0);
        //glEnable(GL_LINE_STIPPLE);
        const int factor = frameNum/*/4*/;
        glColor3f(.6f,float(factor%2)/12.f + .7f,float(factor%2)*.2f + .6f);
        glBegin(GL_LINES);
        for (int i = 0; i*grid_w < w; ++i ) {
            glVertex2f(i*grid_w, 0);
            glVertex2f(i*grid_w, h);
        }
        for (int i = 0; i*grid_w < h; ++i ) {
            glVertex2f(0, i*grid_w);
            glVertex2f(w, i*grid_w);
        }
        glEnd();
        //glDisable(GL_LINE_STIPPLE);
    }
}

