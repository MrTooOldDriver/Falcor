/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "HelloDXR.h"
#include "Utils/Math/FalcorMath.h"
#include "Utils/UI/TextRenderer.h"

FALCOR_EXPORT_D3D12_AGILITY_SDK

static const float4 kClearColor(0.38f, 0.52f, 0.10f, 1);
static const std::string kDefaultScene = "Arcade/Arcade.pyscene";
// static const std::string kDefaultScene = "test_scenes/animated_cubese/animated_cubese.pyscene";

HelloDXR::HelloDXR(const SampleAppConfig& config) : SampleApp(config) {}

HelloDXR::~HelloDXR() {}

void HelloDXR::onLoad(RenderContext* pRenderContext)
{
    if (getDevice()->isFeatureSupported(Device::SupportedFeatures::Raytracing) == false)
    {
        FALCOR_THROW("Device does not support raytracing!");
    }

    loadScene(kDefaultScene, getTargetFbo().get());
}

void HelloDXR::onResize(uint32_t width, uint32_t height)
{
    float h = (float)height;
    float w = (float)width;

    if (mpCamera)
    {
        mpCamera->setFocalLength(18);
        float aspectRatio = (w / h);
        mpCamera->setAspectRatio(aspectRatio);
    }

    mpRtOut = getDevice()->createTexture2D(
        width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
    );

    mpRtNoShadowOut = getDevice()->createTexture2D(
        width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
    );

    onShadowRenderFactorChange();

    mpShadowUpsampleTexture = getDevice()->createTexture2D(
        width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpOverlapOut = Fbo::create2D(getDevice(), width, height, ResourceFormat::RGBA16Float);
    mpTempFbo1 = Fbo::create2D(getDevice(), width, height, ResourceFormat::RGBA16Float);
    mpTempFbo3 = Fbo::create2D(getDevice(), width, height, ResourceFormat::RGBA16Float);
}

void HelloDXR::onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    pRenderContext->clearFbo(pTargetFbo.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

    if (mpScene)
    {
        Scene::UpdateFlags updates = mpScene->update(pRenderContext, getGlobalClock().getTime());
        if (is_set(updates, Scene::UpdateFlags::GeometryChanged))
            FALCOR_THROW("This sample does not support scene geometry changes.");
        if (is_set(updates, Scene::UpdateFlags::RecompileNeeded))
            FALCOR_THROW("This sample does not support scene changes that require shader recompilation.");

        if (mRayTrace)
            if (mUseCoarsePixelShading)
            {
                renderCoarsePixelShadingRT(pRenderContext, pTargetFbo);
            }
            else
            {
                renderRT(pRenderContext, pTargetFbo);
            }
        else
            renderRaster(pRenderContext, pTargetFbo);
    }

    getTextRenderer().render(pRenderContext, getFrameRate().getMsg(), pTargetFbo, {20, 20});
}

void HelloDXR::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Hello DXR Settings", {300, 400}, {10, 80});

    w.checkbox("Ray Trace", mRayTrace);
    w.checkbox("Use Depth of Field", mUseDOF);
    w.checkbox("Use Coarse Pixel Shading RT", mUseCoarsePixelShading);
    hasShadowRenderedChanged =  w.slider("Shadow Render factor", shadowRenderFactor, 1.0f, 4.0f);

    Gui::DropdownList debugViewOptions = {
        {0, "Overlapped (Final)"},
        {1, "No Shadow RT"},
        {2, "Shadow RT"},
        {3, "Shadow RT Upsampled"}
    };

    w.dropdown("Debug View", debugViewOptions, currentDebugView);

    if (w.button("Load Scene"))
    {
        std::filesystem::path path;
        if (openFileDialog(Scene::getFileExtensionFilters(), path))
        {
            loadScene(path, getTargetFbo().get());
        }
    }

    mpScene->renderUI(w);
}

bool HelloDXR::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (keyEvent.key == Input::Key::Space && keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        mRayTrace = !mRayTrace;
        return true;
    }

    if (mpScene && mpScene->onKeyEvent(keyEvent))
        return true;

    return false;
}

