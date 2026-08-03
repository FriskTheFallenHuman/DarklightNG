#ifndef __PARTICLE_EDITOR_IMGUI_H__
#define __PARTICLE_EDITOR_IMGUI_H__

void ParticleEditorImGuiShow( const char *particleName = NULL );
void ParticleEditorImGuiHide();
bool ParticleEditorImGuiIsOpen();
void ParticleEditorImGuiRender();
void ParticleEditorImGuiShutdown();

#endif
