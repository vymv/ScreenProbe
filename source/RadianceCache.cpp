#include "RadianceCache.h"



Vector3 operator-(const Vector3& target, float value) {
	return target - Vector3(value, value, value);
}
Vector3 operator+(const Vector3& target, float value) {
	return target + Vector3(value, value, value);
}

Vector3int32 operator+(const Vector3int32& target, float value) {
	return target + Vector3int32(value, value, value);
}

void RadianceCache::updateRadianceCache(RenderDevice* rd,
	Rect2D atlasRect,
	bool Radiance
	)
{
	rd->push2D(Radiance ? m_radianceCacheFB : m_radianceMeanDistanceFB); {
		Args args;
		rd->setBlendFunc(RenderDevice::BLEND_SRC_ALPHA, RenderDevice::BLEND_ONE_MINUS_SRC_ALPHA);
		// Set the depth test to discard the border pixels
		rd->setDepthTest(RenderDevice::DepthTest::DEPTH_GREATER);
		args.setRect(atlasRect);
		//args.setUniform("hysteresis", m_firstFrame ? 0.0f : m_specification.hysteresis);
		args.setUniform("depthSharpness", m_specification.depthSharpness);
		args.setMacro("Radiance", Radiance);
		if (Radiance) {
			args.setImageUniform("RadianceCache", m_RadianceCache);
			
		}
		else {
			args.setImageUniform("RadianceCacheDepth", m_RadianceCacheDepth);
		}
		
		// Uniforms to compute texel to direction and back in oct format
		args.setMacro("RadianceProbeResolution", radianceCacheInputs.RadianceProbeResolution);
		//args.setMacro("AtlasResolutionInProbe", radianceCacheInputs.ProbeAtlasResolutionInProbes);
		//args.setUniform("probeSideLength", irradiance ? irradianceOctSideLength() : depthOctSideLength());
		args.setUniform("maxDistance", m_maxDistance);
		args.setUniform("energyConservation", 0.95f);
		//->setShaderArgs(args, "rayHitLocations.", Sampler::buffer());
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_NORMAL)->setShaderArgs(args, "rayHitNormals.", Sampler::buffer());
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_POSITION)->setShaderArgs(args, "rayHitLocations.", Sampler::buffer());
		args.setUniform("hysteresis", frameCount <= 1 ? 0.0f : m_specification.hysteresis);
		m_radianceRayOrigins->setShaderArgs(args, "rayOrigins.", Sampler::buffer());
		m_radianceRayDirections->setShaderArgs(args, "rayDirections.", Sampler::buffer());
		m_radianceRaysRadianceFB->texture(0)->setShaderArgs(args, "rayHitRadiance.", Sampler::buffer());

		// Set skybox args to read on miss
		dynamic_pointer_cast<Skybox>(m_scene->entity("skybox"))->keyframeArray()[0]->setShaderArgs(args, "skybox_", Sampler::defaults());

		//args.setMacro("OUTPUT_IRRADIANCE", irradiance);
		LAUNCH_SHADER("shaders/WorldSpaceProbeUpdateRadianceRast.pix", args);

	}rd->pop2D();
}

RadianceCache::RadianceCache()
{
	
	//m_sceneTriTree = TriTree::create(true);
	
}

bool RadianceCache::UpdateRadianceCacheState(shared_ptr<Camera> camera, RadianceCacheInputs& radianceCacheInputs, RadianceCacheState& CacheState)
{
	
	bool bResetState = CacheState.ClipmapWorldExtent != radianceCacheInputs.ClipmapWorldExtent || CacheState.ClipmapDistributionBase != radianceCacheInputs.ClipmapDistributionBase;

	CacheState.ClipmapWorldExtent = radianceCacheInputs.ClipmapWorldExtent;
	CacheState.ClipmapDistributionBase = radianceCacheInputs.ClipmapDistributionBase;

	const int32 ClipmapResolution = radianceCacheInputs.RadianceProbeClipmapResolution;
	const int32 NumClipmaps = radianceCacheInputs.NumRadianceProbeClipmaps;
	Vector3 NewViewOrigin = camera->frame().translation;

	CacheState.clipmaps.resize(NumClipmaps);

	for (int32 ClipmapIndex = 0; ClipmapIndex < NumClipmaps; ++ClipmapIndex)
	{
		RadianceCacheClipmap& Clipmap = CacheState.clipmaps[ClipmapIndex];

		const float ClipmapExtent = radianceCacheInputs.ClipmapWorldExtent * pow(radianceCacheInputs.ClipmapDistributionBase, ClipmapIndex);
		const float CellSize = (2.0f * ClipmapExtent) / ClipmapResolution;

		Vector3int32 GridCenter;
		GridCenter.x = floor(NewViewOrigin.x / CellSize);
		GridCenter.y = floor(NewViewOrigin.y / CellSize);
		GridCenter.z = floor(NewViewOrigin.z / CellSize);

		const Vector3 SnappedCenter = Vector3(GridCenter) * CellSize;

		Clipmap.Center = SnappedCenter;
		Clipmap.Extent = ClipmapExtent;
		Clipmap.VolumeUVOffset = Vector3(0.0f, 0.0f, 0.0f);
		Clipmap.CellSize = CellSize;

		// Shift the clipmap grid down so that probes align with other clipmaps
		const Vector3 ClipmapMin = Clipmap.Center - Clipmap.Extent - 0.5f * Clipmap.CellSize;

		Clipmap.ProbeCoordToWorldCenterBias = ClipmapMin + 0.5f * Clipmap.CellSize;
		Clipmap.ProbeCoordToWorldCenterScale = Clipmap.CellSize;

		Clipmap.WorldPositionToProbeCoordScale = 1.0f / CellSize;
		Clipmap.WorldPositionToProbeCoordBias = -ClipmapMin / CellSize;

		Clipmap.ProbeTMin = radianceCacheInputs.CalculateIrradiance ? 0.0f : Vector3(CellSize, CellSize, CellSize).magnitude();
		//Vector3int32 ProbeCoordOrig(25,32,20);

		//Vector3 WorldSpacePosition = Vector3(ProbeCoordOrig) * Clipmap.ProbeCoordToWorldCenterScale + Clipmap.ProbeCoordToWorldCenterBias;
		//Vector3 ProbeCoordFloat = WorldSpacePosition * Clipmap.WorldPositionToProbeCoordScale + Clipmap.WorldPositionToProbeCoordBias;
		//Vector3int32 ProbeCoord(floor(ProbeCoordFloat.x), floor(ProbeCoordFloat.y), floor(ProbeCoordFloat.z));
		//Vector3 BottomEdgeFades = Vector3(ProbeCoordFloat - .5f);
		//BottomEdgeFades.x = clamp(BottomEdgeFades.x, 0.f, 1.f);
		//BottomEdgeFades.y = clamp(BottomEdgeFades.y, 0.f, 1.f);
		//BottomEdgeFades.z = clamp(BottomEdgeFades.z, 0.f, 1.f);

		//Vector3 TopEdgeFades = Vector3(Vector3(ClipmapResolution, ClipmapResolution, ClipmapResolution) - .5f - ProbeCoordFloat) * 1.f;

		//TopEdgeFades.x = clamp(TopEdgeFades.x, 0.f, 1.f);
		//TopEdgeFades.y = clamp(TopEdgeFades.y, 0.f, 1.f);
		//TopEdgeFades.z = clamp(TopEdgeFades.z, 0.f, 1.f);

		//float EdgeFade = min(min(BottomEdgeFades.x, BottomEdgeFades.y, BottomEdgeFades.z), min(TopEdgeFades.x, TopEdgeFades.y, TopEdgeFades.z));

	}

	return bResetState;
}

