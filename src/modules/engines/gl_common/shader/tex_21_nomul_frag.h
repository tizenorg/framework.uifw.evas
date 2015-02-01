"#ifdef GL_ES\n"
"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
"precision highp float;\n"
"#else\n"
"precision mediump float;\n"
"#endif\n"
"#endif\n"
"uniform sampler2D tex;\n"
"varying vec2 tex_c;\n"
"varying vec2 tex_s[2];\n"
"varying vec4 div_s;\n"
"void main()\n"
"{\n"
"   vec4 col00 = texture2D(tex, tex_c + tex_s[0]);\n"
"   vec4 col01 = texture2D(tex, tex_c + tex_s[1]);\n"
"   gl_FragColor = (col00 + col01) / div_s;\n"
"}\n"
