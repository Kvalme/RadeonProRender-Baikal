/*****************************************************************************\
*
*  Module Name    FireRender_GL.h
*  Project        FireRender Engine OpenGL Interop API
*
*  Description    Fire Render Engine OpenGL Interop header
*
*  Copyright 2011 - 2013 Advanced Micro Devices, Inc.
*
*  All rights reserved.  This notice is intended as a precaution against
*  inadvertent publication and does not imply publication or any waiver
*  of confidentiality.  The year included in the foregoing notice is the
*  year of creation of the work.
*  @author Dmitry Kozlov (dmitry.kozlov@amd.com)
*  @author Takahiro Harada (takahiro.harada@amd.com)
*  @bug No known bugs.
*
\*****************************************************************************/
#ifndef __RADEONPRORENDER_VK_H
#define __RADEONPRORENDER_VK_H

#ifdef __cplusplus
extern "C" {
#endif

/* rpr_creation_flags */
#define RPR_CREATION_FLAGS_ENABLE_VK_INTEROP (1<<11)

/* rpr_framebuffer_properties */
#define RPR_VK_IMAGE_OBJECT 0x5001
#define RPR_VK_IMAGE_VIEW_OBJECT 0x5002
#define RPR_VK_SEMAPHORE_OBJECT 0x5003

/* rpr_context_properties names */
#define RPR_VK_INSTANCE (void*)0x1
#define RPR_VK_DEVICE (void*)0x2
#define RPR_VK_PHYSICAL_DEVICE (void*)0x3


#ifdef __cplusplus
}
#endif

#endif  /*__RADEONPRORENDER_GL_H  */
