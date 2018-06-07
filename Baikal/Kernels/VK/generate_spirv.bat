glslc -c -MD fullscreen_quad.vert -Dfloat4=vec4 -Dmatrix=mat4
glslc -c -MD output.frag -Dfloat4=vec4 -Dmatrix=mat4
glslc -c -MD mrt.vert -Dfloat4=vec4 -Dmatrix=mat4
glslc -c -MD mrt.frag -Dfloat4=vec4 -Dmatrix=mat4
glslc -c -MD deferred.frag -Dfloat4=vec4 -Dmatrix=mat4