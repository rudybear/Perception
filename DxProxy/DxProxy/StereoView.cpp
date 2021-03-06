/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

File <StereoView.cpp> and
Class <StereoView> :
Copyright (C) 2012 Andres Hernandez

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#include "StereoView.h"
#include "Vireio.h"

using namespace vireio;

/**
* Tiny debug helper.
* Outputs debug info if object reference counter is not zero when release.
***/
inline void releaseCheck(char* object, int newRefCount)
{
//#ifdef _DEBUG
	if (newRefCount > 0) {
		debugf("Error: %s count = %d\n", object, newRefCount);
	}
//#endif
}

/**
* Constructor.
* Sets game configuration data. Sets all member pointers to NULL to prevent uninitialized objects being used.
***/ 
StereoView::StereoView(ProxyConfig *config)	
{
	SHOW_CALL("StereoView::StereoView()\n");

	this->config = config;
	initialized = false;
	m_screenViewGlideFactor = 1.0f;
	XOffset = 0;
	game_type = config->game_type;

	m_bZBufferFilterMode = false;
	m_bZBufferVisualisationMode = false;
	m_fZBufferFilter = 0.0f;
	m_fZBufferFilter = config->zbufferDepthLow;

	chromaticAberrationCorrection = true;
	m_vignetteStyle = NONE;
	m_rotation = 0.0f;
	m_blackSmearCorrection = 0.0f;
	m_mousePos.x = 0;
	m_mousePos.y = 0;
	ZoomOutScale = 1.0f;
	// set all member pointers to NULL to prevent uninitialized objects being used
	m_pActualDevice = NULL;
	backBuffer = NULL;
	leftTexture = NULL;
	rightTexture = NULL;
	
	leftSurface = NULL;
	rightSurface = NULL;	

	screenVertexBuffer = NULL;
	lastVertexShader = NULL;
	lastPixelShader = NULL;
	lastTexture = NULL;
	lastTexture1 = NULL;
	lastVertexDeclaration = NULL;
	lastRenderTarget0 = NULL;
	lastRenderTarget1 = NULL;
	viewEffect = NULL;
	sb = NULL;
	m_bLeftSideActive = false;

	//Start in DSV mode
	m_disconnectedScreenView = true;

	int value = 0;
	if (ProxyHelper::ParseGameType(config->game_type, ProxyHelper::StateBlockSaveRestoreType, value))
	{
		howToSaveRenderStates = (HowToSaveRenderStates)(value);
	}
	else
	{
		howToSaveRenderStates = HowToSaveRenderStates::STATE_BLOCK;
	}

	if(howToSaveRenderStates == HowToSaveRenderStates::ALL_STATES_MANUALLY)
	{
		OutputDebugString( "HowToSaveRenderStates = ALL_STATES_MANUALLY");
	}
	else if(howToSaveRenderStates == HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE)
	{
		OutputDebugString( "HowToSaveRenderStates = DO_NOT_SAVE_AND_RESTORE");
	}
	else if(howToSaveRenderStates == HowToSaveRenderStates::SELECTED_STATES_MANUALLY)
	{
		OutputDebugString( "HowToSaveRenderStates = SELECTED_STATES_MANUALLY");
	}
	else if(howToSaveRenderStates == HowToSaveRenderStates::STATE_BLOCK)
	{
		OutputDebugString( "HowToSaveRenderStates = STATE_BLOCK");
	}

}

/**
* Empty destructor.
***/
StereoView::~StereoView()
{
	SHOW_CALL("StereoView::~StereoView()");
}

/**
* StereoView init.
* Must be initialised with an actual device. Not a wrapped device.
***/
void StereoView::Init(IDirect3DDevice9* pActualDevice)
{
	SHOW_CALL("StereoView::Init");

	if (initialized) {
		OutputDebugString("SteroView already Init'd\n");
		return;
	}

	m_pActualDevice = pActualDevice;
	
	InitShaderEffects();
	InitTextureBuffers();
	InitVertexBuffers();
	CalculateShaderVariables();

	initialized = true;
}

/**
* Cycle Render States
* Boolean for going backwards
***/
std::string StereoView::CycleRenderState(bool blnBackwards)
{
	if (initialized) {
		if(blnBackwards)
		{
			if(howToSaveRenderStates == HowToSaveRenderStates::ALL_STATES_MANUALLY)
			{
				howToSaveRenderStates = HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE;
				return "DO_NOT_SAVE_AND_RESTORE";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE)
			{
				howToSaveRenderStates = HowToSaveRenderStates::SELECTED_STATES_MANUALLY;
				return "SELECTED_STATES_MANUALLY";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::SELECTED_STATES_MANUALLY)
			{
				howToSaveRenderStates = HowToSaveRenderStates::STATE_BLOCK;
				return "STATE_BLOCK";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::STATE_BLOCK)
			{
				howToSaveRenderStates = HowToSaveRenderStates::ALL_STATES_MANUALLY;
				return "ALL_STATES_MANUALLY";
			}
		}
		else
		{
			if(howToSaveRenderStates == HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE)
			{
				howToSaveRenderStates = HowToSaveRenderStates::ALL_STATES_MANUALLY;
				return "ALL_STATES_MANUALLY";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::SELECTED_STATES_MANUALLY)
			{
				howToSaveRenderStates = HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE;
				return "DO_NOT_SAVE_AND_RESTORE";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::STATE_BLOCK)
			{
				howToSaveRenderStates = HowToSaveRenderStates::SELECTED_STATES_MANUALLY;
				return "SELECTED_STATES_MANUALLY";
			}
			else if(howToSaveRenderStates == HowToSaveRenderStates::ALL_STATES_MANUALLY)
			{
				howToSaveRenderStates = HowToSaveRenderStates::STATE_BLOCK;
				return "STATE_BLOCK";
			}
		}
		return "ERROR";
	}
	return "";
}

