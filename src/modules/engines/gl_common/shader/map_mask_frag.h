"#ifdef GL_ES\n"
"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
"precision highp float;\n"
"#else\n"
"precision mediump float;\n"
"#endif\n"
"#endif\n"
"uniform sampler2D tex, texm;\n"
"varying vec2 tex_c, tex_m;\n"
"varying vec4 col;\n"
"void main()\n"
"{\n"
"   // FIXME: Fix Mach band effect using proper 4-point color interpolation\n"
"   gl_FragColor = texture2D(tex, tex_c).bgra * texture2D(texm, tex_m).a *  col;\n"
"}\n"