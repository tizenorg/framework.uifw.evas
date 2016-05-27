"#extension GL_OES_EGL_image_external : require\n"
"\n"
"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
"precision highp float;\n"
"#else\n"
"precision mediump float;\n"
"#endif\n"
"uniform samplerExternalOES tex;\n"
"varying vec2 tex_c;\n"
"void main()\n"
"{\n"
"   gl_FragColor = texture2D(tex, tex_c.xy);\n"
"}\n"