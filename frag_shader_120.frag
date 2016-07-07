#version 120
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect srcTex;
uniform sampler2DRect hotspots;
uniform sampler2DRect warp;

uniform int do_warping;
uniform vec2 size;

void main(void)
{
    vec2 st = gl_TexCoord[0].st;

    if (do_warping != 0) {
        float w = size.x;
        float h = size.y;

        vec4 c = texture2DRect(warp,st);

        float x = c.r*float(256)*float(255) + c.g*float(255);
        float y = c.b*float(256)*float(255) + c.a*float(255);

        float xx = x/65535.0;
        float yy = y/65535.0;
        xx *= w;  yy *= h;

        st = vec2(xx, yy);
    }

    vec4 hspot = texture2DRect(hotspots, st);
    gl_FragColor = texture2DRect(srcTex, st)*hspot;
}