bool HelloDXR::onMouseEvent(const MouseEvent& mouseEvent)
{
    return mpScene && mpScene->onMouseEvent(mouseEvent);
}

void HelloDXR::loadScene(const std::filesystem::path& path, const Fbo* pTargetFbo)
{
    mpScene = Scene::create(getDevice(), path);
    mpCamera = mpScene->getCamera();

    // Update the controllers
    float radius = mpScene->getSceneBounds().radius();
    mpScene->setCameraSpeed(radius * 0.25f);
    float nearZ = std::max(0.1f, radius / 750.0f);
    float farZ = radius * 10;
    mpCamera->setDepthRange(nearZ, farZ);
    mpCamera->setAspectRatio((float)pTargetFbo->getWidth() / (float)pTargetFbo->getHeight());

    // Get shader modules and type conformances for types used by the scene.
    // These need to be set on the program in order to use Falcor's material system.
    auto shaderModules = mpScene->getShaderModules();
    auto typeConformances = mpScene->getTypeConformances();

    // Get scene defines. These need to be set on any program using the scene.
    auto defines = mpScene->getSceneDefines();

    // Create raster pass.
    // This utility wraps the creation of the program and vars, and sets the necessary scene defines.
    ProgramDesc rasterProgDesc;
    rasterProgDesc.addShaderModules(shaderModules);
    rasterProgDesc.addShaderLibrary("Samples/HelloDXR/HelloDXR.3d.slang").vsEntry("vsMain").psEntry("psMain");
    rasterProgDesc.addTypeConformances(typeConformances);

    mpRasterPass = RasterPass::create(getDevice(), rasterProgDesc, defines);

    // We'll now create a raytracing program. To do that we need to setup two things:
    // - A program description (ProgramDesc). This holds all shader entry points, compiler flags, macro defintions,
    // etc.
    // - A binding table (RtBindingTable). This maps shaders to geometries in the scene, and sets the ray generation and
    // miss shaders.
    //
    // After setting up these, we can create the Program and associated RtProgramVars that holds the variable/resource
    // bindings. The Program can be reused for different scenes, but RtProgramVars needs to binding table which is
    // Scene-specific and needs to be re-created when switching scene. In this example, we re-create both the program
    // and vars when a scene is loaded.

    ProgramDesc rtProgDesc;
    rtProgDesc.addShaderModules(shaderModules);
    rtProgDesc.addShaderLibrary("Samples/HelloDXR/HelloDXR.rt.slang");
    rtProgDesc.addTypeConformances(typeConformances);
    rtProgDesc.setMaxTraceRecursionDepth(3); // 1 for calling TraceRay from RayGen, 1 for calling it from the
                                             // primary-ray ClosestHit shader for reflections, 1 for reflection ray
                                             // tracing a shadow ray
    rtProgDesc.setMaxPayloadSize(24);        // The largest ray payload struct (PrimaryRayData) is 24 bytes. The payload size
                                             // should be set as small as possible for maximum performance.

    ref<RtBindingTable> sbt = RtBindingTable::create(2, 2, mpScene->getGeometryCount());
    sbt->setRayGen(rtProgDesc.addRayGen("rayGen"));
    sbt->setMiss(0, rtProgDesc.addMiss("primaryMiss"));
    sbt->setMiss(1, rtProgDesc.addMiss("shadowMiss"));
    auto primary = rtProgDesc.addHitGroup("primaryClosestHit", "primaryAnyHit");
    auto shadow = rtProgDesc.addHitGroup("", "shadowAnyHit");
    sbt->setHitGroup(0, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), primary);
    sbt->setHitGroup(1, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), shadow);

    mpRaytraceProgram = Program::create(getDevice(), rtProgDesc, defines);
    mpRtVars = RtProgramVars::create(getDevice(), mpRaytraceProgram, sbt);

    // Modify version, no shadow ray
    ProgramDesc noShadowProgDesc;
    noShadowProgDesc.addShaderModules(shaderModules);
    noShadowProgDesc.addShaderLibrary("Samples/HelloDXR/HelloDXR_no_shadow.rt.slang");
    noShadowProgDesc.addTypeConformances(typeConformances);
    noShadowProgDesc.setMaxTraceRecursionDepth(3); // 1 for calling TraceRay from RayGen, 1 for calling it from the
                                                   // primary-ray ClosestHit shader for reflections, 1 for reflection ray
                                                   // tracing a shadow ray
    noShadowProgDesc.setMaxPayloadSize(24);        // The largest ray payload struct (PrimaryRayData) is 24 bytes. The payload size
                                                   // should be set as small as possible for maximum performance.

    ref<RtBindingTable> noShadowSbt = RtBindingTable::create(2, 2, mpScene->getGeometryCount());
    noShadowSbt->setRayGen(noShadowProgDesc.addRayGen("rayGen"));
    noShadowSbt->setMiss(0, noShadowProgDesc.addMiss("primaryMiss"));
    noShadowSbt->setMiss(1, noShadowProgDesc.addMiss("shadowMiss"));
    auto noShadowPrimary = noShadowProgDesc.addHitGroup("primaryClosestHit", "primaryAnyHit");
    auto noShadowShadow = noShadowProgDesc.addHitGroup("", "shadowAnyHit");
    noShadowSbt->setHitGroup(0, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), noShadowPrimary);
    noShadowSbt->setHitGroup(1, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), noShadowShadow);

    mpRaytraceNoShadowProgram = Program::create(getDevice(), noShadowProgDesc, defines);
    mpRtNoShadowVars = RtProgramVars::create(getDevice(), mpRaytraceNoShadowProgram, noShadowSbt);

    // Modify version, only calculate the shadow ray
    ProgramDesc shadowProgDesc;
    shadowProgDesc.addShaderModules(mpScene->getShaderModules());
    shadowProgDesc.addShaderLibrary("Samples/HelloDXR/HelloDXR_shadow.rt.slang");
    shadowProgDesc.addTypeConformances(typeConformances);
    shadowProgDesc.setMaxTraceRecursionDepth(3); // Only need to trace the shadow ray
    shadowProgDesc.setMaxPayloadSize(24);        // The largest ray payload struct (PrimaryRayData) is 24 bytes

    ref<RtBindingTable> shadowSbt = RtBindingTable::create(2, 2, mpScene->getGeometryCount());
    shadowSbt->setRayGen(shadowProgDesc.addRayGen("rayGen"));
    shadowSbt->setMiss(0, shadowProgDesc.addMiss("primaryMiss"));
    shadowSbt->setMiss(1, shadowProgDesc.addMiss("shadowMiss"));
    auto shadowPrimary = shadowProgDesc.addHitGroup("primaryClosestHit", "primaryAnyHit");
    auto shadowOnly = shadowProgDesc.addHitGroup("", "shadowAnyHit");
    shadowSbt->setHitGroup(0, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), shadowPrimary);
    shadowSbt->setHitGroup(1, mpScene->getGeometryIDs(Scene::GeometryType::TriangleMesh), shadowOnly);

    mpShadowRaytraceProgram = Program::create(getDevice(), shadowProgDesc, defines);
    mpShadowRtVars = RtProgramVars::create(getDevice(), mpShadowRaytraceProgram, shadowSbt);

    // Initialize upsample shader
    mpNNUpsamplePass = ComputePass::create(getDevice(), "Samples/HelloDXR/HelloDXR_upsample.rt.slang", "nearest_neighbour_upsample");
    mpBiLinearUpsamplePass = ComputePass::create(getDevice(), "Samples/HelloDXR/HelloDXR_upsample.rt.slang", "bi_linear_upsample");
    mpOverlapPass = FullScreenPass::create(getDevice(), "Samples/HelloDXR/HelloDXR_overlap.rt.slang");
}

