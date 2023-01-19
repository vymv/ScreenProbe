#pragma once
#include <G3D/G3D.h>
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
protected:
	friend class App; // This is here for exposing debugging parameters
	struct Specification {
		Vector2int32 finalRadianceAtlasExtent;
	};
	shared_ptr<Camera> activeCamera;
	Specification m_specification;
	RadianceCacheInputs radianceCacheInputs;
	RadianceCacheState radianceCacheState;

	shared_ptr<GBuffer> m_finalRadianceAtlas;

	shared_ptr<GBuffer> m_gbuffer;
	shared_ptr<Texture> m_radianceProbeIndirectionTexture;
	shared_ptr<Texture> m_lastFrameProbeIndirectionTexture;
	shared_ptr<Texture> m_radianceProbeFreeListTexture;
	shared_ptr<Texture> m_probeLastUsedFrameTexture;
	shared_ptr<Texture> m_probeLastTracedFrameTexture;
	shared_ptr<Texture> m_probeFreeListAllocator;
	shared_ptr<Texture> m_probeAllocator;
	shared_ptr<Texture> ProbeWorldOffset;

	//clear update resources
	shared_ptr<Texture> m_priorityHistogram;
	shared_ptr<Texture> m_maxUpdateBucket;
	shared_ptr<Texture> m_maxTracesFromMaxUpdateBucket;
	shared_ptr<Texture> m_probesToUpdateTraceCost;
	shared_ptr<Texture> m_probeTraceAllocator;
	shared_ptr<Texture> m_probeTraceData;

	//mark
	shared_ptr<Texture> AdaptiveProbeWSPosition;
	shared_ptr<Texture> AdaptiveProbeSSPosition;
	shared_ptr<Texture> AdaptiveProbeNum;
	shared_ptr<Texture> WorldPositionToRadianceProbeCoordForMark;
	shared_ptr<Texture> RadianceProbeCoordToWorldPosition;
	shared_ptr<Texture> NumRadianceProbe;
	shared_ptr<Texture> RadianceProbeWorldPosition;

	int MaxRadianceProbeCount;
	int PRIORITY_HISTOGRAM_SIZE = 128;
	int PROBES_TO_UPDATE_TRACE_COST_STRIDE = 2;

	float RadianceCacheDownsampleDistanceFromCamera = 400.0f;
	float RadianceCacheSupersampleDistanceFromCamera = 200.0f;
	uint frameCount;

public:
	bool UpdateRadianceCacheState(shared_ptr<Camera> camera, RadianceCacheInputs& input, RadianceCacheState& cache);
	void UpdateRadianceCache(RenderDevice* rd);
	virtual void onGraphics3D(RenderDevice* rd, const Array<shared_ptr<Surface>>& surfaceArray);
	void debugDraw();
	shared_ptr<RadianceCache> create();
	void setupInputs(shared_ptr<Camera> active_camera,
		shared_ptr<Texture> screenProbeWSAdaptivePositionTexture,
		shared_ptr<Texture> screenProbeSSAdaptivePositionTexture,
		shared_ptr<Texture> numAdaptiveScreenProbesTexture,
		shared_ptr<GBuffer> gbuffer,
		uint frameCount);
	void SetupArgs();
};
