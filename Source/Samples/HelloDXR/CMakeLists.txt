add_falcor_executable(HelloDXR)

target_sources(HelloDXR PRIVATE
    HelloDXR.cpp
    HelloDXR.h
    HelloDXR.3d.slang
    HelloDXR.rt.slang
    HelloDXR_shadow.rt.slang
    HelloDXR_overlap.rt.slang
    HelloDXR_no_shadow.rt.slang
    HelloDXR_upsample.rt.slang
    HelloDXR_bitmask_view.rt.slang
    HelloDXR_convert.rt.slang
)

target_copy_shaders(HelloDXR Samples/HelloDXR)

target_source_group(HelloDXR "Samples")
