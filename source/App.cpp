#include "App.h"

G3D_START_AT_MAIN();

int main(int argc, const char* argv[])
{
	initGLG3D(G3DSpecification());

	GApp::Settings settings(argc, argv);

	settings.window.caption = argv[0];

	settings.window.fullScreen = false;
	settings.window.width = 1600;
	settings.window.height = 960;
	settings.window.resizable = !settings.window.fullScreen;
	settings.window.framed = !settings.window.fullScreen;
	settings.window.defaultIconFilename = "icon.png";

	settings.window.asynchronous = true;

	settings.hdrFramebuffer.colorGuardBandThickness = Vector2int16(0, 0);
	settings.hdrFramebuffer.depthGuardBandThickness = Vector2int16(0, 0);

	settings.renderer.deferredShading = true;
	settings.renderer.orderIndependentTransparency = true;

	settings.dataDir = FileSystem::currentDirectory();

	settings.screenCapture.includeAppRevision = false;
	settings.screenCapture.includeG3DRevision = false;
	settings.screenCapture.filenamePrefix = "_";

	return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings)
{
}

void App::onInit()
{
	GApp::onInit();

	setFrameDuration(1.0f / 240.0f);

	m_gbufferSpecification.encoding[GBuffer::Field::LAMBERTIAN].format = ImageFormat::RGBA32F();
	m_gbufferSpecification.encoding[GBuffer::Field::GLOSSY].format = ImageFormat::RGBA32F();
	m_gbufferSpecification.encoding[GBuffer::Field::EMISSIVE].format = ImageFormat::RGBA32F();
	m_gbufferSpecification.encoding[GBuffer::Field::WS_POSITION].format = ImageFormat::RGBA32F();
	m_gbufferSpecification.encoding[GBuffer::Field::WS_NORMAL] = Texture::Encoding(ImageFormat::RGBA32F(), FrameName::CAMERA, 1.0f, 0.0f);

	m_pGIRenderer = dynamic_pointer_cast<CGIRenderer>(CGIRenderer::create());
	m_pGIRenderer->setDeferredShading(true);
	m_pGIRenderer->setOrderIndependentTransparency(true);

	//String SceneName = "Dragon (Dynamic Light Source)";
	//String SceneName = "G3D Breakfast Room";
	String SceneName = "G3D Living Room (Area Lights)";
	//String SceneName = "G3D Living Room";
	//String SceneName = "BathRoom";
	//String SceneName = "Living Room (Screen Probe)";
	loadScene(SceneName);
	//m_activeCamera = m_debugCamera;

	m_renderer = m_pGIRenderer;
	
	makeGUI();
}

void App::onGraphics3D(RenderDevice * rd, Array<shared_ptr<Surface>>& surface3D)
{
	if (m_pIrradianceField)
	{
		
		if (!m_firstFrame) {

			// if (activeCamera()->frame() != last_view) {
			if (activeCamera()->lastChangeTime() != last_view) {

				// last_view = activeCamera()->frame();
				last_view = activeCamera()->lastChangeTime();
				cleanScreenProbe();
				screenProbeAdaptivePlacement(rd);
				// show(screenTileAdaptiveProbeHeaderTexture);
			}
			//screenProbeDebugDraw();
			m_pIrradianceField->onGraphics3D(rd, surface3D, 
				screenProbeWSAdaptivePositionTexture, 
				screenProbeWSUniformPositionTexture, 
				screenProbeSSAdaptivePositionTexture,
				numAdaptiveScreenProbesTexture, 
				screenTileAdaptiveProbeHeaderTexture, 
				screenTileAdaptiveProbeIndicesTexture, 
				m_gbuffer);
		}

		
	}

	GApp::onGraphics3D(rd, surface3D);

	if (m_firstFrame) {
		m_firstFrame = false;
		
		//show(m_gbuffer_ws_position);
	}
}