/**
* Releases all Direct3D objects.
***/
void StereoView::ReleaseEverything()
{
	SHOW_CALL("StereoView::ReleaseEverything()");

	if(!initialized)
		OutputDebugString("SteroView is already reset\n");

	if(backBuffer)
		releaseCheck("backBuffer", backBuffer->Release());	
	backBuffer = NULL;

	if(leftTexture)
		releaseCheck("leftTexture", leftTexture->Release());
	leftTexture = NULL;

	if(rightTexture)
		releaseCheck("rightTexture", rightTexture->Release());
	rightTexture = NULL;

	if(leftSurface)
		releaseCheck("leftSurface", leftSurface->Release());
	leftSurface = NULL;

	if(rightSurface)
		releaseCheck("rightSurface", rightSurface->Release());
	rightSurface = NULL;

	if(lastVertexShader)
		releaseCheck("lastVertexShader", lastVertexShader->Release());
	lastVertexShader = NULL;

	if(lastPixelShader)
		releaseCheck("lastPixelShader", lastPixelShader->Release());
	lastPixelShader = NULL;

	if(lastTexture)
		releaseCheck("lastTexture", lastTexture->Release());
	lastTexture = NULL;

	if(lastTexture1)
		releaseCheck("lastTexture1", lastTexture1->Release());
	lastTexture1 = NULL;

	if(lastVertexDeclaration)
		releaseCheck("lastVertexDeclaration", lastVertexDeclaration->Release());
	lastVertexDeclaration = NULL;

	if(lastRenderTarget0)
		releaseCheck("lastRenderTarget0", lastRenderTarget0->Release());
	lastRenderTarget0 = NULL;

	if(lastRenderTarget1)
		releaseCheck("lastRenderTarget1", lastRenderTarget1->Release());
	lastRenderTarget1 = NULL;

	if (viewEffect)
		viewEffect->OnLostDevice();

	initialized = false;
}

