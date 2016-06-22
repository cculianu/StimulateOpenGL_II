#version 130
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect srcTex;
uniform sampler2DRect hotspots;

void main(void)
{
    vec4 hspot = texture2DRect(hotspots, gl_TexCoord[0].st);
    gl_FragColor = texture2DRect(srcTex, gl_TexCoord[0].st)*hspot;
}