void App::screenProbeDebugDraw() {

	int probeCountX = screenProbeWSUniformPositionTexture->width();
	int probeCountY = screenProbeWSUniformPositionTexture->height();
	shared_ptr<Image> screenProbeWSPositionImg = screenProbeWSUniformPositionTexture->toImage();

	const float radius = 0.01f;
	Color3 uniform_color = Color3(1.0f, 1.0f, 1.0f);
	Color3 adaptive_color = Color3(1.0f, 0.0f, 0.0f);

	for (int i = 0; i < probeCountX; ++i)
	{
		for (int j = 0; j < probeCountY; ++j) {
			Color4 wsPosition;
			
			Point2int32 index = Point2int32(i, j);
			screenProbeWSPositionImg->get(index, wsPosition);
			::debugDraw(std::make_shared<SphereShape>((Vector3)wsPosition.rgb(), radius), 0.0f, uniform_color * 0.8f, Color4::clear());
		}
	}

	shared_ptr<GLPixelTransferBuffer> numAdaptiveScreenProbesImg = numAdaptiveScreenProbesTexture->toPixelTransferBuffer();
	int *adaptiveProbeCount = (int*)numAdaptiveScreenProbesImg->mapRead();
	numAdaptiveScreenProbesImg->unmap();

	shared_ptr<GLPixelTransferBuffer> screenProbeWSAdaptivePositionImg = screenProbeWSAdaptivePositionTexture->toPixelTransferBuffer();
	float* adaptiveProbeList = (float*)screenProbeWSAdaptivePositionImg->mapRead();
	//Vector4unorm8 * adaptiveProbeList = (Vector4unorm8*)screenProbeWSAdaptivePositionImg->mapRead();
	screenProbeWSAdaptivePositionImg->unmap();

	for (int i = 0;i < *adaptiveProbeCount * 4; i += 4) {

		float x = adaptiveProbeList[i];
		float y = adaptiveProbeList[i + 1];
		float z = adaptiveProbeList[i + 2];
		Vector3 wsPosition = Vector3(x, y, z);
		::debugDraw(std::make_shared<SphereShape>(wsPosition, radius * 3), 0.0f, adaptive_color * 0.8f, Color4::clear());
		
	}
	//::debugDraw(std::make_shared<SphereShape>(Vector3(2.88,0.42,2.94), radius * 5), 0.0f, Color3(1.0f,1.0f,0.0f) * 0.8f, Color4::clear());
	//::debugDraw(std::make_shared<SphereShape>(Vector3(2.88, 0.6, 2.68), radius * 5), 0.0f, Color3(1.0f, 1.0f, 0.0f) * 0.8f, Color4::clear());
}

void App::onAfterLoadScene(const Any & any, const String & sceneName)
{
	m_pIrradianceField = IrradianceField::create(sceneName, scene());
	m_pIrradianceField->onSceneChanged(scene());
	m_pGIRenderer->setIrradianceField(m_pIrradianceField);
}

void App::makeGUI()
{
	debugWindow->setVisible(true);
	developerWindow->videoRecordDialog->setEnabled(true);

	debugWindow->pack();
	debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}

void App::screenProbeAdaptivePlacement(RenderDevice* rd) {

	//if (m_staticProbe) {
		int placementDownsampleFactor = 16;
		int screenProbeDownsampleFactor = placementDownsampleFactor;
		screenProbeUniformPlacement(rd, placementDownsampleFactor);

		int minDownsampleFactor = 4;
		// float maxAdaptiveFactor = 0.5f; // adaptive数量最多为uniform的0.5倍

		// TODO： View Change Clean

		// RW Buffer
		// World position
		screenProbeWSAdaptivePositionTexture = Texture::createEmpty("AdaptiveProbeWsPosition", rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor * maxAdaptiveFactor, ImageFormat::RGBA32F());
		shared_ptr<GLPixelTransferBuffer>& adaptiveProbeWSPosBuffer = GLPixelTransferBuffer::create(rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor * maxAdaptiveFactor, ImageFormat::RGBA32F());
		// Screen position
		screenProbeSSAdaptivePositionTexture = Texture::createEmpty("AdaptiveProbeSsPosition", rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor * maxAdaptiveFactor, ImageFormat::RGBA32F());
		shared_ptr<GLPixelTransferBuffer>& adaptiveProbeSSPosBuffer = GLPixelTransferBuffer::create(rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor * maxAdaptiveFactor, ImageFormat::RGBA32F());
		// Header
		screenTileAdaptiveProbeHeaderTexture = Texture::createEmpty("ScreenTileAdaptiveProbeHeader", rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor, ImageFormat::R32UI());
		shared_ptr<GLPixelTransferBuffer>& screenTileAdaptiveProbeHeaderBuffer = GLPixelTransferBuffer::create(rd->viewport().width() / placementDownsampleFactor, rd->viewport().height() / placementDownsampleFactor, ImageFormat::R32UI());
		// Index
		screenTileAdaptiveProbeIndicesTexture = Texture::createEmpty("ScreenTileAdaptiveProbeIndices", rd->viewport().width(), rd->viewport().height(), ImageFormat::R32UI());
		shared_ptr<GLPixelTransferBuffer>& screenTileAdaptiveProbeIndicesBuffer = GLPixelTransferBuffer::create(rd->viewport().width(), rd->viewport().height(), ImageFormat::R32UI());
		// Num
		numAdaptiveScreenProbesTexture = Texture::createEmpty("NumAdaptiveScreenProbes", 1, 1, ImageFormat::R32UI());
		shared_ptr<GLPixelTransferBuffer>& numAdaptiveScreenProbesBuffer = GLPixelTransferBuffer::create(1, 1, ImageFormat::R32UI());
		
		do {
			placementDownsampleFactor /= 2;
			Args args;

			// GroupSize & GroupNum
			const Vector3int32 blockSize(16, 16, 1);
			args.setComputeGridDim(Vector3int32(iCeil(rd->viewport().width() / (float(blockSize.x) * placementDownsampleFactor)),
				iCeil(rd->viewport().height() / (float(blockSize.y) * placementDownsampleFactor)), 1));
			args.setComputeGroupSize(blockSize);

			adaptiveProbeWSPosBuffer->bindAsShaderStorageBuffer(0);
			adaptiveProbeSSPosBuffer->bindAsShaderStorageBuffer(1);
			screenTileAdaptiveProbeHeaderBuffer->bindAsShaderStorageBuffer(2);
			screenTileAdaptiveProbeIndicesBuffer->bindAsShaderStorageBuffer(3);
			numAdaptiveScreenProbesBuffer->bindAsShaderStorageBuffer(4);

			args.setUniform("placementDownsampleFactor", placementDownsampleFactor);
			args.setUniform("screenProbeDownsampleFactor", screenProbeDownsampleFactor);
			args.setUniform("viewport_width", rd->viewport().width());
			args.setUniform("viewport_height", rd->viewport().height());
			args.setUniform("ws_positionTexture", m_gbuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
			args.setUniform("depthTexture", m_gbuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL), Sampler::buffer());
			args.setUniform("ws_normalTexture", m_gbuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());

			LAUNCH_SHADER("shaders/ScreenProbeAdaptivePlacement.glc", args);

			screenProbeWSAdaptivePositionTexture->update(adaptiveProbeWSPosBuffer);
			screenProbeSSAdaptivePositionTexture->update(adaptiveProbeSSPosBuffer);
			screenTileAdaptiveProbeHeaderTexture->update(screenTileAdaptiveProbeHeaderBuffer);
			screenTileAdaptiveProbeIndicesTexture->update(screenTileAdaptiveProbeIndicesBuffer);
			numAdaptiveScreenProbesTexture->update(numAdaptiveScreenProbesBuffer);
			
		} while (placementDownsampleFactor > minDownsampleFactor);
			

		m_staticProbe = false;
	//}

	

}

