#pragma once
#include <G3D/G3D.h>
#include "IrradianceField.h"
#include "GIRenderer.h"

class App : public GApp
{
	shared_ptr<CGIRenderer>     m_pGIRenderer;
	shared_ptr<IrradianceField> m_pIrradianceField;
	bool m_firstFrame = true;
	bool m_staticProbe = true;
	float maxAdaptiveFactor = 0.5f; // adaptive数量最多为uniform的0.5倍

	//shared_ptr<Texture> m_gbuffer_depth;
	//shared_ptr<Texture> m_gbuffer_ws_normal;
	//shared_ptr<Texture> m_gbuffer_ws_position;
	shared_ptr<Texture> screenProbeWSUniformPositionTexture;
	shared_ptr<Texture> screenProbeWSAdaptivePositionTexture;
	shared_ptr<Texture> screenProbeSSAdaptivePositionTexture;
	shared_ptr<Texture> screenTileAdaptiveProbeHeaderTexture;
	shared_ptr<Texture> screenTileAdaptiveProbeIndicesTexture;
	shared_ptr<Texture> numAdaptiveScreenProbesTexture;
	//CoordinateFrame last_view;
	RealTime last_view;
protected:
	void makeGUI();

public:
	App(const GApp::Settings& settings = GApp::Settings());

	virtual void onInit() override;
	virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface>>& surface3D) override;
	virtual void onAfterLoadScene(const Any& any, const String& sceneName) override;
	void screenProbeAdaptivePlacement(RenderDevice* rd);
	void screenProbeDebugDraw();
	void cleanScreenProbe();
	void screenProbeUniformPlacement(RenderDevice* rd, int downsampleFactor);
};
