for %%s in (
        fullscreen_quad.vert
        output.frag
        mrt.vert
        mrt.frag
        deferred.vert
        deferred.frag
        depth_only.vert
        depth_only.frag
        convert_to_cubemap.comp
        cubemap_sh9_project.comp
        cubemap_sh9_downsample.comp
        cubemap_sh9_final.comp
        prefilter_reflections.comp
        generate_cube_mips.comp
        generate_brdf_lut.comp
        log_luminance.frag
        log_luminance_adapt.frag
        txaa.frag
        copy_rt.frag
        tonemap.frag
        edge_detection.frag
       ) do (
         glslc -c -MD %%s -Dfloat4=vec4 -Dmatrix=mat4 -flimit-file limits.conf
         del %%s.spv.d
       )