void App::screenProbeUniformPlacement(RenderDevice* rd, int downsampleFactor) {

	//m_gbuffer_ws_position = m_gbuffer->texture(GBuffer::Field::WS_POSITION);
	Args args;

	// GroupSize & GroupNum
	const Vector3int32 blockSize(16, 16, 1);
	args.setComputeGridDim(Vector3int32(iCeil(rd->viewport().width() / (float(blockSize.x) * downsampleFactor)),
		iCeil(rd->viewport().height() / (float(blockSize.y) * downsampleFactor)), 1));
	args.setComputeGroupSize(blockSize);

	// IO Variable
	screenProbeWSUniformPositionTexture = Texture::createEmpty("UniformProbeWsPosition", rd->viewport().width() / downsampleFactor, rd->viewport().height() / downsampleFactor, ImageFormat::RGBA32F());
	const shared_ptr<GLPixelTransferBuffer>& outputBuffer = GLPixelTransferBuffer::create(rd->viewport().width() / downsampleFactor, rd->viewport().height() / downsampleFactor, ImageFormat::RGBA32F());

	outputBuffer->bindAsShaderStorageBuffer(0);
	args.setUniform("placementDownsampleFactor", downsampleFactor);
	args.setUniform("viewport_width", rd->viewport().width());
	args.setUniform("viewport_height", rd->viewport().height());
	args.setUniform("ws_positionTexture", m_gbuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
	args.setUniform("depthTexture", m_gbuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL), Sampler::buffer());
	args.setUniform("ws_normalTexture", m_gbuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());

	// Run the uniform shader
	LAUNCH_SHADER("shaders/ScreenProbeUniformPlacement.glc", args);

	screenProbeWSUniformPositionTexture->update(outputBuffer);
}


void App::cleanScreenProbe() {

	if (screenProbeWSUniformPositionTexture) {
		screenProbeWSUniformPositionTexture->clear();
	}
	if (screenProbeWSAdaptivePositionTexture) {
		screenProbeWSAdaptivePositionTexture->clear();
	}
	if (screenProbeSSAdaptivePositionTexture) {
		screenProbeSSAdaptivePositionTexture->clear();
	}
	if (screenTileAdaptiveProbeHeaderTexture) {
		screenTileAdaptiveProbeHeaderTexture->clear();
	}
	if (screenTileAdaptiveProbeIndicesTexture) {
		screenTileAdaptiveProbeIndicesTexture->clear();
	}
	if (numAdaptiveScreenProbesTexture) {
		numAdaptiveScreenProbesTexture->clear();
	}
	
}