"#ifdef GL_ES\n"
"precision highp float;\n"
"#endif\n"
"attribute vec4 vertex;\n"
"attribute vec4 color;\n"
"attribute vec2 tex_coord;\n"
"attribute vec2 tex_coordm;\n"
"uniform mat4 mvp;\n"
"varying vec4 col;\n"
"varying vec2 tex_c;\n"
"varying vec2 tex_m;\n"
"void main()\n"
"{\n"
"   gl_Position = mvp * vertex;\n"
"   col = color;\n"
"   tex_c = tex_coord;\n"
"   tex_m = tex_coordm;\n"
"}\n"
"\n"
