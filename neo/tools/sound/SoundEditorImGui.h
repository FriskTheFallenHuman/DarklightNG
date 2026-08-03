#ifndef __SOUND_EDITOR_IMGUI_H__
#define __SOUND_EDITOR_IMGUI_H__

void SoundEditorImGuiShow( const char *soundShader = NULL, const idDict *spawnArgs = NULL );
void SoundEditorImGuiHide();
bool SoundEditorImGuiIsOpen();
void SoundEditorImGuiRender();
void SoundEditorImGuiShutdown();

#endif