void HelloDXR::setPerFrameVars(const Fbo* pTargetFbo)
{
    auto var = mpRtVars->getRootVar();
    var["PerFrameCB"]["invView"] = inverse(mpCamera->getViewMatrix());
    var["PerFrameCB"]["viewportDims"] = float2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
    float fovY = focalLengthToFovY(mpCamera->getFocalLength(), Camera::kDefaultFrameHeight);
    var["PerFrameCB"]["tanHalfFovY"] = std::tan(fovY * 0.5f);
    var["PerFrameCB"]["sampleIndex"] = mSampleIndex++;
    var["PerFrameCB"]["useDOF"] = mUseDOF;
    var["gOutput"] = mpRtOut;

    var = mpRtNoShadowVars->getRootVar();
    var["PerFrameCB"]["invView"] = inverse(mpCamera->getViewMatrix());
    var["PerFrameCB"]["viewportDims"] = float2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
    fovY = focalLengthToFovY(mpCamera->getFocalLength(), Camera::kDefaultFrameHeight);
    var["PerFrameCB"]["tanHalfFovY"] = std::tan(fovY * 0.5f);
    var["PerFrameCB"]["sampleIndex"] = mSampleIndex++;
    var["PerFrameCB"]["useDOF"] = mUseDOF;
    var["gOutput"] = mpRtNoShadowOut;

    var = mpShadowRtVars->getRootVar();
    var["PerFrameCB"]["invView"] = inverse(mpCamera->getViewMatrix());
    var["PerFrameCB"]["viewportDims"] = float2(pTargetFbo->getWidth() / shadowRenderFactor, pTargetFbo->getHeight() / shadowRenderFactor);
    fovY = focalLengthToFovY(mpCamera->getFocalLength(), Camera::kDefaultFrameHeight);
    var["PerFrameCB"]["tanHalfFovY"] = std::tan(fovY * 0.5f);
    var["PerFrameCB"]["sampleIndex"] = mSampleIndex++;
    var["PerFrameCB"]["useDOF"] = mUseDOF;
    var["gOutput"] = mpShadowRtOut;

    auto overlapVar = mpOverlapPass->getVars()->getRootVar();
    overlapVar["gNormalFrame"] = mpOverlapNormalIn;
    overlapVar["gShadowFrame"] = mpOverlapShadowIn;

    auto upsampleVar = mpNNUpsamplePass->getVars()->getRootVar();
    upsampleVar["gShadowFrame"] = mpShadowTexture;
    upsampleVar["gUpsampledShadowFrame"] = mpShadowUpsampleTexture;
    upsampleVar["shadowFactor"] = shadowRenderFactor;
}