void RadianceCache::onGraphics3D(RenderDevice* rd, const Array<shared_ptr<Surface>>& surfaceArray)
{
	UpdateRadianceCache(rd);	
}

void RadianceCache::debugDraw() {
	//shared_ptr<Image> radianceProbeWSPositionImg = RadianceProbeWorldPosition->toImage(ImageFormat::RGB32F());
	//Color4 c = NumRadianceProbe->readTexel(0,0);

	//int count = c.r;
	//const float radius = 0.015f;
	//for (int i = 0; i < count; ++i)
	//{
	//	Color4 position;
	//	radianceProbeWSPositionImg->get(Point2int32(i, 0), position);
	//	Color3 color(0.f, 1.f, 1.f);
	//	//color = Color3::fromASRGB(0xff007e);

	//	::debugDraw(std::make_shared<SphereShape>((Vector3)position.rgb(), radius), 0.0f, color * 0.8f, Color4::clear());
	//}
}

void RadianceCache::setupInputs(shared_ptr<Camera> active_camera, 
	shared_ptr<Texture> ssRayOrigins,
	shared_ptr<Texture> ssRayDirections,
	shared_ptr<GBuffer> gbuffer,
	shared_ptr<TriTree> sceneTriTree,
	uint frameCount)
{
	activeCamera = active_camera;
	int numMipMaps = 1;
	const int MaxClipmaps = 6;
	radianceCacheInputs.ReprojectionRadiusScale = 1.5f;
	radianceCacheInputs.ClipmapWorldExtent = 20.f;
	radianceCacheInputs.ClipmapDistributionBase = 2.f;
	radianceCacheInputs.RadianceProbeClipmapResolution = iClamp(64, 1, 256);
	radianceCacheInputs.ProbeAtlasResolutionInProbes = Vector2int32(128, 128);
	radianceCacheInputs.NumRadianceProbeClipmaps = iClamp(4, 1, MaxClipmaps);
	radianceCacheInputs.RadianceProbeResolution = 16;
	radianceCacheInputs.FinalProbeResolution = 16 + 2 * (1 << (numMipMaps - 1));
	radianceCacheInputs.FinalRadianceAtlasMaxMip = numMipMaps - 1;
	radianceCacheInputs.CalculateIrradiance = 0;
	radianceCacheInputs.IrradianceProbeResolution = 6;
	radianceCacheInputs.OcclusionProbeResolution = 16;
	radianceCacheInputs.NumProbesToTraceBudget = 200;
	radianceCacheInputs.RadianceCacheStats = 0;
	radianceCacheInputs.InvClipmapFadeSize = 1.0f / clamp(1.f, .001f, 16.0f);
	radianceCacheInputs.NumFramesToKeepCachedProbes = 4;
	m_specification.finalRadianceAtlasExtent = radianceCacheInputs.ProbeAtlasResolutionInProbes * radianceCacheInputs.RadianceProbeResolution;
	m_specification.depthSharpness = 50.0f;
	m_specification.hysteresis = 0.98f;

	m_ssRayOrigins = ssRayOrigins;
	m_ssRayDirections = ssRayDirections;
	m_gbuffer = gbuffer;
	m_sceneTriTree = sceneTriTree;
	this->frameCount = frameCount;
}