/**
* Draws stereoscopic frame, called before present
***/
void StereoView::PrePresent(D3D9ProxySurface* stereoCapableSurface)
{
	SHOW_CALL("StereoView::PrePresent");
	// Copy left and right surfaces to textures to use as shader input
	// TODO match aspect ratio of source in target ? 
	IDirect3DSurface9* leftImage;
	IDirect3DSurface9* rightImage;	
	
	if(m_3DReconstructionMode != 1) // Geometry
	{
		if(m_bLeftSideActive == true)
		{
			leftImage = stereoCapableSurface->getActualRight();			
		}
		else
		{
			leftImage = stereoCapableSurface->getActualLeft();			
		}
		rightImage = leftImage;
	}
	else
	{
		leftImage = stereoCapableSurface->getActualLeft();
		rightImage = stereoCapableSurface->getActualRight();
	}
	
	m_pActualDevice->StretchRect(leftImage, NULL, leftSurface, NULL, D3DTEXF_NONE);
	if (stereoCapableSurface->IsStereo())
		m_pActualDevice->StretchRect(rightImage, NULL, rightSurface, NULL, D3DTEXF_NONE);
	else
		m_pActualDevice->StretchRect(leftImage, NULL, rightSurface, NULL, D3DTEXF_NONE);

	m_pActualDevice->GetViewport(&viewport);
	D3DSURFACE_DESC pDesc = D3DSURFACE_DESC();
	m_pActualDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	backBuffer->GetDesc(&pDesc);
	
	bool setTexture = false;
	/*IDirect3DSurface9* depthSurface;
	IDirect3DSurface9* depthStencilSurface;
	IDirect3DTexture9* depthTexture; 
	IDirect3DTexture9* depthStencilTexture; 
	
	//if (FAILED(m_pActualDevice->CreateOffscreenPlainSurface (pDesc.Width, pDesc.Height, D3DFMT_D16_LOCKABLE, D3DPOOL_DEFAULT, &depthSurface, NULL))) {
	if (FAILED(m_pActualDevice->CreateTexture(pDesc.Width, pDesc.Height, 0, D3DUSAGE_RENDERTARGET, pDesc.Format, D3DPOOL_DEFAULT, &depthTexture, NULL))) {	
		OutputDebugString("CreateOffscreenPlainSurface (ZBuffer Texture) failed\n");
	}
	else
	{
		depthTexture->GetSurfaceLevel(0, &depthSurface);
		if (FAILED(m_pActualDevice->GetDepthStencilSurface (&depthStencilSurface))) {
			OutputDebugString("GetDepthStencilSurface (ZBuffer) failed\n");
		}
		else
		{
			if (FAILED(m_pActualDevice->UpdateSurface (depthStencilSurface, NULL, depthSurface, NULL))) {
				OutputDebugString("UpdateSurface (ZBuffer) failed\n");
			}
			else
			{				
				//m_pActualDevice->CreateTexture(pDesc.Width, pDesc.Height, 0, D3DUSAGE_RENDERTARGET, pDesc.Format, D3DPOOL_DEFAULT, &depthTexture, NULL);
				//depthTexture->GetSurfaceLevel(0, &depthSurface);
				m_pActualDevice->SetTexture(3, depthTexture);
				setTexture = true;
			}
		}
	}	*/
	if(!setTexture)
	{
		m_pActualDevice->SetTexture(3, leftTexture);
	}


	// how to save (backup) render states ?	
	switch(howToSaveRenderStates)
	{
	case HowToSaveRenderStates::STATE_BLOCK:
		m_pActualDevice->CreateStateBlock(D3DSBT_ALL, &sb);
		break;
	case HowToSaveRenderStates::SELECTED_STATES_MANUALLY:
		SaveState();
		break;
	case HowToSaveRenderStates::ALL_STATES_MANUALLY:
		SaveAllRenderStates(m_pActualDevice);
		SetAllRenderStatesDefault(m_pActualDevice);
		break;
	case HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE:
		break;
	}	
	

	// set states for fullscreen render
	SetState();

	// all render settings start here
	m_pActualDevice->SetFVF(D3DFVF_TEXVERTEX);
	
	// swap eyes
	if(!config->swap_eyes)
	{
		m_pActualDevice->SetTexture(0, leftTexture);
		m_pActualDevice->SetTexture(1, rightTexture);
	}
	else 
	{
		m_pActualDevice->SetTexture(0, rightTexture);
		m_pActualDevice->SetTexture(1, leftTexture);		
	}
		
	if (FAILED(m_pActualDevice->SetRenderTarget(0, backBuffer))) {
		OutputDebugString("SetRenderTarget backbuffer failed\n");
	}

	if (FAILED(m_pActualDevice->SetStreamSource(0, screenVertexBuffer, 0, sizeof(TEXVERTEX)))) {
		OutputDebugString("SetStreamSource failed\n");
	}

	UINT iPass, cPasses;

	if (FAILED(viewEffect->SetTechnique("ViewShader"))) {
		OutputDebugString("SetTechnique failed\n");
	}

	SetViewEffectInitialValues();

	// now, render
	if (FAILED(viewEffect->Begin(&cPasses, 0))) {
		OutputDebugString("Begin failed\n");
	}

	for(iPass = 0; iPass < cPasses; iPass++)
	{
		if (FAILED(viewEffect->BeginPass(iPass))) {
			OutputDebugString("Beginpass failed\n");
		}

		if (FAILED(m_pActualDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2))) {
			OutputDebugString("Draw failed\n");
		}		

		if (FAILED(viewEffect->EndPass())) {
			OutputDebugString("Beginpass failed\n");
		}
	}

	if (FAILED(viewEffect->End())) {
		OutputDebugString("End failed\n");
	}

	PostViewEffectCleanup();
	
	// how to restore render states ?
	switch(howToSaveRenderStates)
	{
	case HowToSaveRenderStates::STATE_BLOCK:
		// apply stored render states
		sb->Apply();
		sb->Release();
		sb = NULL;
		break;
	case HowToSaveRenderStates::SELECTED_STATES_MANUALLY:
		RestoreState();
		break;
	case HowToSaveRenderStates::ALL_STATES_MANUALLY:
		RestoreAllRenderStates(m_pActualDevice);
		break;
	case HowToSaveRenderStates::DO_NOT_SAVE_AND_RESTORE:
		break;
	}

}

void StereoView::SaveLastScreen()
{
	
}

/**
* Saves screenshot and shot of left and right surface.
***/
void StereoView::SaveScreen()
{
	static int screenCount = 0;
	++screenCount;

	char fileName[32];
	wsprintf(fileName, "%d_final.bmp", screenCount);
	char fileNameLeft[32];
	wsprintf(fileNameLeft, "%d_left.bmp", screenCount);
	char fileNameRight[32];
	wsprintf(fileNameRight, "%d_right.bmp", screenCount);

#ifdef _DEBUG
	OutputDebugString(fileName);
	OutputDebugString("\n");
#endif	

	D3DXSaveSurfaceToFile(fileNameLeft, D3DXIFF_BMP, leftSurface, NULL, NULL);
	D3DXSaveSurfaceToFile(fileNameRight, D3DXIFF_BMP, rightSurface, NULL, NULL);
	D3DXSaveSurfaceToFile(fileName, D3DXIFF_BMP, backBuffer, NULL, NULL);
}

IDirect3DSurface9* StereoView::GetBackBuffer()
{
	SHOW_CALL("StereoView::GetBackBuffer\n");
	return backBuffer;
}

/**
* Calls ID3DXEffect::OnResetDevice.
***/
void StereoView::PostReset()
{
	SHOW_CALL("StereoView::PostReset\n");
	CalculateShaderVariables();
	if (viewEffect)
		viewEffect->OnResetDevice();
}

