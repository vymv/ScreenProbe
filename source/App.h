#pragma once
#include <G3D/G3D.h>
#include "IrradianceField.h"
#include "GIRenderer.h"
#include "RadianceCache.h"

class App : public GApp
{
	shared_ptr<CGIRenderer>     m_pGIRenderer;
	shared_ptr<IrradianceField> m_pIrradianceField;
	shared_ptr<RadianceCache> m_pRadianceCache;
	bool m_firstFrame = true;
	bool m_staticProbe = true;
	int frameCount = 0;
	float maxAdaptiveFactor = 0.5f; // adaptive�������Ϊuniform��0.5��

	//shared_ptr<Texture> m_gbuffer_depth;
	//shared_ptr<Texture> m_gbuffer_ws_normal;
	//shared_ptr<Texture> m_gbuffer_ws_position;
	shared_ptr<Texture> screenProbeWSUniformPositionTexture;
	shared_ptr<Texture> screenProbeWSAdaptivePositionTexture;
	shared_ptr<Texture> screenProbeSSAdaptivePositionTexture;
	shared_ptr<Texture> screenTileAdaptiveProbeHeaderTexture;
	shared_ptr<Texture> screenTileAdaptiveProbeIndicesTexture;
	shared_ptr<Texture> numAdaptiveScreenProbesTexture;

	RealTime last_view;
protected:
	void makeGUI();

public:
	App(const GApp::Settings& settings = GApp::Settings());

	virtual void onInit() override;
	virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface>>& surface3D) override;
	virtual void onAfterLoadScene(const Any& any, const String& sceneName) override;
	void screenProbeAdaptivePlacement();
	void screenProbeDebugDraw();
	void cleanScreenProbe();
	void screenProbeUniformPlacement(int downsampleFactor);
};
