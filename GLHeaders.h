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
/* These #defines are needed because the stupid MinGW headers are 
 OpenGL 1.1 only.. but the actual Windows lib has OpenGL 2               */
#ifndef GL_BGRA
#define GL_BGRA                           0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR                            0x80E0
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#endif
#ifndef GL_UNSIGNED_BYTE_3_3_2
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#endif
#ifndef GL_UNSIGNED_BYTE_2_3_3_REV
#define GL_UNSIGNED_BYTE_2_3_3_REV        0x8362
#endif
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB          0x84F5
#endif
#ifndef GL_BUFFER_SIZE_ARB 
#define GL_BUFFER_SIZE_ARB                0x8764
#endif
#ifdef Q_OS_WIN /* Hack for now to get windows to see the framebuffer ext stuff */
#if defined(__GNUC__ ) || defined(_MSC_VER)
#ifndef GLAPI
#define GLAPI 
#endif
#define GL_MAX_COLOR_ATTACHMENTS_EXT      0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT          0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT          0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT          0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT          0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT          0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT          0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT          0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT          0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT          0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT         0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT         0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT         0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT         0x8CED
#define GL_COLOR_ATTACHMENT14_EXT         0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT         0x8CEF
#define GL_FRAMEBUFFER_EXT                0x8D40
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT    0x8CDD
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
extern "C" {
 GLAPI void APIENTRY glDeleteFramebuffersEXT (GLsizei, const GLuint *);
 GLAPI void APIENTRY glGenFramebuffersEXT (GLsizei, GLuint *);
 GLAPI GLenum APIENTRY glCheckFramebufferStatusEXT (GLenum);
 GLAPI void APIENTRY glFramebufferTexture2DEXT (GLenum, GLenum, GLenum, GLuint, GLint);
 GLAPI void APIENTRY glGenerateMipmapEXT (GLenum);
 GLAPI void APIENTRY glBindFramebufferEXT (GLenum, GLuint);
}

#endif // __GNUC__
#endif // Q_OS_WIN