/**
* Inits the left and right texture buffer.
* Also gets viewport data and back buffer render target.
***/
void StereoView::InitTextureBuffers()
{
	SHOW_CALL("StereoView::InitTextureBuffers\n");
	m_pActualDevice->GetViewport(&viewport);
	D3DSURFACE_DESC pDesc = D3DSURFACE_DESC();
	m_pActualDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	backBuffer->GetDesc(&pDesc);
	

#ifdef _DEBUG
	debugf("viewport width: %d",viewport.Width);
	OutputDebugString("\n");

	debugf("backbuffer width: %d",pDesc.Width);
	OutputDebugString("\n");
#endif

	m_pActualDevice->CreateTexture(pDesc.Width, pDesc.Height, 0, D3DUSAGE_RENDERTARGET, pDesc.Format, D3DPOOL_DEFAULT, &leftTexture, NULL);
	leftTexture->GetSurfaceLevel(0, &leftSurface);

	m_pActualDevice->CreateTexture(pDesc.Width, pDesc.Height, 0, D3DUSAGE_RENDERTARGET, pDesc.Format, D3DPOOL_DEFAULT, &rightTexture, NULL);
	rightTexture->GetSurfaceLevel(0, &rightSurface);


}

/**
* Inits a simple full screen vertex buffer containing 4 vertices.
***/
void StereoView::InitVertexBuffers()
{
	SHOW_CALL("StereoView::initVertexBuffers\n");

	HRESULT hr = S_OK;
	IDirect3DDevice9Ex *pDirect3DDevice9Ex = NULL;
	if (SUCCEEDED(m_pActualDevice->QueryInterface(IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&pDirect3DDevice9Ex))))
	{
		//Must use default pool for DX9Ex 
		hr = pDirect3DDevice9Ex->CreateVertexBuffer(sizeof(TEXVERTEX) * 4, NULL,
			D3DFVF_TEXVERTEX, D3DPOOL_DEFAULT, &screenVertexBuffer, NULL);
		pDirect3DDevice9Ex->Release();
	}
	else
	{
		hr = m_pActualDevice->CreateVertexBuffer(sizeof(TEXVERTEX) * 4, NULL,
			D3DFVF_TEXVERTEX, D3DPOOL_MANAGED, &screenVertexBuffer, NULL);
	}

	if (FAILED(hr))
	{
		char buffer[256];
		sprintf_s(buffer, "CreateVertexBuffer - Failed: 0x%0.8x", hr);
		OutputDebugString(buffer);
	}

	TEXVERTEX* vertices;

	screenVertexBuffer->Lock(0, 0, (void**)&vertices, NULL);

	float scale = 1.0f;

	RECT* rDest = new RECT();
	rDest->left = 0;
	rDest->right = int(viewport.Width*scale);
	rDest->top = 0;
	rDest->bottom = int(viewport.Height*scale);

	//Setup vertices
	vertices[0].x = (float) rDest->left - 0.5f;
	vertices[0].y = (float) rDest->top - 0.5f;
	vertices[0].z = 0.0f;
	vertices[0].rhw = 1.0f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = (float) rDest->right - 0.5f;
	vertices[1].y = (float) rDest->top - 0.5f;
	vertices[1].z = 0.0f;
	vertices[1].rhw = 1.0f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = (float) rDest->right - 0.5f;
	vertices[2].y = (float) rDest->bottom - 0.5f;
	vertices[2].z = 0.0f;
	vertices[2].rhw = 1.0f;
	vertices[2].u = 1.0f;	
	vertices[2].v = 1.0f;

	vertices[3].x = (float) rDest->left - 0.5f;
	vertices[3].y = (float) rDest->bottom - 0.5f;
	vertices[3].z = 0.0f;
	vertices[3].rhw = 1.0f;
	vertices[3].u = 0.0f;
	vertices[3].v = 1.0f;

	screenVertexBuffer->Unlock();
}

/**
* Loads stereo mode effect file.
***/
void StereoView::InitShaderEffects()
{
	SHOW_CALL("StereoView::InitShaderEffects\n");

	shaderEffect[ANAGLYPH_RED_CYAN] = "AnaglyphRedCyan.fx";
	shaderEffect[ANAGLYPH_RED_CYAN_GRAY] = "AnaglyphRedCyanGray.fx";
	shaderEffect[ANAGLYPH_YELLOW_BLUE] = "AnaglyphYellowBlue.fx";
	shaderEffect[ANAGLYPH_YELLOW_BLUE_GRAY] = "AnaglyphYellowBlueGray.fx";
	shaderEffect[ANAGLYPH_GREEN_MAGENTA] = "AnaglyphGreenMagenta.fx";
	shaderEffect[ANAGLYPH_GREEN_MAGENTA_GRAY] = "AnaglyphGreenMagentaGray.fx";
	shaderEffect[SIDE_BY_SIDE] = "SideBySide.fx";
	shaderEffect[DIY_RIFT] = "SideBySideRift.fx";
	shaderEffect[OVER_UNDER] = "OverUnder.fx";
	shaderEffect[INTERLEAVE_HORZ] = "InterleaveHorz.fx";
	shaderEffect[INTERLEAVE_VERT] = "InterleaveVert.fx";
	shaderEffect[CHECKERBOARD] = "Checkerboard.fx";

	ProxyHelper helper = ProxyHelper();

	std::string viewPath = helper.GetPath("fx\\") + shaderEffect[config->stereo_mode];

	if (FAILED(D3DXCreateEffectFromFile(m_pActualDevice, viewPath.c_str(), NULL, NULL, D3DXFX_DONOTSAVESTATE, NULL, &viewEffect, NULL))) {
		OutputDebugString("Effect creation failed\n");
	}
}

