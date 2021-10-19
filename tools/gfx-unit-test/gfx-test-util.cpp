#include "gfx-test-util.h"
#include "tools/unit-test/slang-unit-test.h"

#include <slang-com-ptr.h>

#define GFX_ENABLE_RENDERDOC_INTEGRATION 0

#if GFX_ENABLE_RENDERDOC_INTEGRATION
#    include "external/renderdoc_app.h"
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#endif

using Slang::ComPtr;

namespace gfx_test
{
    void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob)
    {
        if (diagnosticsBlob != nullptr)
        {
            getTestReporter()->message(TestMessageType::Info, (const char*)diagnosticsBlob->getBufferPointer());
        }
    }

    Slang::Result loadComputeProgram(
        gfx::IDevice* device,
        Slang::ComPtr<gfx::IShaderProgram>& outShaderProgram,
        const char* shaderModuleName,
        const char* entryPointName,
        slang::ProgramLayout*& slangReflection)
    {
        Slang::ComPtr<slang::ISession> slangSession;
        SLANG_RETURN_ON_FAIL(device->getSlangSession(slangSession.writeRef()));
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slang::IModule* module = slangSession->loadModule(shaderModuleName, diagnosticsBlob.writeRef());
        diagnoseIfNeeded(diagnosticsBlob);
        if (!module)
            return SLANG_FAIL;

        ComPtr<slang::IEntryPoint> computeEntryPoint;
        SLANG_RETURN_ON_FAIL(
            module->findEntryPointByName(entryPointName, computeEntryPoint.writeRef()));

        Slang::List<slang::IComponentType*> componentTypes;
        componentTypes.add(module);
        componentTypes.add(computeEntryPoint);

        Slang::ComPtr<slang::IComponentType> composedProgram;
        SlangResult result = slangSession->createCompositeComponentType(
            componentTypes.getBuffer(),
            componentTypes.getCount(),
            composedProgram.writeRef(),
            diagnosticsBlob.writeRef());
        diagnoseIfNeeded(diagnosticsBlob);
        SLANG_RETURN_ON_FAIL(result);
        slangReflection = composedProgram->getLayout();

        gfx::IShaderProgram::Desc programDesc = {};
        programDesc.pipelineType = gfx::PipelineType::Compute;
        programDesc.slangProgram = composedProgram.get();

        auto shaderProgram = device->createProgram(programDesc);

        outShaderProgram = shaderProgram;
        return SLANG_OK;
    }

    void compareComputeResult(gfx::IDevice* device, gfx::IBufferResource* buffer, uint8_t* expectedResult, size_t expectedBufferSize)
    {
        // Read back the results.
        ComPtr<ISlangBlob> resultBlob;
        GFX_CHECK_CALL_ABORT(device->readBufferResource(
            buffer, 0, expectedBufferSize, resultBlob.writeRef()));
        SLANG_CHECK(resultBlob->getBufferSize() == expectedBufferSize);
        // Compare results.
        SLANG_CHECK(memcmp(resultBlob->getBufferPointer(), expectedResult, expectedBufferSize) == 0);
    }

    Slang::ComPtr<gfx::IDevice> createTestingDevice(UnitTestContext* context, Slang::RenderApiFlag::Enum api)
    {
        Slang::ComPtr<gfx::IDevice> device;
        gfx::IDevice::Desc deviceDesc = {};
        switch (api)
        {
        case Slang::RenderApiFlag::D3D11:
            deviceDesc.deviceType = gfx::DeviceType::DirectX11;
            break;
        case Slang::RenderApiFlag::D3D12:
            deviceDesc.deviceType = gfx::DeviceType::DirectX12;
            break;
        case Slang::RenderApiFlag::Vulkan:
            deviceDesc.deviceType = gfx::DeviceType::Vulkan;
            break;
        case Slang::RenderApiFlag::CPU:
            deviceDesc.deviceType = gfx::DeviceType::CPU;
            break;
        case Slang::RenderApiFlag::CUDA:
            deviceDesc.deviceType = gfx::DeviceType::CUDA;
            break;
        case Slang::RenderApiFlag::OpenGl:
            deviceDesc.deviceType = gfx::DeviceType::OpenGl;
            break;
        default:
            SLANG_IGNORE_TEST
        }
        deviceDesc.slang.slangGlobalSession = context->slangGlobalSession;
        const char* searchPaths[] = { "", "../../tools/gfx-unit-test", "tools/gfx-unit-test" };
        deviceDesc.slang.searchPathCount = (SlangInt)SLANG_COUNT_OF(searchPaths);
        deviceDesc.slang.searchPaths = searchPaths;
        auto createDeviceResult = gfxCreateDevice(&deviceDesc, device.writeRef());
        if (SLANG_FAILED(createDeviceResult))
        {
            SLANG_IGNORE_TEST
        }
        return device;
    }

#if GFX_ENABLE_RENDERDOC_INTEGRATION
    RENDERDOC_API_1_1_2* rdoc_api = NULL;
    void initializeRenderDoc()
    {
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI =
                (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
            assert(ret == 1);
        }
    }
    void renderDocBeginFrame()
    {
        if (!rdoc_api) initializeRenderDoc();
        if (rdoc_api) rdoc_api->StartFrameCapture(nullptr, nullptr);
    }
    void renderDocEndFrame()
    {
        if (rdoc_api)
            rdoc_api->EndFrameCapture(nullptr, nullptr);
        _fgetchar();
    }
#else
    void initializeRenderDoc() {}
    void renderDocBeginFrame() {}
    void renderDocEndFrame() {}
#endif
}