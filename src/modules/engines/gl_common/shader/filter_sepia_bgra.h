"#ifdef GL_ES\n"
"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
"precision highp float;\n"
"#else\n"
"precision mediump float;\n"
"#endif\n"
"#endif\n"
"uniform sampler2D tex;\n"
"varying vec4 col;\n"
"varying vec2 tex_c;\n"
"void main()\n"
"{\n"
"	vec3 inp = texture2D(tex,tex_c.xy).rgb;\n"
"	vec4 sep;\n"
"	sep.r = dot(inp, vec3(.393, .769, .189));\n"
"	sep.g = dot(inp, vec3(.349, .686, .168));\n"
"	sep.b = dot(inp, vec3(.272, .534, .131));\n"
"	sep.a = texture2D(tex,tex_c.xy).a;\n"
"	gl_FragColor = sep * col;\n"
"}\n"