/**
* Empty in parent class.
***/
void StereoView::SetViewEffectInitialValues() {} 

void StereoView::PostViewEffectCleanup()
{
	//Do nothing here
}

/**
* Empty in parent class.
***/
void StereoView::CalculateShaderVariables() {} 

/**
* Workaround for Half Life 2 for now.
***/
void StereoView::SaveState()
{
	SHOW_CALL("StereoView::SaveState");
	m_pActualDevice->GetTextureStageState(0, D3DTSS_COLOROP, &tssColorOp);
	m_pActualDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &tssColorArg1);
	m_pActualDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &tssAlphaOp);
	m_pActualDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &tssAlphaArg1);
	m_pActualDevice->GetTextureStageState(0, D3DTSS_CONSTANT, &tssConstant);

	m_pActualDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &rsAlphaEnable);
	m_pActualDevice->GetRenderState(D3DRS_ZWRITEENABLE, &rsZWriteEnable);
	m_pActualDevice->GetRenderState(D3DRS_ZENABLE, &rsZEnable);
	m_pActualDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &rsSrgbEnable);

	m_pActualDevice->GetSamplerState(0, D3DSAMP_SRGBTEXTURE, &ssSrgb);
	m_pActualDevice->GetSamplerState(1, D3DSAMP_SRGBTEXTURE, &ssSrgb1);
	
	m_pActualDevice->GetSamplerState(0, D3DSAMP_ADDRESSU, &ssAddressU);
	m_pActualDevice->GetSamplerState(0, D3DSAMP_ADDRESSV, &ssAddressV);
	m_pActualDevice->GetSamplerState(0, D3DSAMP_ADDRESSW, &ssAddressW);

	m_pActualDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &ssMag0);
	m_pActualDevice->GetSamplerState(1, D3DSAMP_MAGFILTER, &ssMag1);
	m_pActualDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &ssMin0);
	m_pActualDevice->GetSamplerState(1, D3DSAMP_MINFILTER, &ssMin1);
	m_pActualDevice->GetSamplerState(0, D3DSAMP_MIPFILTER, &ssMip0);
	m_pActualDevice->GetSamplerState(1, D3DSAMP_MIPFILTER, &ssMip1);

	m_pActualDevice->GetTexture(0, &lastTexture);
	m_pActualDevice->GetTexture(1, &lastTexture1);

	m_pActualDevice->GetVertexShader(&lastVertexShader);
	m_pActualDevice->GetPixelShader(&lastPixelShader);

	m_pActualDevice->GetVertexDeclaration(&lastVertexDeclaration);

	m_pActualDevice->GetRenderTarget(0, &lastRenderTarget0);
	m_pActualDevice->GetRenderTarget(1, &lastRenderTarget1);
}

/**
* Set all states and settings for fullscreen render.
* Also sets identity world, view and projection matrix. 
***/
void StereoView::SetState()
{
	SHOW_CALL("StereoView::SetState");
	D3DXMATRIX	identity;
	m_pActualDevice->SetTransform(D3DTS_WORLD, D3DXMatrixIdentity(&identity));
	m_pActualDevice->SetTransform(D3DTS_VIEW, &identity);
	m_pActualDevice->SetTransform(D3DTS_PROJECTION, &identity);
	m_pActualDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_pActualDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pActualDevice->SetRenderState(D3DRS_ZENABLE,  D3DZB_TRUE);
	m_pActualDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_pActualDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	m_pActualDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);// This fixed interior or car not being drawn in rFactor
	m_pActualDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE); 

	m_pActualDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CONSTANT);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_CONSTANT, 0xffffffff);

	m_pActualDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	m_pActualDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	m_pActualDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_pActualDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);  

	int value = 0;
	if (ProxyHelper::ParseGameType(config->game_type, ProxyHelper::DeviceStateFlags, value) && (value & 2))
	{
		m_pActualDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, ssSrgb);
		m_pActualDevice->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, ssSrgb);
	}
	else
	{
		//Borderlands Dark Eye FIX
		m_pActualDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
		m_pActualDevice->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, 0);
	}
	

	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	// TODO Need to check m_pActualDevice capabilities if we want a prefered order of fallback rather than 
	// whatever the default is being used when a mode isn't supported.
	// Example - GeForce 660 doesn't appear to support D3DTEXF_ANISOTROPIC on the MAGFILTER (at least
	// according to the spam of error messages when running with the directx debug runtime)
	m_pActualDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	//m_pActualDevice->SetTexture(0, NULL);
	//m_pActualDevice->SetTexture(1, NULL);

	m_pActualDevice->SetVertexShader(NULL);
	m_pActualDevice->SetPixelShader(NULL);

	m_pActualDevice->SetVertexDeclaration(NULL);

	//It's a Direct3D9 error when using the debug runtine to set RenderTarget 0 to NULL
	//m_pActualDevice->SetRenderTarget(0, NULL);
	m_pActualDevice->SetRenderTarget(1, NULL);
	m_pActualDevice->SetRenderTarget(2, NULL);
	m_pActualDevice->SetRenderTarget(3, NULL);
}