void HelloDXR::renderRaster(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    FALCOR_ASSERT(mpScene);
    FALCOR_PROFILE(pRenderContext, "renderRaster");

    mpRasterPass->getState()->setFbo(pTargetFbo);
    mpScene->rasterize(pRenderContext, mpRasterPass->getState().get(), mpRasterPass->getVars().get());
}

void HelloDXR::renderRT(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    FALCOR_ASSERT(mpScene);
    FALCOR_PROFILE(pRenderContext, "renderRT");

    setPerFrameVars(pTargetFbo.get());

    pRenderContext->clearUAV(mpRtOut->getUAV().get(), kClearColor);
    mpScene->raytrace(pRenderContext, mpRaytraceProgram.get(), mpRtVars, uint3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1));
    pRenderContext->blit(mpRtOut->getSRV(), pTargetFbo->getRenderTargetView(0));
}

void HelloDXR::renderCoarsePixelShadingRT(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    FALCOR_ASSERT(mpScene);
    FALCOR_PROFILE(pRenderContext, "renderRT");

    if (hasShadowRenderedChanged)
    {
        onShadowRenderFactorChange();
    }

    setPerFrameVars(pTargetFbo.get());

    pRenderContext->clearUAV(mpRtNoShadowOut->getUAV().get(), kClearColor);
    pRenderContext->clearUAV(mpShadowRtOut->getUAV().get(), kClearColor);

    mpScene->raytrace(
        pRenderContext, mpShadowRaytraceProgram.get(), mpShadowRtVars, uint3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1)
    );
    mpScene->raytrace(
        pRenderContext, mpRaytraceNoShadowProgram.get(), mpRtNoShadowVars, uint3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1)
    );
    pRenderContext->uavBarrier(mpRtNoShadowOut.get()); // Ensure that the original ray tracing is finished before reading its result
    pRenderContext->uavBarrier(mpShadowRtOut.get());   // Ensure that the shadow ray tracing is finished before reading its result

    // TODO FIX WITH RWTexture2D?

    pRenderContext->blit(mpRtNoShadowOut->getSRV(), mpTempFbo1->getRenderTargetView(0));
    mpOverlapNormalIn = mpTempFbo1->getColorTexture(0);

    pRenderContext->blit(mpShadowRtOut->getSRV(), mpTempFbo2->getRenderTargetView(0));
    mpShadowTexture = mpTempFbo2->getColorTexture(0);

    // TODO CHANGE TO SWITCH BETWEEN NN AND BILINEAR
    if (true)
    {
        pRenderContext->clearUAV(mpShadowUpsampleTexture->getUAV().get(), kClearColor);
        upsampleShadow(pRenderContext);

        pRenderContext->blit(mpShadowUpsampleTexture->getSRV(), mpTempFbo3->getRenderTargetView(0));
        mpOverlapShadowIn = mpTempFbo3->getColorTexture(0);
    }
    else
    {
        pRenderContext->blit(mpShadowTexture->getSRV(), mpTempFbo3->getRenderTargetView(0));
        mpOverlapShadowIn = mpTempFbo3->getColorTexture(0);
    }

    // TODO TEMP CODE FOR CHECKING SHADOW RENDER
    //    pRenderContext->blit(mpShadowRtOut->getSRV(), mpTempFbo3->getRenderTargetView(0));
    //    mpOverlapShadowIn = mpTempFbo3->getColorTexture(0);

    mpOverlapPass->execute(pRenderContext, mpOverlapOut);
    auto outputTex = mpOverlapOut->getColorTexture(0);

    // TODO CHECK SHADOW SAMPLER

    if (currentDebugView == 0)
    {
        pRenderContext->blit(outputTex->getSRV(), pTargetFbo->getRenderTargetView(0));
    }
    else if (currentDebugView == 1)
    {
        pRenderContext->blit(mpRtNoShadowOut->getSRV(), pTargetFbo->getRenderTargetView(0));
    }
    else if (currentDebugView == 2)
    {
        pRenderContext->blit(mpShadowRtOut->getSRV(), pTargetFbo->getRenderTargetView(0));
    }
    else if (currentDebugView == 3)
    {
        pRenderContext->blit(mpShadowUpsampleTexture->getSRV(), pTargetFbo->getRenderTargetView(0));
    }
    else
        FALCOR_THROW("Invalid debug view selected");
}