void RadianceCache::UpdateRadianceCache(RenderDevice* rd) {
	
	//if (frameCount > 3) return;
	Array<RadianceCacheClipmap> lastFrameClipmap;
	bool firstFrame = frameCount <= 1;
	if (!firstFrame) {
		lastFrameClipmap.resize(radianceCacheState.clipmaps.size());
		lastFrameClipmap.copyFrom(radianceCacheState.clipmaps);
	}
	
	bool resizedHistoryState = UpdateRadianceCacheState(activeCamera, radianceCacheInputs, radianceCacheState);

	//TODO: depthProbeAtlas texture
	SetupArgs();

	const Vector2int32 FinalRadianceAtlasSize = m_specification.finalRadianceAtlasExtent;
	float ScreenProbeDownsampleFactor = 16;
	float fraction = 0.5;

	//clear indirection
	{
		Args args;
		Vector3int32 extent(m_radianceProbeIndirectionTexture->width(), m_radianceProbeIndirectionTexture->height(), m_radianceProbeIndirectionTexture->depth());
		Vector3int32 groupsize(4, 4, 4); //groupsize = 4
		args.setComputeGroupSize(groupsize);
		args.setComputeGridDim(extent / groupsize);
		args.setImageUniform("RWRadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE, false);
		LAUNCH_SHADER("shaders/WorldSpaceProbe_ClearProbeIndirect.glc", args);
	}

	const int ClipmapCount = radianceCacheState.clipmaps.size();
	auto Clipmaps = radianceCacheState.clipmaps;
	Array<Vector4> world2probeData;
	Array<Vector4> probe2worldData;
	Array<Vector4> lastFrameWorld2probeData;
	Array<Vector4> lastFrameProbe2worldData;
	world2probeData.resize(ClipmapCount);
	probe2worldData.resize(ClipmapCount);
	lastFrameWorld2probeData.resize(ClipmapCount);
	lastFrameProbe2worldData.resize(ClipmapCount);
	for (int i = 0; i < ClipmapCount; i++)
	{
		world2probeData[i] = Vector4(Clipmaps[i].WorldPositionToProbeCoordBias, Clipmaps[i].WorldPositionToProbeCoordScale);
		probe2worldData[i] = Vector4(Clipmaps[i].ProbeCoordToWorldCenterBias, Clipmaps[i].ProbeCoordToWorldCenterScale);

		if (!firstFrame) {
			lastFrameWorld2probeData[i] = Vector4(lastFrameClipmap[i].WorldPositionToProbeCoordBias, lastFrameClipmap[i].WorldPositionToProbeCoordScale);
			lastFrameProbe2worldData[i] = Vector4(lastFrameClipmap[i].ProbeCoordToWorldCenterBias, lastFrameClipmap[i].ProbeCoordToWorldCenterScale);
		}

	}
	BEGIN_PROFILER_EVENT("markUsedProbes");
	//mark used probes

	shared_ptr<GLPixelTransferBuffer> worldPositionToRadianceProbeCoordForMark = GLPixelTransferBuffer::create(ClipmapCount, 1, ImageFormat::RGBA32F(), world2probeData.getCArray());
	shared_ptr<GLPixelTransferBuffer> radianceProbeCoordToWorldPosition = GLPixelTransferBuffer::create(ClipmapCount, 1, ImageFormat::RGBA32F(), probe2worldData.getCArray());
	{

		worldPositionToRadianceProbeCoordForMark->bindAsShaderStorageBuffer(0);
		radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(1);

		Args args;
		Vector3int32 groupSize(16, 16, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(Vector3int32(m_ssRayOrigins->width(), m_ssRayOrigins->height(), 1) / groupSize);
		args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE, false);
		//args.setImageUniform("testRadianceProbeIndirectionTexture", testRadianceIndirect, Access::READ_WRITE, false);
		args.setUniform("NumRadianceProbeClipmapsForMark", ClipmapCount);
		args.setUniform("RadianceProbeClipmapResolutionForMark", radianceCacheInputs.RadianceProbeClipmapResolution);
		args.setUniform("InvClipmapFadeSizeForMark", radianceCacheInputs.InvClipmapFadeSize);

		args.setUniform("ws_positionTexture", m_gbuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
		args.setUniform("ws_normalTexture", m_gbuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());
		/*

		uniform int NumRadianceProbeClipmapsForMark;
		uniform uint RadianceProbeClipmapResolutionForMark;
		uniform float InvClipmapFadeSizeForMark;

		// Texture
		uniform sampler2D   ws_positionTexture;
		uniform sampler2D   ws_normalTexture;

		*/

		LAUNCH_SHADER("shaders/WorldSpaceProbePlacement.glc", args);
	}
	END_PROFILER_EVENT();
		
	BEGIN_PROFILER_EVENT("clear free list");
	//clear free list
	{
		Args args;
		Vector3int32 groupSize(1, 1, 1);
		Vector3int32 extent(MaxRadianceProbeCount, 1, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setImageUniform("RadianceProbeFreeList", m_radianceProbeFreeListTexture, Access::READ_WRITE, false);
		args.setImageUniform("LastFrameUsedProbe", m_probeLastUsedFrameTexture, Access::READ_WRITE, false);
		args.setImageUniform("ProbeFreeListAllocator", m_probeFreeListAllocator, Access::READ_WRITE, false);
		args.setImageUniform("ProbeWorldOffset", ProbeWorldOffset, Access::READ_WRITE, false);
		args.setUniform("MaxProbeCount", MaxRadianceProbeCount);
		args.setMacro("FirstFrame", firstFrame);
		LAUNCH_SHADER("shaders/WorldSpaceProbeClearFreeList.glc", args);

	}
	END_PROFILER_EVENT();
	BEGIN_PROFILER_EVENT("UpdateClipmapIndirect");
	shared_ptr<GLPixelTransferBuffer> lastFrameRadianceProbeCoordToWorldPosition = GLPixelTransferBuffer::create(ClipmapCount, 1, ImageFormat::RGBA32F(), firstFrame ? probe2worldData.getCArray() : lastFrameProbe2worldData.getCArray());
	//update cache for last frame used probes
	{

		worldPositionToRadianceProbeCoordForMark->bindAsShaderStorageBuffer(0);
		radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(1);
		lastFrameRadianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(2);

			
		Args args;
		Vector3int32 groupSize(4,4,4);
		Vector3int32 extent(m_radianceProbeIndirectionTexture->width(), m_radianceProbeIndirectionTexture->height(), m_radianceProbeIndirectionTexture->depth());
		if (!testTexture)testTexture = Texture::createEmpty("RadianceCache::test", 128, 128, ImageFormat::RGBA32F());
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setImageUniform("ProbeFreeListAllocator", m_probeFreeListAllocator, Access::READ_WRITE);
		args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE);
		args.setImageUniform("LastFrameRadianceProbeIndirectionTexture", m_lastFrameProbeIndirectionTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeLastUsedFrame", m_probeLastUsedFrameTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeFreeList", m_radianceProbeFreeListTexture, Access::READ_WRITE);
		args.setImageUniform("reverseProbeIndex", testTexture, Access::READ_WRITE);

		args.setUniform("FrameNumber", frameCount);

		args.setUniform("NumFramesToKeepCachedProbes", radianceCacheInputs.NumFramesToKeepCachedProbes);
		args.setUniform("RadianceProbeClipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);

		args.setUniform("NumRadianceProbeClipmapsForMark", ClipmapCount);
		args.setUniform("RadianceProbeClipmapResolutionForMark", radianceCacheInputs.RadianceProbeClipmapResolution);
		args.setUniform("InvClipmapFadeSizeForMark", radianceCacheInputs.InvClipmapFadeSize);
		args.setUniform("MaxProbeCount", MaxRadianceProbeCount);
			
		LAUNCH_SHADER("shaders/WorldSpaceProbeUpdateIndirection.glc", args);
			
			
	}
	END_PROFILER_EVENT();

	BEGIN_PROFILER_EVENT("allocateTraces");
	float FirstClipmapWorldExtentRcp = 1.0f / max(radianceCacheInputs.ClipmapWorldExtent, 1.0f);
	//clear update resources, prepare to choose probes for actual traces
	{

		m_maxUpdateBucket->bindAsShaderStorageBuffer(0);
		Args args;
		Vector3int32 groupSize(1, 1, 1);
		Vector3int32 extent(PRIORITY_HISTOGRAM_SIZE, 1, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);

		args.setImageUniform("PriorityHistogram", m_priorityHistogram, Access::WRITE);
		args.setImageUniform("ProbeTraceAllocator", m_probeTraceAllocator, Access::WRITE);
		args.setImageUniform("MaxTracesFromMaxUpdateBucket", m_maxTracesFromMaxUpdateBucket, Access::WRITE);
		args.setImageUniform("ProbesToUpdateTraceCost", m_probesToUpdateTraceCost, Access::WRITE);


		LAUNCH_SHADER("shaders/WorldSpaceProbeClearUpdateResources.glc", args);
	}


	//allocate actual used probes and update their priority histogram
	{
		radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(0);

		Args args;
		Vector3int32 groupSize(4, 4, 4);
		Vector3int32 extent(m_radianceProbeIndirectionTexture->width(), m_radianceProbeIndirectionTexture->height(), m_radianceProbeIndirectionTexture->depth());
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);

		args.setImageUniform("PriorityHistogram", m_priorityHistogram, Access::READ_WRITE);
		args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeAllocator", m_probeAllocator, Access::READ_WRITE);
		args.setImageUniform("ProbeFreeListAllocator", m_probeFreeListAllocator, Access::READ_WRITE);
		args.setImageUniform("ProbeFreeList", m_radianceProbeFreeListTexture, Access::READ_WRITE);

		args.setImageUniform("ProbeLastUsedFrame", m_probeLastUsedFrameTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeLastTracedFrame", m_probeLastTracedFrameTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeWorldOffset", ProbeWorldOffset, Access::READ_WRITE);

			
		args.setUniform("FrameNumber", frameCount);
		args.setUniform("MaxProbeCount", MaxRadianceProbeCount);
		args.setUniform("RadianceProbeClipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);

		args.setUniform("FirstClipmapWorldExtentRcp", FirstClipmapWorldExtentRcp);
		args.setUniform("DownsampleDistanceFromCameraSq", RadianceCacheDownsampleDistanceFromCamera * RadianceCacheDownsampleDistanceFromCamera);
		args.setUniform("SupersampleDistanceFromCameraSq", RadianceCacheSupersampleDistanceFromCamera * RadianceCacheSupersampleDistanceFromCamera);
		args.setMacro("PRIORITY_HISTOGRAM_SIZE", PRIORITY_HISTOGRAM_SIZE);
		args.setUniform("WorldCameraOrigin", activeCamera->frame().translation);
		args.setUniform("NumProbesToTraceBudget", radianceCacheInputs.NumProbesToTraceBudget);

		LAUNCH_SHADER("shaders/WorldSpaceProbeAllocateUsedProbe.glc", args);
			
	}


	// select max histogram index from bucket
	{
		m_maxUpdateBucket->bindAsShaderStorageBuffer(0);
		Args args;
		Vector3int32 groupSize(1, 1, 1);
		Vector3int32 extent(1, 1, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setImageUniform("PriorityHistogram", m_priorityHistogram, Access::READ_WRITE);
		
		args.setImageUniform("MaxTracesFromMaxUpdateBucket", m_maxTracesFromMaxUpdateBucket, Access::WRITE);
		args.setMacro("PRIORITY_HISTOGRAM_SIZE", PRIORITY_HISTOGRAM_SIZE);
		args.setUniform("NumProbesToTraceBudget", radianceCacheInputs.NumProbesToTraceBudget);

		LAUNCH_SHADER("shaders/WorldSpaceProbeSelectMaxHistogram.glc", args);
		uint data[1];
		m_maxUpdateBucket->getData(data);
		Vector4 rad[4];
		radianceProbeCoordToWorldPosition->getData(&rad);
	}

	// allocateTraces
	{

		radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(0);
		m_maxUpdateBucket->bindAsShaderStorageBuffer(1);


		Args args;
		Vector3int32 groupSize(4, 4, 4);
		Vector3int32 extent(m_radianceProbeIndirectionTexture->width(), m_radianceProbeIndirectionTexture->height(), m_radianceProbeIndirectionTexture->depth());
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);

		args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeTraceAllocator", m_probeTraceAllocator, Access::READ_WRITE);
		args.setImageUniform("MaxTracesFromMaxUpdateBucket", m_maxTracesFromMaxUpdateBucket, Access::READ);
		args.setImageUniform("ProbesToUpdateTraceCost", m_probesToUpdateTraceCost, Access::READ_WRITE);

		args.setImageUniform("ProbeLastUsedFrame", m_probeLastUsedFrameTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeLastTracedFrame", m_probeLastTracedFrameTexture, Access::READ_WRITE);
		args.setImageUniform("ProbeTraceData", m_probeTraceData, Access::READ_WRITE);

		args.setUniform("FrameNumber", frameCount);
		args.setUniform("MaxProbeCount", MaxRadianceProbeCount);
		args.setUniform("RadianceProbeClipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);

		args.setUniform("FirstClipmapWorldExtentRcp", FirstClipmapWorldExtentRcp);
		args.setUniform("DownsampleDistanceFromCameraSq", RadianceCacheDownsampleDistanceFromCamera* RadianceCacheDownsampleDistanceFromCamera);
		args.setUniform("SupersampleDistanceFromCameraSq", RadianceCacheSupersampleDistanceFromCamera* RadianceCacheSupersampleDistanceFromCamera);
		args.setMacro("PRIORITY_HISTOGRAM_SIZE", PRIORITY_HISTOGRAM_SIZE);
		args.setUniform("WorldCameraOrigin", activeCamera->frame().translation);
		args.setUniform("NumProbesToTraceBudget", radianceCacheInputs.NumProbesToTraceBudget);

		LAUNCH_SHADER("shaders/WorldSpaceProbeAllocateTraces.glc", args);

	}
	END_PROFILER_EVENT();
	uint traceAllocated = m_probeTraceAllocator->readTexel(0,0).r;
	shared_ptr<GLPixelTransferBuffer> rayOrigins = GLPixelTransferBuffer::create(traceAllocated, radianceCacheInputs.RadianceProbeResolution * radianceCacheInputs.RadianceProbeResolution, ImageFormat::RGBA32F());
	shared_ptr<GLPixelTransferBuffer> rayDirections = GLPixelTransferBuffer::create(traceAllocated, radianceCacheInputs.RadianceProbeResolution * radianceCacheInputs.RadianceProbeResolution, ImageFormat::RGBA32F());

	int AtlasResolutionInProbe = radianceCacheInputs.ProbeAtlasResolutionInProbes.x;

	BEGIN_PROFILER_EVENT("generateRays");
	//clear trace data
	{
		rayMarker->bindAsShaderStorageBuffer(0);
		m_rayHitLocation->bindAsShaderStorageBuffer(1);
		m_rayHitNormal->bindAsShaderStorageBuffer(2);
		m_rayHitLambertian->bindAsShaderStorageBuffer(3);
		m_rayHitGlossy->bindAsShaderStorageBuffer(4);
		m_rayHitEmissive->bindAsShaderStorageBuffer(5);
		Args args;
		Vector3int32 groupSize(4, 4, 1);
		Vector3int32 extent(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setMacro("AtlasResolutionInProbe", AtlasResolutionInProbe);
		args.setMacro("RadianceProbeResolution", radianceCacheInputs.RadianceProbeResolution);
		args.setImageUniform("RayOrigins", m_radianceRayOrigins);
		args.setImageUniform("RayDirections", m_radianceRayDirections);
		LAUNCH_SHADER("shaders/WorldSpaceProbeClearTraceResources.glc", args);
	}
	int actualRayWidth = rayOrigins->width();
	int rayCount = rayOrigins->height();
	//generate rays
	{
		rayOrigins->bindAsShaderStorageBuffer(0);
		rayDirections->bindAsShaderStorageBuffer(1);
		rayMarker->bindAsShaderStorageBuffer(2);

		Args args;
		Vector3int32 groupSize(4, 4, 1);
		Vector3int32 extent(actualRayWidth, rayCount, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setImageUniform("iRayOrigins", m_radianceRayOrigins, Access::WRITE);
		args.setImageUniform("iRayDirections", m_radianceRayDirections, Access::WRITE);
		args.setImageUniform("ProbeTraceData", m_probeTraceData, Access::READ);
		args.setImageUniform("ProbeTraceAllocator", m_probeTraceAllocator, Access::READ);

		args.setMacro("RadianceProbeResolution", radianceCacheInputs.RadianceProbeResolution);
		args.setMacro("AtlasResolutionInProbe", AtlasResolutionInProbe);
		args.setUniform("randomOrientation", Matrix3::fromAxisAngle(Vector3::random(), Random::common().uniform(0.f, 2 * pif())));

		LAUNCH_SHADER("shaders/WorldSpaceProbeGenerateRays.glc", args);
			
	}
	//trace
	uint a = m_probeLastUsedFrameTexture->readTexel(0, 0).r;
	uint b = m_probeLastTracedFrameTexture->readTexel(0, 0).r;
		
	for (int i = 0; i < 5; ++i)
	{
		if (!RTOutBuffers[i]) {
			switch (i) {
			case 2:
			case 3:
				RTOutBuffers[i] = GLPixelTransferBuffer::create(actualRayWidth, rayCount, ImageFormat::RGBA8());// , nullptr, 1, GL_STREAM_DRAW);
				break;
			default:
				RTOutBuffers[i] = GLPixelTransferBuffer::create(actualRayWidth, rayCount, ImageFormat::RGBA32F());// , nullptr, 1, GL_STREAM_DRAW);
			}
		} else {
			RTOutBuffers[i]->resize(actualRayWidth, rayCount);
		}
			
	}
	m_sceneTriTree->intersectRays(rayOrigins, rayDirections, RTOutBuffers);
		
	//prepare shading data
	{

		rayMarker->bindAsShaderStorageBuffer(0);
		RTOutBuffers[0]->bindAsShaderStorageBuffer(1); //rayHitPosition
		RTOutBuffers[1]->bindAsShaderStorageBuffer(2); //rayHitNormal
		RTOutBuffers[2]->bindAsShaderStorageBuffer(3);
		RTOutBuffers[3]->bindAsShaderStorageBuffer(4); //depthTexture
		RTOutBuffers[4]->bindAsShaderStorageBuffer(5); 

		
		m_rayHitLocation->bindAsShaderStorageBuffer(6);
		m_rayHitNormal->bindAsShaderStorageBuffer(7);
		m_rayHitLambertian->bindAsShaderStorageBuffer(8);
		m_rayHitGlossy->bindAsShaderStorageBuffer(9);
		m_rayHitEmissive->bindAsShaderStorageBuffer(10);
		
		Args args;
		Vector3int32 groupSize(4, 4, 1);
		Vector3int32 extent(actualRayWidth, rayCount, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setImageUniform("ProbeTraceData", m_probeTraceData, Access::READ);
		args.setImageUniform("ProbeTraceAllocator", m_probeTraceAllocator, Access::READ);
		args.setMacro("RadianceProbeResolution", radianceCacheInputs.RadianceProbeResolution);
		args.setMacro("AtlasResolutionInProbe", radianceCacheInputs.ProbeAtlasResolutionInProbes.x);

		LAUNCH_SHADER("shaders/WorldSpaceProbePrepareShadingResources.glc", args);
			
			
	}

	END_PROFILER_EVENT();

	BEGIN_PROFILER_EVENT("RayShading");
	Rect2D atlasRect = Rect2D(Vector2(m_specification.finalRadianceAtlasExtent.x, m_specification.finalRadianceAtlasExtent.y));
	//shading
	{
		//if(!testTexture)
		//testTexture = Texture::createEmpty("RadianceCache::test", 128, 128, ImageFormat::RG32UI());
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_POSITION)->update(m_rayHitLocation);
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_NORMAL)->update(m_rayHitNormal);
		m_radianceRaysGBuffer->texture(GBuffer::Field::LAMBERTIAN)->update(m_rayHitLambertian);
		m_radianceRaysGBuffer->texture(GBuffer::Field::GLOSSY)->update(m_rayHitGlossy);
		m_radianceRaysGBuffer->texture(GBuffer::Field::EMISSIVE)->update(m_rayHitEmissive);
		//testTexture->update(rayMarker);


		//shade hitpoint use previous radianceCache

		rd->push2D(m_radianceRaysIndirectFB); {
			rd->setGuardBandClip2D(m_radianceRaysGBuffer->colorGuardBandThickness());
			// Don't shade the skybox on this pass because it will be forward rendered
			rd->setDepthTest(RenderDevice::DEPTH_GREATER);

			rayMarker->bindAsShaderStorageBuffer(0);
			worldPositionToRadianceProbeCoordForMark->bindAsShaderStorageBuffer(1);
			radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(2);
			Args args;
			m_radianceRaysGBuffer->setShaderArgsRead(args, "gbuffer_");
			args.setRect(atlasRect);
			//setShaderArgs(args, "irradianceFieldSurface.");
			m_radianceRayOrigins->setShaderArgs(args, "gbuffer_WS_RAY_ORIGIN_", Sampler::buffer());
			m_RadianceCache->setShaderArgs(args, "m_RadianceCache_", Sampler::buffer());
			m_RadianceCacheDepth->setShaderArgs(args, "m_RadianceCacheDepth_", Sampler::buffer());
			args.setUniform("ws_positionTexture", m_radianceRaysGBuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
			//args.setUniform("depthTexture", m_radianceRaysGBuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL), Sampler::buffer());
			args.setUniform("ws_normalTexture", m_radianceRaysGBuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());
			args.setUniform("energyPreservation", m_specification.energyPreservation);
			args.setUniform("normalBias", m_specification.normalBias);
			args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture);
			args.setUniform("InvClipmapFadeSize", radianceCacheInputs.InvClipmapFadeSize);
			args.setUniform("RadianceProbeClipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);
			LAUNCH_SHADER("shaders/WorldSpaceProbeComputeIndirect.pix", args);
		}rd->pop2D();

		rd->push2D(m_radianceRaysRadianceFB); {
			// Disable screen-space effects. Note that this is a COPY we're making in order to mutate it
			LightingEnvironment e = m_scene->lightingEnvironment();
			e.ambientOcclusionSettings.enabled = false;

			Args args;
			e.setShaderArgs(args);
			m_radianceRaysGBuffer->setShaderArgsRead(args, "gbuffer_");
			args.setRect(atlasRect);

			args.setMacro("GLOSSY_TO_MATTE", true);
			args.setUniform("raysIndirectBuffer", !firstFrame ? m_radianceRaysIndirectFB->texture(0) : Texture::opaqueBlack(), Sampler::buffer());
			args.setMacro("LIGHTING_MODE", LightMode::DIRECT_INDIRECT);

			args.setMacro("OVERRIDE_SKYBOX", true);
			if (skyBox) skyBox->setShaderArgs(args, "skybox_");

			// When rendering the probes, we don't have ray traced glossy reflections at the probe's primary ray hits,
			// so use the environment map (won't matter, because we usually kill all glossy reflection for irradiance
			// probes anyway since it is so viewer dependent).
			args.setMacro("USE_GLOSSY_INDIRECT_BUFFER", false);
			m_radianceRayOrigins->setShaderArgs(args, "gbuffer_WS_RAY_ORIGIN_", Sampler::buffer());
			m_radianceRayDirections->setShaderArgs(args, "gbuffer_WS_RAY_DIRECTION_", Sampler::buffer());

			LAUNCH_SHADER("shaders/WorldSpaceProbeDeferredShading.pix", args);
		} rd->pop2D();

		END_PROFILER_EVENT();

		BEGIN_PROFILER_EVENT("updateRadianceCache");
#ifndef compute
		updateRadianceCache(rd, atlasRect, true);
		updateRadianceCache(rd, atlasRect, false);

#else
		//rd->setBlendFunc(RenderDevice::BLEND_SRC_ALPHA, RenderDevice::BLEND_ONE_MINUS_SRC_ALPHA);
		//// Set the depth test to discard the border pixels
		//rd->setDepthTest(RenderDevice::DepthTest::DEPTH_GREATER);
		Args args;
		Vector3int32 groupSize(radianceCacheInputs.RadianceProbeResolution, radianceCacheInputs.RadianceProbeResolution, 1);
		Vector3int32 extent(m_specification.finalRadianceAtlasExtent.x, m_specification.finalRadianceAtlasExtent.y, 1);
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent);
			
		args.setUniform("depthSharpness", m_specification.depthSharpness);
		args.setImageUniform("RadianceCache", m_RadianceCache);
		args.setImageUniform("RadianceCacheDepth", m_RadianceCacheDepth);
		// Uniforms to compute texel to direction and back in oct format
		args.setMacro("RadianceProbeResolution", radianceCacheInputs.RadianceProbeResolution);
		//args.setMacro("AtlasResolutionInProbe", radianceCacheInputs.ProbeAtlasResolutionInProbes);
		//args.setUniform("probeSideLength", irradiance ? irradianceOctSideLength() : depthOctSideLength());
		args.setUniform("maxDistance", m_maxDistance);
		args.setUniform("energyConservation", 0.95f);
		//->setShaderArgs(args, "rayHitLocations.", Sampler::buffer());
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_NORMAL)->setShaderArgs(args, "rayHitNormals.", Sampler::buffer());
		m_radianceRaysGBuffer->texture(GBuffer::Field::WS_POSITION)->setShaderArgs(args, "rayHitLocations.", Sampler::buffer());

		m_radianceRayOrigins->setShaderArgs(args, "rayOrigins.", Sampler::buffer());
		m_radianceRayDirections->setShaderArgs(args, "rayDirections.", Sampler::buffer());
		m_radianceRaysRadianceFB->texture(0)->setShaderArgs(args, "rayHitRadiance.", Sampler::buffer());

		// Set skybox args to read on miss
		dynamic_pointer_cast<Skybox>(m_scene->entity("skybox"))->keyframeArray()[0]->setShaderArgs(args, "skybox_", Sampler::defaults());

		//args.setMacro("OUTPUT_IRRADIANCE", irradiance);
		LAUNCH_SHADER("shaders/WorldSpaceProbeUpdateRadiance.glc", args);
		
	//rd->pop2D();

#endif
		END_PROFILER_EVENT();
	}
		
	{
		shared_ptr<GLPixelTransferBuffer>& worldProbePosition = GLPixelTransferBuffer::create(MaxRadianceProbeCount, 1, ImageFormat::RGBA32F());
		//shared_ptr<GLPixelTransferBuffer>& numWorldProbe = GLPixelTransferBuffer::create(1, 1, ImageFormat::RGB32I());

		shared_ptr<GLPixelTransferBuffer>& worldPositionToRadianceProbeCoordForMark = GLPixelTransferBuffer::create(ClipmapCount, 1, ImageFormat::RGBA32F(), world2probeData.getCArray());
		shared_ptr<GLPixelTransferBuffer>& radianceProbeCoordToWorldPosition = GLPixelTransferBuffer::create(ClipmapCount, 1, ImageFormat::RGBA32F(), probe2worldData.getCArray());

		/*
		layout( local_size_variable ) in;
		layout(std430, binding = 0) buffer worldSpacePosition{vec3 worldspacePositionData[];};

		layout(r32ui) uniform uimage3D RadianceProbeIndirectionTexture;

		layout (binding = 1, offset = 0) uniform atomic_uint  probeCount;
		layout(std430, binding=2) buffer worldPositionToRadianceProbeCoordForMark {vec4 WorldPositionToRadianceProbeCoordForMark[];};

		layout(std430, binding=3) buffer radianceProbeCoordToWorldPosition {vec4 RadianceProbeCoordToWorldPosition[];};

		uniform int clipmapResolution;
		*/

		worldProbePosition->bindAsShaderStorageBuffer(0);
		worldPositionToRadianceProbeCoordForMark->bindAsShaderStorageBuffer(2);
		radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(3);
		//numWorldProbe->bindAsShaderStorageBuffer(4);
		Args args;
		Vector3int32 groupSize(4, 4, 4);
		Vector3int32 extent(m_radianceProbeIndirectionTexture->width(), m_radianceProbeIndirectionTexture->height(), m_radianceProbeIndirectionTexture->depth());
		args.setComputeGroupSize(groupSize);
		args.setComputeGridDim(extent / groupSize);
		args.setUniform("probeCount", 0);
		args.setUniform("clipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);
		args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture, Access::READ_WRITE, false);
		args.setImageUniform("numWorldSpacePosition", NumRadianceProbe, Access::READ_WRITE, false);
		LAUNCH_SHADER("shaders/WorldSpaceProbe_Gather.glc", args);

		RadianceProbeWorldPosition->update(worldProbePosition);
		//NumRadianceProbe->update(numWorldProbe);
	}
		
	{
		if (!m_radianceHitPointIndirectFB) {
			m_radianceHitPointIndirectFB = Framebuffer::create(Texture::createEmpty(
				"RadianceCache::OutIndirect",
				m_ssRayOrigins->width(),
				m_ssRayOrigins->height(),
				ImageFormat::RGBA32F()));
		}
		rd->push2D(m_radianceHitPointIndirectFB); {
			rd->setGuardBandClip2D(m_gbuffer->colorGuardBandThickness());
			// Don't shade the skybox on this pass because it will be forward rendered
			rd->setDepthTest(RenderDevice::DEPTH_GREATER);

			rayMarker->bindAsShaderStorageBuffer(0);
			worldPositionToRadianceProbeCoordForMark->bindAsShaderStorageBuffer(1);
			radianceProbeCoordToWorldPosition->bindAsShaderStorageBuffer(2);
			Args args;
			m_gbuffer->setShaderArgsRead(args, "gbuffer_");
			args.setRect(m_ssRayOrigins->rect2DBounds());
			m_ssRayOrigins->setShaderArgs(args, "gbuffer_WS_RAY_ORIGIN_", Sampler::buffer());
			m_RadianceCache->setShaderArgs(args, "m_RadianceCache_", Sampler::buffer());
			m_RadianceCacheDepth->setShaderArgs(args, "m_RadianceCacheDepth_", Sampler::buffer());
			args.setUniform("ws_positionTexture", m_gbuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
			//args.setUniform("depthTexture", m_radianceRaysGBuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL), Sampler::buffer());
			args.setUniform("ws_normalTexture", m_gbuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());
			args.setMacro("OUT_INDIRECT", true);
			args.setUniform("energyPreservation", m_specification.energyPreservation);
			args.setUniform("normalBias", m_specification.normalBias);
			args.setImageUniform("RadianceProbeIndirectionTexture", m_radianceProbeIndirectionTexture);
			args.setUniform("InvClipmapFadeSize", radianceCacheInputs.InvClipmapFadeSize);
			args.setUniform("RadianceProbeClipmapResolution", radianceCacheInputs.RadianceProbeClipmapResolution);
			LAUNCH_SHADER("shaders/WorldSpaceProbeComputeIndirect.pix", args);
		}rd->pop2D();
	}

	//update args
	{
		m_lastFrameProbeIndirectionTexture->update(m_radianceProbeIndirectionTexture->toPixelTransferBuffer());
	}

	
	
}

void RadianceCache::SetupArgs() {

	const Vector3int32 RadianceProbeIndirectionTextureSize = Vector3int32(
		radianceCacheInputs.RadianceProbeClipmapResolution * radianceCacheInputs.NumRadianceProbeClipmaps,
		radianceCacheInputs.RadianceProbeClipmapResolution,
		radianceCacheInputs.RadianceProbeClipmapResolution);

	if (!m_radianceProbeIndirectionTexture)
		m_radianceProbeIndirectionTexture = Texture::createEmpty(
			"RadianceCache::m_radianceProbeIndirect",
			RadianceProbeIndirectionTextureSize.x,
			RadianceProbeIndirectionTextureSize.y,
			ImageFormat::R32UI(),
			Texture::DIM_3D,
			false,
			RadianceProbeIndirectionTextureSize.z,
			1);
	if (!m_lastFrameProbeIndirectionTexture) {
		m_lastFrameProbeIndirectionTexture = Texture::createEmpty(
			"RadianceCache::m_lastFrameRadianceProbeIndirect",
			RadianceProbeIndirectionTextureSize.x,
			RadianceProbeIndirectionTextureSize.y,
			ImageFormat::R32UI(),
			Texture::DIM_3D,
			false,
			RadianceProbeIndirectionTextureSize.z,
			1);
	}
	MaxRadianceProbeCount = radianceCacheInputs.ProbeAtlasResolutionInProbes.x * radianceCacheInputs.ProbeAtlasResolutionInProbes.y;
	if (!m_radianceProbeFreeListTexture) {
		m_radianceProbeFreeListTexture = Texture::createEmpty(
			"RadianceCache::m_radianceProbeFreeList",
			MaxRadianceProbeCount,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D);
	};

	if (!m_probeLastUsedFrameTexture) {
		m_probeLastUsedFrameTexture = Texture::createEmpty(
			"RadianceCache::m_probeLastUsedFrame",
			MaxRadianceProbeCount,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D);
	};

	if (!m_probeLastTracedFrameTexture) {
		m_probeLastTracedFrameTexture = Texture::createEmpty(
			"RadianceCache::m_probeLastTraceFrame",
			MaxRadianceProbeCount,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D);
	};
	
	if (!m_probeFreeListAllocator) {
		m_probeFreeListAllocator = Texture::createEmpty(
			"RadianceCache::m_probeFreeListAllocator",
			1,
			1,
			ImageFormat::R32I(),
			Texture::DIM_2D
		);
	};

	if (!m_probeAllocator) {
		m_probeAllocator = Texture::createEmpty(
			"RadianceCache::m_probeAllocator",
			1,
			1,
			ImageFormat::R32I(),
			Texture::DIM_2D
		);
	};


	if (!ProbeWorldOffset) {
		ProbeWorldOffset = Texture::createEmpty(
			"RadianceCache::ProbeWorldOffset",
			MaxRadianceProbeCount,
			1,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
	};

	if (!RadianceProbeWorldPosition) {
		RadianceProbeWorldPosition = Texture::createEmpty("RadianceCache::RadianceProbeWorldPosition", 2048, 1, ImageFormat::RGBA32F());

	}
	if (!NumRadianceProbe) {
		NumRadianceProbe = Texture::createEmpty("RadianceCache::NumRadianceProbe", 1, 1, ImageFormat::R32UI());
	}

	if (!m_priorityHistogram) {
		m_priorityHistogram = Texture::createEmpty(
			"RadianceCache::m_priorityHistogram",
			PRIORITY_HISTOGRAM_SIZE,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D
		);
	};
	if (!m_maxUpdateBucket) {
		m_maxUpdateBucket = GLPixelTransferBuffer::create(1, 1, ImageFormat::R32UI());
	};
	if (!m_maxTracesFromMaxUpdateBucket) {
		m_maxTracesFromMaxUpdateBucket = Texture::createEmpty(
			"RadianceCache::m_maxTracesFromMaxUpdateBucket",
			1,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D
		);
	};
	if (!m_probesToUpdateTraceCost) {
		m_probesToUpdateTraceCost = Texture::createEmpty(
			"RadianceCache::m_probesToUpdateTraceCost",
			PROBES_TO_UPDATE_TRACE_COST_STRIDE,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D
		);
	};
	if (!m_probeTraceAllocator) {
		m_probeTraceAllocator = Texture::createEmpty(
			"RadianceCache::m_ProbeTraceAllocator",
			1,
			1,
			ImageFormat::R32UI(),
			Texture::DIM_2D
		);
	};
	if (!m_probeTraceData) {
		m_probeTraceData = Texture::createEmpty(
			"RadianceCache::m_probeTraceData",
			MaxRadianceProbeCount,
			1,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
	}
	if (!m_radianceRayOrigins) {
		m_radianceRayOrigins = Texture::createEmpty(
			"RadianceCache::m_radianceRayOrigins",
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
	};
	if (!m_radianceRayDirections) {
		m_radianceRayDirections = Texture::createEmpty(
			"RadianceCache::m_radianceRayDirections",
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
	};
	if (!m_radianceRaysRadianceFB) {
		m_radianceRaysRadianceFB = Framebuffer::create(Texture::createEmpty("RadianceCache::m_radianceRaysRadianceFB", 
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F()));
		};
	if (!m_radianceRaysIndirectFB) {
		m_radianceRaysIndirectFB = Framebuffer::create(Texture::createEmpty("RadianceCache::m_radianceRaysIndrectFB",
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F()));
	}
	
	if (!rayMarker) {
		rayMarker = GLPixelTransferBuffer::create(
			radianceCacheInputs.ProbeAtlasResolutionInProbes.x,
			radianceCacheInputs.ProbeAtlasResolutionInProbes.y,
			ImageFormat::RG32UI());
		m_rayOrigins = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F());
		m_rayDirections = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F());
		m_rayHitLocation = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F());
		m_rayHitNormal = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F());
		m_rayHitLambertian = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA8());
		m_rayHitGlossy = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA8());
		m_rayHitEmissive = GLPixelTransferBuffer::create(
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F());
	}
	if (!m_radianceRaysGBuffer) {
		const ImageFormat* depthFormat = ImageFormat::DEPTH32();

		GBuffer::Specification gbufferRTSpec;

		gbufferRTSpec.encoding[GBuffer::Field::LAMBERTIAN].format = ImageFormat::RGBA32F();
		gbufferRTSpec.encoding[GBuffer::Field::GLOSSY].format = ImageFormat::RGBA32F();
		gbufferRTSpec.encoding[GBuffer::Field::EMISSIVE].format = ImageFormat::RGBA32F();
		gbufferRTSpec.encoding[GBuffer::Field::TRANSMISSIVE].format = ImageFormat::RGBA32F();
		gbufferRTSpec.encoding[GBuffer::Field::WS_POSITION].format = ImageFormat::RGBA32F();
		gbufferRTSpec.encoding[GBuffer::Field::WS_NORMAL] = Texture::Encoding(ImageFormat::RGBA32F(), FrameName::CAMERA, 1.0f, 0.0f);
		gbufferRTSpec.encoding[GBuffer::Field::DEPTH_AND_STENCIL].format = nullptr;
		gbufferRTSpec.encoding[GBuffer::Field::CS_NORMAL] = nullptr;
		gbufferRTSpec.encoding[GBuffer::Field::CS_POSITION] = nullptr;

		//int rayDimX = probeCount();
		int rayDimX = m_specification.finalRadianceAtlasExtent.x;
		int rayDimY = m_specification.finalRadianceAtlasExtent.y;


		m_radianceRaysGBuffer = GBuffer::create(gbufferRTSpec, "RadianceCache::m_radianceRaysGBuffer");
		m_radianceRaysGBuffer->setSpecification(gbufferRTSpec);
		m_radianceRaysGBuffer->resize(rayDimX, rayDimY);
	}
	if (!m_RadianceCache) {
		m_RadianceCache = Texture::createEmpty(
			"RadianceCache::m_RadianceCache",
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
		m_RadianceCacheDepth = Texture::createEmpty(
			"RadianceCache::m_RadianceCacheDepth",
			m_specification.finalRadianceAtlasExtent.x,
			m_specification.finalRadianceAtlasExtent.y,
			ImageFormat::RGBA32F(),
			Texture::DIM_2D);
	}
	if (!m_radianceCacheFB) {
		m_radianceCacheFB = Framebuffer::create(m_RadianceCache);
	}
	if (!m_radianceMeanDistanceFB) {
		m_radianceMeanDistanceFB = Framebuffer::create(m_RadianceCacheDepth);
	}
}

void RadianceCache::onSceneChanged(const shared_ptr<Scene>& scene)
{
	m_scene = scene;
	m_sceneDirty = true;
	if (surfaceArray.size()!=0) {
		for (const shared_ptr<Surface>& surface : surfaceArray)
		{
			skyBox = dynamic_pointer_cast<SkyboxSurface>(surface);
			if (skyBox) { break; }
		}
	}
}