/**
* Workaround for Half Life 2 for now.
***/
void StereoView::RestoreState()
{
	SHOW_CALL("StereoView::RestoreState");
	m_pActualDevice->SetTextureStageState(0, D3DTSS_COLOROP, tssColorOp);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_COLORARG1, tssColorArg1);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, tssAlphaOp);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, tssAlphaArg1);
	m_pActualDevice->SetTextureStageState(0, D3DTSS_CONSTANT, tssConstant);

	m_pActualDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, rsAlphaEnable);
	m_pActualDevice->SetRenderState(D3DRS_ZWRITEENABLE, rsZWriteEnable);
	m_pActualDevice->SetRenderState(D3DRS_ZENABLE, rsZEnable);
	m_pActualDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, rsSrgbEnable);

	m_pActualDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, ssSrgb);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, ssSrgb1);

	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, ssAddressU);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, ssAddressV);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, ssAddressW);

	m_pActualDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, ssMag0);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, ssMag1);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_MINFILTER, ssMin0);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MINFILTER, ssMin1);
	m_pActualDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, ssMip0);
	m_pActualDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, ssMip1);

	m_pActualDevice->SetTexture(0, lastTexture);
	if(lastTexture != NULL)
		lastTexture->Release();
	lastTexture = NULL;

	m_pActualDevice->SetTexture(1, lastTexture1);
	if(lastTexture1 != NULL)
		lastTexture1->Release();
	lastTexture1 = NULL;

	m_pActualDevice->SetVertexShader(lastVertexShader);
	if(lastVertexShader != NULL)
		lastVertexShader->Release();
	lastVertexShader = NULL;

	m_pActualDevice->SetPixelShader(lastPixelShader);
	if(lastPixelShader != NULL)
		lastPixelShader->Release();
	lastPixelShader = NULL;

	m_pActualDevice->SetVertexDeclaration(lastVertexDeclaration);
	if(lastVertexDeclaration != NULL)
		lastVertexDeclaration->Release();
	lastVertexDeclaration = NULL;

	m_pActualDevice->SetRenderTarget(0, lastRenderTarget0);
	if(lastRenderTarget0 != NULL)
		lastRenderTarget0->Release();
	lastRenderTarget0 = NULL;

	m_pActualDevice->SetRenderTarget(1, lastRenderTarget1);
	if(lastRenderTarget1 != NULL)
		lastRenderTarget1->Release();
	lastRenderTarget1 = NULL;
}

/**
* Saves all Direct3D 9 render states.
* Used for games that do not work with state blocks for some reason.
***/
void StereoView::SaveAllRenderStates(LPDIRECT3DDEVICE9 pDevice)
{
	SHOW_CALL("StereoView::SaveAllRenderStates\n");
	// save all Direct3D 9 RenderStates 
	DWORD dwCount = 0;
	pDevice->GetRenderState(D3DRS_ZENABLE                     , &renderStates[dwCount++]); 
	pDevice->GetRenderState(D3DRS_FILLMODE                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SHADEMODE                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ZWRITEENABLE                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ALPHATESTENABLE             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_LASTPIXEL                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SRCBLEND                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DESTBLEND                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CULLMODE                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ZFUNC                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ALPHAREF                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ALPHAFUNC                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DITHERENABLE                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGENABLE                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SPECULARENABLE              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGCOLOR                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGTABLEMODE                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGSTART                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGEND                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGDENSITY                  , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_RANGEFOGENABLE              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILENABLE               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILFAIL                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILZFAIL                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILPASS                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILFUNC                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILREF                  , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILMASK                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_STENCILWRITEMASK            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_TEXTUREFACTOR               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP0                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP1                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP2                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP3                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP4                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP5                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP6                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP7                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CLIPPING                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_LIGHTING                    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_AMBIENT                     , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_FOGVERTEXMODE               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_COLORVERTEX                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_LOCALVIEWER                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_NORMALIZENORMALS            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DIFFUSEMATERIALSOURCE       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SPECULARMATERIALSOURCE      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_AMBIENTMATERIALSOURCE       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_EMISSIVEMATERIALSOURCE      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_VERTEXBLEND                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CLIPPLANEENABLE             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSIZE                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSIZE_MIN               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSPRITEENABLE           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSCALEENABLE            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSCALE_A                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSCALE_B                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSCALE_C                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS        , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_MULTISAMPLEMASK             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_PATCHEDGESTYLE              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DEBUGMONITORTOKEN           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POINTSIZE_MAX               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_COLORWRITEENABLE            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_TWEENFACTOR                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_BLENDOP                     , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_POSITIONDEGREE              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_NORMALDEGREE                , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS         , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ANTIALIASEDLINEENABLE       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_MINTESSELLATIONLEVEL        , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_MAXTESSELLATIONLEVEL        , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ADAPTIVETESS_X              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ADAPTIVETESS_Y              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ADAPTIVETESS_Z              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ADAPTIVETESS_W              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION  , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_TWOSIDEDSTENCILMODE         , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CCW_STENCILFAIL             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CCW_STENCILZFAIL            , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CCW_STENCILPASS             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_CCW_STENCILFUNC             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_COLORWRITEENABLE1           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_COLORWRITEENABLE2           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_COLORWRITEENABLE3           , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_BLENDFACTOR                 , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE             , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DEPTHBIAS                   , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP8                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP9                       , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP10                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP11                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP12                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP13                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP14                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_WRAP15                      , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE    , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_SRCBLENDALPHA               , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_DESTBLENDALPHA              , &renderStates[dwCount++]);
	pDevice->GetRenderState(D3DRS_BLENDOPALPHA                , &renderStates[dwCount++]);
}

