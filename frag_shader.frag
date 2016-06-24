#version 420
#extension GL_ARB_texture_rectangle : enable
#extension GL_EXT_gpu_shader4 : enable
#extension GL_ARB_shading_language_420pack: enable

uniform sampler2DRect srcTex;
uniform sampler2DRect hotspots;
uniform usampler2DRect warp;

uniform int do_warping;

in vec4 gl_FragCoord;
layout ( location = 0 ) out vec4 gl_FragColor;
in vec4 myCoord;

void main(void)
{
    vec2 st = myCoord.st;

    if (do_warping != 0) {
        st = gl_FragCoord.xy;
        const ivec2 size = textureSize(srcTex);
        const int w = size.x, h = size.y;

        ivec2 sti = ivec2(int(st.x), int(st.y));
        uvec4 c = texelFetch(warp, sti);
        //uvec4 c = texture(warp,st);

        uint x = c.r*uint(256) + c.g;
        uint y = c.b*uint(256) + c.a;

        double xx = double(x)/65535.0;
        double yy = double(y)/65535.0;
        xx *= double(w);  yy *= double(h);

        st = vec2(float(xx), float(yy));
        sti = ivec2(float(st.x),float(st.y));

        //vec4 hspot = texelFetch(hotspots, sti);
        //gl_FragColor = texelFetch(srcTex, sti)*hspot;
    }

    vec4 hspot = texture(hotspots, st);
    gl_FragColor = texture(srcTex, st)*hspot;
}