void HelloDXR::upsampleShadow(RenderContext* pRenderContext)
{
    uint3 dispatchDims;
    dispatchDims.x = mpShadowUpsampleTexture->getWidth();
    dispatchDims.y = mpShadowUpsampleTexture->getHeight();
    dispatchDims.z = 1; // For 2D textures, the Z dimension should be 1
    mpNNUpsamplePass->execute(pRenderContext, dispatchDims);
}

void HelloDXR::onShadowRenderFactorChange()
{
    float width = getTargetFbo()->getWidth();
    float height = getTargetFbo()->getHeight();

    mpShadowRtOut = getDevice()->createTexture2D(
        (uint32_t)(width / shadowRenderFactor),
        (uint32_t)(height / shadowRenderFactor),
        ResourceFormat::RGBA16Float,
        1,
        1,
        nullptr,
        ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
    );

    mpShadowTexture = getDevice()->createTexture2D(
        (uint32_t)(width / shadowRenderFactor),
        (uint32_t)(height / shadowRenderFactor),
        ResourceFormat::RGBA16Float,
        1,
        1,
        nullptr,
        ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
    );

    mpTempFbo2 = Fbo::create2D(
        getDevice(), (uint32_t)(width / shadowRenderFactor), (uint32_t)(height / shadowRenderFactor), ResourceFormat::RGBA16Float
    );
}

int runMain(int argc, char** argv)
{
    SampleAppConfig config;
    config.windowDesc.title = "HelloDXR";
    config.windowDesc.resizableWindow = true;

    HelloDXR helloDXR(config);
    return helloDXR.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