/**
* Sets all Direct3D 9 render states to their default values.
* Use this function only if a game does not want to render.
***/
void StereoView::SetAllRenderStatesDefault(LPDIRECT3DDEVICE9 pDevice)
{
	SHOW_CALL("StereoView::SetAllRenderStatesDefault\n");
	// set all Direct3D 9 RenderStates to default values
	float fData = 0.0f;
	double dData = 0.0f;

	pDevice->SetRenderState(D3DRS_ZENABLE                     , D3DZB_TRUE);
	pDevice->SetRenderState(D3DRS_FILLMODE                    , D3DFILL_SOLID);
	pDevice->SetRenderState(D3DRS_SHADEMODE                   , D3DSHADE_GOURAUD);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE                , TRUE);
	pDevice->SetRenderState(D3DRS_ALPHATESTENABLE             , FALSE);
	pDevice->SetRenderState(D3DRS_LASTPIXEL                   , TRUE);
	pDevice->SetRenderState(D3DRS_SRCBLEND                    , D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLEND                   , D3DBLEND_ZERO);
	pDevice->SetRenderState(D3DRS_CULLMODE                    , D3DCULL_CCW);
	pDevice->SetRenderState(D3DRS_ZFUNC                       , D3DCMP_LESSEQUAL);
	pDevice->SetRenderState(D3DRS_ALPHAREF                    , 0);
	pDevice->SetRenderState(D3DRS_ALPHAFUNC                   , D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_DITHERENABLE                , FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE            , FALSE);
	pDevice->SetRenderState(D3DRS_FOGENABLE                   , FALSE);
	pDevice->SetRenderState(D3DRS_SPECULARENABLE              , FALSE);
	pDevice->SetRenderState(D3DRS_FOGCOLOR                    , 0);
	pDevice->SetRenderState(D3DRS_FOGTABLEMODE                , D3DFOG_NONE);
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_FOGSTART                    , *((DWORD*)&fData));
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_FOGEND                      , *((DWORD*)&fData));
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_FOGDENSITY                  , *((DWORD*)&fData));
	pDevice->SetRenderState(D3DRS_RANGEFOGENABLE              , FALSE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE               , FALSE);
	pDevice->SetRenderState(D3DRS_STENCILFAIL                 , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILZFAIL                , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILPASS                 , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILFUNC                 , D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_STENCILREF                  , 0);
	pDevice->SetRenderState(D3DRS_STENCILMASK                 , 0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_STENCILWRITEMASK            , 0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_TEXTUREFACTOR               , 0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_WRAP0                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP1                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP2                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP3                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP4                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP5                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP6                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP7                       , 0);
	pDevice->SetRenderState(D3DRS_CLIPPING                    , TRUE);
	pDevice->SetRenderState(D3DRS_LIGHTING                    , TRUE);
	pDevice->SetRenderState(D3DRS_AMBIENT                     , 0);
	pDevice->SetRenderState(D3DRS_FOGVERTEXMODE               , D3DFOG_NONE);
	pDevice->SetRenderState(D3DRS_COLORVERTEX                 , TRUE);
	pDevice->SetRenderState(D3DRS_LOCALVIEWER                 , TRUE);
	pDevice->SetRenderState(D3DRS_NORMALIZENORMALS            , FALSE);
	pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE       , D3DMCS_COLOR1);
	pDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE      , D3DMCS_COLOR2);
	pDevice->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE       , D3DMCS_MATERIAL);
	pDevice->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE      , D3DMCS_MATERIAL);
	pDevice->SetRenderState(D3DRS_VERTEXBLEND                 , D3DVBF_DISABLE);
	pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE             , 0);
	pDevice->SetRenderState(D3DRS_POINTSIZE                   , 64);
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_POINTSIZE_MIN               , *((DWORD*)&fData));
	pDevice->SetRenderState(D3DRS_POINTSPRITEENABLE           , FALSE);
	pDevice->SetRenderState(D3DRS_POINTSCALEENABLE            , FALSE);
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_POINTSCALE_A                , *((DWORD*)&fData));
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_POINTSCALE_B                , *((DWORD*)&fData));
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_POINTSCALE_C                , *((DWORD*)&fData));
	pDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS        , TRUE);
	pDevice->SetRenderState(D3DRS_MULTISAMPLEMASK             , 0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_PATCHEDGESTYLE              , D3DPATCHEDGE_DISCRETE);
	pDevice->SetRenderState(D3DRS_DEBUGMONITORTOKEN           , D3DDMT_ENABLE);
	dData = 64.0;
	pDevice->SetRenderState(D3DRS_POINTSIZE_MAX               , *((DWORD*)&dData));
	pDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE    , FALSE);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE            , 0x0000000F);
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_TWEENFACTOR                 , *((DWORD*)&fData));
	pDevice->SetRenderState(D3DRS_BLENDOP                     , D3DBLENDOP_ADD);
	pDevice->SetRenderState(D3DRS_POSITIONDEGREE              , D3DDEGREE_CUBIC);
	pDevice->SetRenderState(D3DRS_NORMALDEGREE                , D3DDEGREE_LINEAR );
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE           , FALSE);
	pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS         , 0);
	pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE       , FALSE);
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_MINTESSELLATIONLEVEL        , *((DWORD*)&fData));
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_MAXTESSELLATIONLEVEL        , *((DWORD*)&fData));
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_X              , *((DWORD*)&fData));
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y              , *((DWORD*)&fData));
	fData = 1.0f;
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_Z              , *((DWORD*)&fData));
	fData = 0.0f;
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_W              , *((DWORD*)&fData));
	pDevice->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION  , FALSE);
	pDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE         , FALSE);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFAIL             , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL            , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILPASS             , D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFUNC             , D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE1           , 0x0000000f);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE2           , 0x0000000f);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE3           , 0x0000000f);
	pDevice->SetRenderState(D3DRS_BLENDFACTOR                 , 0xffffffff);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE             , 0);
	pDevice->SetRenderState(D3DRS_DEPTHBIAS                   , 0);
	pDevice->SetRenderState(D3DRS_WRAP8                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP9                       , 0);
	pDevice->SetRenderState(D3DRS_WRAP10                      , 0);
	pDevice->SetRenderState(D3DRS_WRAP11                      , 0);
	pDevice->SetRenderState(D3DRS_WRAP12                      , 0);
	pDevice->SetRenderState(D3DRS_WRAP13                      , 0);
	pDevice->SetRenderState(D3DRS_WRAP14                      , 0);
	pDevice->SetRenderState(D3DRS_WRAP15                      , 0);
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE    , FALSE);
	pDevice->SetRenderState(D3DRS_SRCBLENDALPHA               , D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLENDALPHA              , D3DBLEND_ZERO);
	pDevice->SetRenderState(D3DRS_BLENDOPALPHA                , D3DBLENDOP_ADD);
}

