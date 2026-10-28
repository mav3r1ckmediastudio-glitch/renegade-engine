// Isolated proof adapter of pinned Wicked RenderPath3D::RenderTransparents.
// Original: Wicked Engine, Copyright (c) Wicked Engine authors, MIT license.
// Pin 3a800b7134aafe58461093c8abb2e274d4e64033. Only regular transparent visibility order differs.
// Included inside proof Render subclass; production adoption requires separate renderer design.
void RenderTransparents(CommandList cmd) const override
{
        if(!collisionPaths){wi::RenderPath3D::RenderTransparents(cmd);return;}
        constexpr float foreground_depth_range=.01f;
		GraphicsDevice* device = wi::graphics::GetDevice();

		// Water ripple rendering:
		if (!scene->waterRipples.empty())
		{
			device->Barrier(GPUBarrier::Aliasing(&rtParticleDistortion, &rtWaterRipple), cmd);
			RenderPassImage rp[] = {
				RenderPassImage::RenderTarget(&rtWaterRipple, RenderPassImage::LoadOp::CLEAR),
			};
			device->RenderPassBegin(rp, arraysize(rp), cmd);

			Viewport vp;
			vp.width = (float)rtWaterRipple.GetDesc().width;
			vp.height = (float)rtWaterRipple.GetDesc().height;
			device->BindViewports(1, &vp, cmd);

			wi::renderer::DrawWaterRipples(visibility_main, cmd);
			device->RenderPassEnd(cmd);
		}

		if (getFSR2Enabled())
		{
			// Save the pre-alpha for FSR2 reactive mask:
			//	Note that rtFSR temp resource is always larger or equal to rtMain, so CopyTexture is used instead of CopyResource!
			GPUBarrier barriers[] = {
				GPUBarrier::Image(&rtMain, rtMain.desc.layout, ResourceState::COPY_SRC),
				GPUBarrier::Image(&rtFSR[1], rtFSR->desc.layout, ResourceState::COPY_DST),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
			device->CopyTexture(
				&rtFSR[1], 0, 0, 0, 0, 0,
				&rtMain, 0, 0,
				cmd
			);
			for (int i = 0; i < arraysize(barriers); ++i)
			{
				std::swap(barriers[i].image.layout_before, barriers[i].image.layout_after);
			}
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		wi::renderer::UpdateGaussianSplatsForCamera(*scene, *camera, cmd);

		wi::graphics::Rect scissor = GetScissorInternalResolution();
		device->BindScissorRects(1, &scissor, cmd);

		Viewport vp;
		vp.width = (float)depthBuffer_Main.GetDesc().width;
		vp.height = (float)depthBuffer_Main.GetDesc().height;
		vp.min_depth = 0;
		vp.max_depth = 1;
		device->BindViewports(1, &vp, cmd);

		RenderPassImage rp[3];
		uint32_t rp_count = 0;
		if (getMSAASampleCount() > 1)
		{
			rp[rp_count++] = RenderPassImage::RenderTarget(&rtMain_render, RenderPassImage::LoadOp::LOAD);
			rp[rp_count++] = RenderPassImage::Resolve(&rtMain);
			rp[rp_count++] = RenderPassImage::DepthStencil(
				&depthBuffer_Main,
				RenderPassImage::LoadOp::LOAD,
				RenderPassImage::StoreOp::STORE,
				ResourceState::DEPTHSTENCIL,
				ResourceState::DEPTHSTENCIL,
				ResourceState::DEPTHSTENCIL
			);
		}
		else
		{

			rp[rp_count++] = RenderPassImage::RenderTarget(&rtMain_render, RenderPassImage::LoadOp::LOAD);
			rp[rp_count++] = RenderPassImage::DepthStencil(
				&depthBuffer_Main,
				RenderPassImage::LoadOp::LOAD,
				RenderPassImage::StoreOp::STORE,
				ResourceState::DEPTHSTENCIL,
				ResourceState::DEPTHSTENCIL,
				ResourceState::DEPTHSTENCIL
			);
		}

		// Draw only the ocean first, fog and lightshafts will be blended on top:
		if (scene->weather.IsOceanEnabled() && scene->ocean.IsValid() && (!scene->ocean.IsOccluded() || !wi::renderer::GetOcclusionCullingEnabled()))
		{
			device->EventBegin("Copy scene tex only mip0 for ocean", cmd);
			wi::renderer::Postprocess_Downsample4x(rtMain, rtSceneCopy, cmd);
			device->EventEnd(cmd);

			device->RenderPassBegin(rp, rp_count, cmd);

			wi::renderer::DrawScene(
				visibility_main,
				wi::enums::RENDERPASS_MAIN,
				cmd,
				wi::renderer::DRAWSCENE_OCEAN
			);

			device->RenderPassEnd(cmd);
		}

		if (visibility_main.IsTransparentsVisible())
		{
			RenderSceneMIPChain(cmd);
		}

		device->RenderPassBegin(rp, rp_count, cmd);

		// Transparent scene
		if (visibility_main.IsTransparentsVisible())
		{
			auto range = wi::profiler::BeginRangeGPU("Transparent Scene", cmd);
			device->EventBegin("Transparent Scene", cmd);

			// Regular:
			vp.min_depth = 0;
			vp.max_depth = 1;
			device->BindViewports(1, &vp, cmd);
			// Distance sorting cannot order an extended pool against a crossing waterfall.
            // Split local visibility only; preserve native world depth and all subsequent particle passes.
            wi::renderer::Visibility poolVisibility,waterVisibility; for(auto* v:{&poolVisibility,&waterVisibility}){v->scene=visibility_main.scene;v->camera=visibility_main.camera;v->layerMask=visibility_main.layerMask;v->frustum=visibility_main.frustum;}
            poolVisibility.visibleObjects.clear();waterVisibility.visibleObjects.clear();
            auto poolEntity=scene->Entity_FindByName("Optional receiving pool (fixture only)");
            for(auto index:visibility_main.visibleObjects){
                if(scene->objects.GetEntity(index)==poolEntity)poolVisibility.visibleObjects.push_back(index);
                else waterVisibility.visibleObjects.push_back(index);
            }
            auto draw=[&](const wi::renderer::Visibility& visibility){wi::renderer::DrawScene(visibility,wi::enums::RENDERPASS_MAIN,cmd,
                wi::renderer::DRAWSCENE_TRANSPARENT|wi::renderer::DRAWSCENE_TESSELLATION|wi::renderer::DRAWSCENE_MAINCAMERA);};
            draw(poolVisibility);draw(waterVisibility);

			// Foreground:
			vp.min_depth = 1 - foreground_depth_range;
			vp.max_depth = 1;
			device->BindViewports(1, &vp, cmd);
			wi::renderer::DrawScene(
				visibility_main,
				wi::enums::RENDERPASS_MAIN,
				cmd,
				wi::renderer::DRAWSCENE_TRANSPARENT |
				wi::renderer::DRAWSCENE_FOREGROUND_ONLY |
				wi::renderer::DRAWSCENE_MAINCAMERA
			);

			// Reset normal viewport:
			vp.min_depth = 0;
			vp.max_depth = 1;
			device->BindViewports(1, &vp, cmd);

			device->EventEnd(cmd);
			wi::profiler::EndRange(range); // Transparent Scene
		}

		wi::renderer::DrawDebugWorld(*scene, *camera, *this, cmd);

		wi::renderer::DrawWireframeOverlay(visibility_main, wi::enums::RENDERPASS_MAIN, cmd);

		wi::renderer::DrawLightVisualizers(visibility_main, cmd);

		wi::renderer::DrawSoftParticles(visibility_main, false, cmd);
		wi::renderer::DrawGaussianSplats(*scene, *camera, cmd);
		wi::renderer::DrawSpritesAndFonts(*scene, *camera, false, cmd);

		if (getVolumeLightsEnabled() && visibility_main.IsRequestedVolumetricLights())
		{
			device->EventBegin("Contribute Volumetric Lights", cmd);
			wi::renderer::Postprocess_Upsample_Bilateral(
				rtVolumetricLights,
				depthBuffer_Copy,
				rtMain,
				cmd,
				true,
				1.5f
			);
			device->EventEnd(cmd);
		}

		XMVECTOR sunDirection = XMLoadFloat3(&scene->weather.sunDirection);
		if (getLightShaftsEnabled() && XMVectorGetX(XMVector3Dot(sunDirection, camera->GetAt())) > 0)
		{
			device->EventBegin("Contribute LightShafts", cmd);
			wi::image::Params fx;
			fx.enableFullScreen();
			fx.blendFlag = wi::enums::BLENDMODE_ADDITIVE;
			fx.opacity = lightShaftsFadeFactor;
			wi::image::Draw(&rtSun[1], fx, cmd);
			device->EventEnd(cmd);
		}

		if (getLensFlareEnabled())
		{
			wi::renderer::DrawLensFlares(
				visibility_main,
				cmd
			);
		}

		device->RenderPassEnd(cmd);

		// Distortion particles:
		{
			if (rtWaterRipple.IsValid())
			{
				device->Barrier(GPUBarrier::Aliasing(&rtWaterRipple, &rtParticleDistortion), cmd);
			}

			if (getMSAASampleCount() > 1)
			{
				RenderPassImage rp[] = {
					RenderPassImage::RenderTarget(&rtParticleDistortion_render, RenderPassImage::LoadOp::CLEAR),
					RenderPassImage::Resolve(&rtParticleDistortion),
					RenderPassImage::DepthStencil(
						&depthBuffer_Main,
						RenderPassImage::LoadOp::LOAD,
						RenderPassImage::StoreOp::STORE,
						ResourceState::DEPTHSTENCIL,
						ResourceState::DEPTHSTENCIL,
						ResourceState::DEPTHSTENCIL
					),
				};
				device->RenderPassBegin(rp, arraysize(rp), cmd);
			}
			else
			{
				RenderPassImage rp[] = {
					RenderPassImage::RenderTarget(&rtParticleDistortion, RenderPassImage::LoadOp::CLEAR),
					RenderPassImage::DepthStencil(
						&depthBuffer_Main,
						RenderPassImage::LoadOp::LOAD,
						RenderPassImage::StoreOp::STORE,
						ResourceState::DEPTHSTENCIL,
						ResourceState::DEPTHSTENCIL,
						ResourceState::DEPTHSTENCIL
					),
				};
				device->RenderPassBegin(rp, arraysize(rp), cmd);
			}

			Viewport vp;
			vp.width = (float)rtParticleDistortion.GetDesc().width;
			vp.height = (float)rtParticleDistortion.GetDesc().height;
			device->BindViewports(1, &vp, cmd);

			wi::renderer::DrawSoftParticles(visibility_main, true, cmd);
			wi::renderer::DrawSpritesAndFonts(*scene, *camera, true, cmd);

			device->RenderPassEnd(cmd);
		}

		wi::renderer::Postprocess_Downsample4x(rtMain, rtSceneCopy, cmd);
	}