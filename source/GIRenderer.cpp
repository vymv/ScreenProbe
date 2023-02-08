#include "GIRenderer.h"

void CGIRenderer::renderDeferredShading(RenderDevice * rd, const Array<shared_ptr<Surface>>& sortedVisibleSurfaceArray, const shared_ptr<GBuffer>& gbuffer, const LightingEnvironment & environment)
{
	if (m_pIrradianceField)
	{
		if (isNull(m_pGIFramebuffer))
		{
			m_pGIFramebuffer = Framebuffer::create("CGIRenderer::m_pGIFramebuffer");
			m_pGIFramebuffer->set(Framebuffer::COLOR0, Texture::createEmpty("CGIRenderer::Indirect", gbuffer->width(), gbuffer->height(), ImageFormat::RGBA32F()));
		}
		m_pGIFramebuffer->resize(gbuffer->width(), gbuffer->height());

		// Compute GI
		rd->push2D(m_pGIFramebuffer); {
			Args args;
			gbuffer->setShaderArgsRead(args, "gbuffer_");
			args.setRect(rd->viewport());
			m_pIrradianceField->setShaderArgs(args, "irradianceFieldSurface.");
			args.setUniform("energyPreservation", 1.0f);

			args.setUniform("screenProbeDownsampleFactor", m_pIrradianceField->screenProbeDownsampleFactor);
			args.setUniform("viewport_height", rd->viewport().height());
			args.setUniform("viewport_width", rd->viewport().width());
			args.setUniform("adaptiveProbeNum", m_pIrradianceField->adaptiveProbeCount);
			args.setUniform("ws_positionTexture", m_pIrradianceField->m_gbuffer->texture(GBuffer::Field::WS_POSITION), Sampler::buffer());
			args.setUniform("depthTexture", m_pIrradianceField->m_gbuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL), Sampler::buffer());
			args.setUniform("ws_normalTexture", m_pIrradianceField->m_gbuffer->texture(GBuffer::Field::WS_NORMAL), Sampler::buffer());
			args.setUniform("adaptiveProbeSSPosData", m_pIrradianceField->screenProbeSSAdaptivePositionTexture, Sampler::buffer());
			args.setUniform("screenTileHeaderData", m_pIrradianceField->screenTileAdaptiveProbeHeaderTexture, Sampler::buffer());
			args.setUniform("screenTileProbeIndex", m_pIrradianceField->screenTileAdaptiveProbeIndicesTexture, Sampler::buffer());
			args.setUniform("maxAdaptiveFactor", m_pIrradianceField->maxAdaptiveFactor());
			LAUNCH_SHADER("shaders/GIRenderer_ComputeIndirect.pix", args);
		} rd->pop2D();
	}

	// Find the skybox
	shared_ptr<SkyboxSurface> skyboxSurface;
	for (const shared_ptr<Surface>& surface : sortedVisibleSurfaceArray)
	{
		skyboxSurface = dynamic_pointer_cast<SkyboxSurface>(surface);
		if (skyboxSurface) { break; }
	}

	rd->push2D(); {
		Args args;
		environment.setShaderArgs(args);
		gbuffer->setShaderArgsRead(args, "gbuffer_");
		args.setRect(rd->viewport());

		args.setUniform("matteIndirectBuffer", notNull(m_pGIFramebuffer) ? m_pGIFramebuffer->texture(0) : Texture::opaqueBlack(), Sampler::buffer());

		args.setMacro("OVERRIDE_SKYBOX", true);
		if (skyboxSurface) skyboxSurface->setShaderArgs(args, "skybox_");

		LAUNCH_SHADER("shaders/GIRenderer_DeferredShade.pix", args);
	} rd->pop2D();
}