/**
* Restores all Direct3D 9 render states.
* Used for games that do not work with state blocks for some reason.
***/
void StereoView::RestoreAllRenderStates(LPDIRECT3DDEVICE9 pDevice)
{
	SHOW_CALL("StereoView::RestoreAllRenderStates\n");
	// set all Direct3D 9 RenderStates to saved values
	DWORD dwCount = 0;
	pDevice->SetRenderState(D3DRS_ZENABLE                     , renderStates[dwCount++]); 
	pDevice->SetRenderState(D3DRS_FILLMODE                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SHADEMODE                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ALPHATESTENABLE             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_LASTPIXEL                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SRCBLEND                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DESTBLEND                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CULLMODE                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ZFUNC                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ALPHAREF                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ALPHAFUNC                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DITHERENABLE                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGENABLE                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SPECULARENABLE              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGCOLOR                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGTABLEMODE                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGSTART                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGEND                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGDENSITY                  , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_RANGEFOGENABLE              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILENABLE               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILFAIL                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILZFAIL                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILPASS                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILFUNC                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILREF                  , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILMASK                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_STENCILWRITEMASK            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_TEXTUREFACTOR               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP0                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP1                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP2                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP3                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP4                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP5                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP6                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP7                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CLIPPING                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_LIGHTING                    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_AMBIENT                     , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_FOGVERTEXMODE               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_COLORVERTEX                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_LOCALVIEWER                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_NORMALIZENORMALS            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_VERTEXBLEND                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSIZE                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSIZE_MIN               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSPRITEENABLE           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSCALEENABLE            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSCALE_A                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSCALE_B                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSCALE_C                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS        , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_MULTISAMPLEMASK             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_PATCHEDGESTYLE              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DEBUGMONITORTOKEN           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POINTSIZE_MAX               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_TWEENFACTOR                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_BLENDOP                     , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_POSITIONDEGREE              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_NORMALDEGREE                , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS         , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_MINTESSELLATIONLEVEL        , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_MAXTESSELLATIONLEVEL        , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_X              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_Z              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ADAPTIVETESS_W              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION  , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE         , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFAIL             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL            , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CCW_STENCILPASS             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFUNC             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE1           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE2           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE3           , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_BLENDFACTOR                 , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE             , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DEPTHBIAS                   , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP8                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP9                       , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP10                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP11                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP12                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP13                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP14                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_WRAP15                      , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE    , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_SRCBLENDALPHA               , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_DESTBLENDALPHA              , renderStates[dwCount++]);
	pDevice->SetRenderState(D3DRS_BLENDOPALPHA                , renderStates[dwCount++]);
}