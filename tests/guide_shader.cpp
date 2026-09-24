#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <iostream>
#include <string>
#include <vector>
#include "../src/GuideShaders.hpp"

template <class T> T glProc(const char *name) {
    return reinterpret_cast<T>(wglGetProcAddress(name));
}
int main() {
    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "LayoutShaderTest";
    if (!RegisterClassA(&wc))
        return 77;
    auto window = CreateWindowA(wc.lpszClassName, "", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr,
                                wc.hInstance, nullptr);
    if (!window)
        return 77;
    auto dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    int pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (!pixelFormat || !SetPixelFormat(dc, pixelFormat, &pfd))
        return 77;
    auto context = wglCreateContext(dc);
    if (!context || !wglMakeCurrent(dc, context))
        return 77;
    auto createShader = glProc<GLuint(APIENTRY *)(GLenum)>("glCreateShader");
    auto shaderSource =
        glProc<void(APIENTRY *)(GLuint, GLsizei, const char *const *, const GLint *)>(
            "glShaderSource");
    auto compileShader = glProc<void(APIENTRY *)(GLuint)>("glCompileShader");
    auto getShader = glProc<void(APIENTRY *)(GLuint, GLenum, GLint *)>("glGetShaderiv");
    auto shaderLog =
        glProc<void(APIENTRY *)(GLuint, GLsizei, GLsizei *, char *)>("glGetShaderInfoLog");
    auto createProgram = glProc<GLuint(APIENTRY *)()>("glCreateProgram");
    auto attachShader = glProc<void(APIENTRY *)(GLuint, GLuint)>("glAttachShader");
    auto bindAttribute =
        glProc<void(APIENTRY *)(GLuint, GLuint, const char *)>("glBindAttribLocation");
    auto linkProgram = glProc<void(APIENTRY *)(GLuint)>("glLinkProgram");
    auto getProgram = glProc<void(APIENTRY *)(GLuint, GLenum, GLint *)>("glGetProgramiv");
    auto programLog =
        glProc<void(APIENTRY *)(GLuint, GLsizei, GLsizei *, char *)>("glGetProgramInfoLog");
    auto useProgram = glProc<void(APIENTRY *)(GLuint)>("glUseProgram");
    auto uniformLocation = glProc<GLint(APIENTRY *)(GLuint, const char *)>("glGetUniformLocation");
    auto uniform1i = glProc<void(APIENTRY *)(GLint, GLint)>("glUniform1i");
    auto uniform2f = glProc<void(APIENTRY *)(GLint, GLfloat, GLfloat)>("glUniform2f");
    auto uniform4f =
        glProc<void(APIENTRY *)(GLint, GLfloat, GLfloat, GLfloat, GLfloat)>("glUniform4f");
    auto uniformMatrix =
        glProc<void(APIENTRY *)(GLint, GLsizei, GLboolean, const GLfloat *)>("glUniformMatrix4fv");
    auto enableAttribute = glProc<void(APIENTRY *)(GLuint)>("glEnableVertexAttribArray");
    auto attributePointer =
        glProc<void(APIENTRY *)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)>(
            "glVertexAttribPointer");
    if (!createShader || !shaderSource || !compileShader || !getShader || !shaderLog ||
        !createProgram || !attachShader || !bindAttribute || !linkProgram || !getProgram ||
        !programLog || !useProgram || !uniformLocation || !uniform1i || !uniform2f || !uniform4f ||
        !uniformMatrix || !enableAttribute || !attributePointer)
        return 77;
    auto compile = [&](GLenum type, std::string const &text) {
        auto shader = createShader(type);
        const char *ptr = text.c_str();
        shaderSource(shader, 1, &ptr, nullptr);
        compileShader(shader);
        GLint ok = 0;
        getShader(shader, 0x8B81, &ok);
        if (!ok) {
            char log[4096]{};
            shaderLog(shader, sizeof(log), nullptr, log);
            std::cerr << log;
            return GLuint(0);
        }
        return shader;
    };
    auto vertex =
        compile(0x8B31, std::string("uniform mat4 CC_MVPMatrix;\n") + layout::shaders::vertex);
    auto fragment = compile(0x8B30, layout::shaders::fragment);
    if (!vertex || !fragment)
        return 1;
    auto program = createProgram();
    attachShader(program, vertex);
    attachShader(program, fragment);
    bindAttribute(program, 0, "a_position");
    bindAttribute(program, 1, "a_texCoord");
    linkProgram(program);
    GLint ok = 0;
    getProgram(program, 0x8B82, &ok);
    if (!ok) {
        char log[4096]{};
        programLog(program, sizeof(log), nullptr, log);
        std::cerr << log;
        return 1;
    }
    useProgram(program);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    std::vector<unsigned char> checker(64 * 64 * 4);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            int i = (y * 64 + x) * 4;
            checker[i] = checker[i + 1] = checker[i + 2] = (x + y) % 2 ? 255 : 0;
            checker[i + 3] = 255;
        }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker.data());
    uniform1i(uniformLocation(program, "CC_Texture0"), 0);
    uniform2f(uniformLocation(program, "u_pixel"), 1.f / 64, 1.f / 64);
    uniform4f(uniformLocation(program, "u_hole"), 24, 24, 40, 40);
    const GLfloat matrix[] = {2.f / 64, 0, 0, 0, 0, 2.f / 64, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
    uniformMatrix(uniformLocation(program, "CC_MVPMatrix"), 1, GL_FALSE, matrix);
    const GLfloat positions[] = {0, 0, 64, 0, 0, 64, 64, 64}, uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    enableAttribute(0);
    enableAttribute(1);
    attributePointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);
    attributePointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    unsigned char center[4]{}, outside[4]{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center);
    glReadPixels(12, 12, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\nSpotlight RGB: " << int(center[0])
              << "," << int(center[1]) << "," << int(center[2]) << "\nBlur RGB: " << int(outside[0])
              << "," << int(outside[1]) << "," << int(outside[2]) << "\n";
    bool pass = center[0] < 5 && center[1] > 250 && center[2] < 5 && outside[0] > 45 &&
                outside[0] < 80 && outside[0] == outside[1] && outside[1] == outside[2] &&
                glGetError() == GL_NO_ERROR;
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, dc);
    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return pass ? 0 : 1;
}
