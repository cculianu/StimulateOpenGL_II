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
#ifdef Q_OS_DARWIN
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
//#if defined(__GNUC__ ) || defined(_MSC_VER)
#ifndef GLAPI
#define GLAPI 
#endif
#ifndef GL_MAX_COLOR_ATTACHMENTS_EXT
#define GL_MAX_COLOR_ATTACHMENTS_EXT      0x8CDF
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#endif
#ifndef GL_COLOR_ATTACHMENT1_EXT
#define GL_COLOR_ATTACHMENT1_EXT          0x8CE1
#endif
#ifndef GL_COLOR_ATTACHMENT2_EXT
#define GL_COLOR_ATTACHMENT2_EXT          0x8CE2
#endif
#ifndef GL_COLOR_ATTACHMENT3_EXT
#define GL_COLOR_ATTACHMENT3_EXT          0x8CE3
#endif
#ifndef GL_COLOR_ATTACHMENT4_EXT
#define GL_COLOR_ATTACHMENT4_EXT          0x8CE4
#endif
#ifndef GL_COLOR_ATTACHMENT5_EXT
#define GL_COLOR_ATTACHMENT5_EXT          0x8CE5
#endif
#ifndef GL_COLOR_ATTACHMENT6_EXT
#define GL_COLOR_ATTACHMENT6_EXT          0x8CE6
#endif
#ifndef GL_COLOR_ATTACHMENT7_EXT
#define GL_COLOR_ATTACHMENT7_EXT          0x8CE7
#endif
#ifndef GL_COLOR_ATTACHMENT8_EXT
#define GL_COLOR_ATTACHMENT8_EXT          0x8CE8
#endif
#ifndef GL_COLOR_ATTACHMENT9_EXT
#define GL_COLOR_ATTACHMENT9_EXT          0x8CE9
#endif
#ifndef GL_COLOR_ATTACHMENT10_EXT
#define GL_COLOR_ATTACHMENT10_EXT         0x8CEA
#endif
#ifndef GL_COLOR_ATTACHMENT11_EXT
#define GL_COLOR_ATTACHMENT11_EXT         0x8CEB
#endif
#ifndef GL_COLOR_ATTACHMENT12_EXT
#define GL_COLOR_ATTACHMENT12_EXT         0x8CEC
#endif
#ifndef GL_COLOR_ATTACHMENT13_EXT
#define GL_COLOR_ATTACHMENT13_EXT         0x8CED
#endif
#ifndef GL_COLOR_ATTACHMENT14_EXT
#define GL_COLOR_ATTACHMENT14_EXT         0x8CEE
#endif
#ifndef GL_COLOR_ATTACHMENT15_EXT
#define GL_COLOR_ATTACHMENT15_EXT         0x8CEF
#endif
#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT                0x8D40
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED_EXT
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT    0x8CDD
#endif
#ifndef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
#endif
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER              0x88EB
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY                      0x88B8
#endif
#ifndef GL_STREAM_READ
#define GL_STREAM_READ                    0x88E1
#endif
#ifndef GL_DYNAMIC_READ
#define GL_DYNAMIC_READ                   0x88E9
#endif
extern "C" {
 GLAPI void APIENTRY glDeleteFramebuffersEXT (GLsizei, const GLuint *);
 GLAPI void APIENTRY glGenFramebuffersEXT (GLsizei, GLuint *);
 GLAPI GLenum APIENTRY glCheckFramebufferStatusEXT (GLenum);
 GLAPI void APIENTRY glFramebufferTexture2DEXT (GLenum, GLenum, GLenum, GLuint, GLint);
 GLAPI void APIENTRY glGenerateMipmapEXT (GLenum);
 GLAPI void APIENTRY glBindFramebufferEXT (GLenum, GLuint);
	
 // PBO-stuff
 GLAPI void APIENTRY glGenBuffers(GLsizei n, GLuint *buffers);
 GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers);
 GLAPI void APIENTRY glBindBuffer(GLenum target, GLuint buffer);
 GLAPI void APIENTRY glBufferData(GLenum target, GLsizei size, const GLvoid * data, GLenum usage);
 GLAPI void * APIENTRY glMapBuffer(GLenum target, GLenum access);	
 GLAPI GLboolean APIENTRY glUnmapBuffer(GLenum target);
}

//#endif // __GNUC__
#endif // Q_OS_WIN

#endif // GLHeaders_H

