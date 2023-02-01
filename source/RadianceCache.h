#pragma once
#include <G3D/G3D.h>
G3D_DECLARE_ENUM_CLASS(LightMode, DIRECT_INDIRECT, DIRECT_ONLY, INDIRECT_ONLY);
class RadianceCacheClipmap {
public:
	/** World space bounds. */
	Vector3 Center;
	float Extent;

	Vector3 ProbeCoordToWorldCenterBias;
	float ProbeCoordToWorldCenterScale;

	Vector3 WorldPositionToProbeCoordBias;
	float WorldPositionToProbeCoordScale;

	float ProbeTMin;

	/** Offset applied to UVs so that only new or dirty areas of the volume texture have to be updated. */
	Vector3 VolumeUVOffset;

	/* Distance between two probes. */
	float CellSize;
};

struct RadianceCacheInputs {
	float ReprojectionRadiusScale;
	float ClipmapWorldExtent;
	float ClipmapDistributionBase;
	int RadianceProbeClipmapResolution;
	int NumRadianceProbeClipmaps;
	bool CalculateIrradiance;
	float InvClipmapFadeSize;
	Vector2int32 ProbeAtlasResolutionInProbes;
	int RadianceProbeResolution;
	int FinalProbeResolution;
	int FinalRadianceAtlasMaxMip;
	int IrradianceProbeResolution;
	float OcclusionProbeResolution;
	int NumProbesToTraceBudget;
	int RadianceCacheStats;
	int NumFramesToKeepCachedProbes;
};

struct RadianceCacheState {
	Array<RadianceCacheClipmap> clipmaps;
	float ClipmapWorldExtent;
	float ClipmapDistributionBase;
};
struct RadianceCacheInterpolationParameters {
	shared_ptr<Texture> RadianceProbeIndirectionTexture;
};

class RadianceCache : public ReferenceCountedObject {
private:
	void updateRadianceCache(RenderDevice* rd,
		Rect2D atlasRect,
		bool Radiance);
protected:
	friend class App; // This is here for exposing debugging parameters
	struct Specification {
		Vector2int32 finalRadianceAtlasExtent;
		float depthSharpness;
		float hysteresis;
		float normalBias = 0.05f;
		float energyPreservation = 0.95f;
	};
	shared_ptr<Camera> activeCamera;
	Specification m_specification;
	RadianceCacheInputs radianceCacheInputs;
	RadianceCacheState radianceCacheState;

	shared_ptr<Texture> m_RadianceCache;
	shared_ptr<Texture> m_RadianceCacheDepth;

	shared_ptr<Texture> m_radianceProbeIndirectionTexture;
	shared_ptr<Texture> m_lastFrameProbeIndirectionTexture;
	shared_ptr<Texture> m_radianceProbeFreeListTexture;
	shared_ptr<Texture> m_probeLastUsedFrameTexture;
	shared_ptr<Texture> m_probeLastTracedFrameTexture;
	shared_ptr<Texture> m_probeFreeListAllocator;
	shared_ptr<Texture> m_probeAllocator;
	shared_ptr<Texture> ProbeWorldOffset;

	shared_ptr<Framebuffer> m_radianceRaysRadianceFB;

	//clear update resources
	shared_ptr<Texture> m_priorityHistogram;
	shared_ptr<GLPixelTransferBuffer> m_maxUpdateBucket;
	shared_ptr<Texture> m_maxTracesFromMaxUpdateBucket;
	shared_ptr<Texture> m_probesToUpdateTraceCost;
	shared_ptr<Texture> m_probeTraceAllocator;
	shared_ptr<Texture> m_probeTraceData;

	//mark
	shared_ptr<Texture> WorldPositionToRadianceProbeCoordForMark;
	shared_ptr<Texture> RadianceProbeCoordToWorldPosition;
	shared_ptr<Texture> NumRadianceProbe;
	shared_ptr<Texture> RadianceProbeWorldPosition;

	shared_ptr<TriTree> m_sceneTriTree;

	shared_ptr<GLPixelTransferBuffer>   rayMarker;
	shared_ptr<GLPixelTransferBuffer>	m_rayOrigins;
	shared_ptr<GLPixelTransferBuffer>	m_rayDirections;
	shared_ptr<GLPixelTransferBuffer>	m_rayHitNormal;
	shared_ptr<GLPixelTransferBuffer>	m_rayHitLocation;
	shared_ptr<GLPixelTransferBuffer>	m_rayHitLambertian;
	shared_ptr<GLPixelTransferBuffer>	m_rayHitGlossy;
	shared_ptr<GLPixelTransferBuffer>	m_rayHitEmissive;

	shared_ptr<Texture>                 m_radianceRayOrigins;
	shared_ptr<Texture>                 m_radianceRayDirections;
	shared_ptr<Texture>					m_ssRayOrigins;
	shared_ptr<Texture>					m_ssRayDirections;
	shared_ptr<Framebuffer>             m_radianceCacheFB;
	shared_ptr<Framebuffer>				m_radianceRaysIndirectFB;
	shared_ptr<Framebuffer>				m_radianceMeanDistanceFB;

	//shared_ptr<Framebuffer>             m_radianceIn;
	shared_ptr<GBuffer>                 m_radianceRaysGBuffer;
	shared_ptr<GBuffer>					m_gbuffer;
	shared_ptr<Texture> testTexture;
	shared_ptr<GLPixelTransferBuffer> RTOutBuffers[5];

	int MaxRadianceProbeCount;
	int PRIORITY_HISTOGRAM_SIZE = 128;
	int PROBES_TO_UPDATE_TRACE_COST_STRIDE = 2;

	float RadianceCacheDownsampleDistanceFromCamera = 1.0f;
	float RadianceCacheSupersampleDistanceFromCamera = 0.5f;
	float                               m_maxDistance = 4.0f;
	
	uint frameCount;
	shared_ptr<Scene> m_scene;
	shared_ptr<SkyboxSurface> skyBox;
	Array<shared_ptr<Surface>> surfaceArray;
	bool m_sceneDirty;

public:
	RadianceCache();
	bool UpdateRadianceCacheState(shared_ptr<Camera> camera, RadianceCacheInputs& input, RadianceCacheState& cache);
	void UpdateRadianceCache(RenderDevice* rd);
	virtual void onGraphics3D(RenderDevice* rd, const Array<shared_ptr<Surface>>& surfaceArray);
	void debugDraw();
	shared_ptr<RadianceCache> create();
	void setupInputs(shared_ptr<Camera> active_camera,
		shared_ptr<Texture> ssRayOrigins,
		shared_ptr<Texture> ssRayDirections,
		shared_ptr<GBuffer> gbuffer,
		shared_ptr<TriTree> sceneTriTree,
		uint frameCount);
	void SetupArgs();
	virtual void onSceneChanged(const shared_ptr<Scene>& scene);
	shared_ptr<Framebuffer>				m_radianceHitPointIndirectFB;
};
