#ifndef __DRAW_VFX_H__
#define __DRAW_VFX_H__

void RB_VFX_Init();
void RB_VFX_Shutdown();
void RB_VFX_BeginFrame();

// Uploads a camera-facing particle surface into the one shared VFX VBO/IBO,
// binds both objects, and returns byte offsets for vertex attributes/indexes.
bool RB_VFX_BindSurface( const srfTriangles_t *tri, int &vertexOffset, int &indexOffset );

#endif
