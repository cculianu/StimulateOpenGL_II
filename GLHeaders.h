/*
 *  GLHeaders.h
 *  StimulateOpenGL_II
 *
 *  Created by calin on 3/11/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef GLHeaders_H
#define GLHeaders_H
#define GL_GLEXT_PROTOTYPES
#include <QGLContext>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#  include <glext.h>
#  include <glu.h>
#else
#  include <GL/gl.h>
#  ifndef Q_OS_WIN
#    include <GL/glext.h>
#  endif
#  include <GL/glu.h>
#endif
#endif

