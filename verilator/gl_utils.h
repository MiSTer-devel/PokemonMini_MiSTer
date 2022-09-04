#include <cstdio>

namespace
{

    #define GL_ERROR_CASE(glerror)\
        case glerror: snprintf(error, sizeof(error), "%s", #glerror)

    void gl_debug(const char *file, int line) {
        GLenum err;
        while((err = glGetError()) != GL_NO_ERROR){
            char error[128];

            switch(err) {
                GL_ERROR_CASE(GL_INVALID_ENUM); break;
                GL_ERROR_CASE(GL_INVALID_VALUE); break;
                GL_ERROR_CASE(GL_INVALID_OPERATION); break;
                GL_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION); break;
                GL_ERROR_CASE(GL_OUT_OF_MEMORY); break;
                default: snprintf(error, sizeof(error), "%s", "UNKNOWN_ERROR"); break;
            }

            fprintf(stderr, "%s - %s: %d\n", error, file, line);
        }
    }

    #undef GL_ERROR_CASE

    #define __GL_DEBUG__ gl_debug(__FILE__, __LINE__);

    void validate_shader(GLuint shader, const char *file = 0){
        static const unsigned int BUFFER_SIZE = 512;
        char buffer[BUFFER_SIZE];
        GLsizei length = 0;

        glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

        if(length > 0)
        {
            printf("Shader %d(%s) compile error: %s\n", shader, (file? file: ""), buffer);
        }
    }

    bool validate_program(GLuint program){
        static const GLsizei BUFFER_SIZE = 512;
        GLchar buffer[BUFFER_SIZE];
        GLsizei length = 0;

        glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

        if(length > 0)
        {
            printf("Program %d link error: %s\n", program, buffer);
            return false;
        }

        return true;
    }

}
