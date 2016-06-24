#ifndef DUMMYPLUGIN_H
#define DUMMYPLUGIN_H

#include <QObject>
#include "StimPlugin.h"

class DummyPlugin : public StimPlugin
{
    Q_OBJECT
    friend class GLWindow;
    DummyPlugin();
    ~DummyPlugin();

public:
    QString description() const { return "Used internally by the HotSpot Config and Warp Config editors."; }

protected:
    /// Draws a gray color to the screen.
    void drawFrame();
    /// Reimplemented from super.
    bool init();
    /// Reimplemented from super.
    void cleanup();

    /// reimplemented from parent to suppress notify
    virtual void notifySpikeGLAboutStart() {}
    virtual void notifySpikeGLAboutStop() {}
    virtual void notifySpikeGLAboutParams() {}
private:
    int grid_w;
};

#endif // DUMMYPLUGIN_H


