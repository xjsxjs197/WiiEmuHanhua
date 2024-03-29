/*
Rice_GX - OGLGraphicsContext.h
Copyright (C) 2003 Rice1964
Copyright (C) 2010, 2011, 2012 sepp256 (Port to Wii/Gamecube/PS3)
Wii64 homepage: http://www.emulatemii.com
email address: sepp256@gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _OGL_CONTEXT_H_
#define _OGL_CONTEXT_H_

class COGLGraphicsContext : public CGraphicsContext
{
    friend class OGLRender;
    friend class COGLRenderTexture;
public:
    virtual ~COGLGraphicsContext();

    bool Initialize(HWND hWnd, HWND hWndStatus, uint32 dwWidth, uint32 dwHeight, BOOL bWindowed );
    void CleanUp();
    void Clear(ClearFlag dwFlags, uint32 color=0xFF000000, float depth=1.0f);

    void UpdateFrame(bool swaponly=false);
    int ToggleFullscreen();     // return 0 as the result is windowed

    bool IsExtensionSupported(const char* pExtName);
    bool IsWglExtensionSupported(const char* pExtName);
    static void InitDeviceParameters();

protected:
    friend class OGLDeviceBuilder;
    COGLGraphicsContext();
    void InitState(void);
    void InitOGLExtension(void);
    bool SetFullscreenMode();
    bool SetWindowMode();

#ifndef __GX__
	//TODO: make this a pointer to the xfb?
    SDL_Surface *m_pScreen;
#endif //!__GX__

    // Important OGL extension features
    bool    m_bSupportMultiTexture;
    bool    m_bSupportTextureEnvCombine;
    bool    m_bSupportSeparateSpecularColor;
    bool    m_bSupportSecondColor;
    bool    m_bSupportFogCoord;
    bool    m_bSupportTextureObject;

    // Optional OGL extension features;
    bool    m_bSupportRescaleNormal;
    bool    m_bSupportLODBias;

    // Nvidia OGL only features
    bool    m_bSupportTextureMirrorRepeat;
    bool    m_bSupportTextureLOD;
    bool    m_bSupportNVRegisterCombiner;
    bool    m_bSupportBlendColor;
    bool    m_bSupportBlendSubtract;
    bool    m_bSupportNVTextureEnvCombine4;
    
    // Minimal requirements, I will even not check them at runtime
    //bool  m_bSupportTextureEnvAdd;
    //bool  m_bSupportVertexArray;

    const unsigned char*    m_pVendorStr;
    const unsigned char*    m_pRenderStr;
    const unsigned char*    m_pExtensionStr;
    const char* m_pWglExtensionStr;
    const unsigned char*    m_pVersionStr;
};

#endif



