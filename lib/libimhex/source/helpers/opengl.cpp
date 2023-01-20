#include <hex/helpers/opengl.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::gl {

    Shader::Shader(const std::string &vertexSource, const std::string &fragmentSource) {
        auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
        this->compile(vertexShader, vertexSource);

        auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        this->compile(fragmentShader, fragmentSource);

        ON_SCOPE_EXIT { glDeleteShader(vertexShader); glDeleteShader(fragmentShader); };

        this->m_program = glCreateProgram();

        glAttachShader(this->m_program, vertexShader);
        glAttachShader(this->m_program, fragmentShader);
        glLinkProgram(this->m_program);

        int result = false;
        glGetProgramiv(this->m_program, GL_LINK_STATUS, &result);
        if (!result) {
            std::vector<char> log(512);
            glGetShaderInfoLog(this->m_program, log.size(), nullptr, log.data());
            log::error("Failed to link shader: {}", log.data());
        }
    }

    Shader::~Shader() {
        glDeleteProgram(this->m_program);
    }

    void Shader::bind() const {
        glUseProgram(this->m_program);
    }

    void Shader::unbind() const {
        glUseProgram(0);
    }

    void Shader::compile(GLuint shader, const std::string &source) {
        auto sourcePtr = source.c_str();

        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        int result = false;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
        if (!result) {
            std::vector<char> log(512);
            glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
            log::error("Failed to compile shader: {}", log.data());
        }
    }


    template<typename T>
    Buffer<T>::Buffer(BufferType type, std::span<T> data) : m_size(data.size()), m_type(GLuint(type)) {
        glGenBuffers(1, &this->m_buffer);
        glBindBuffer(this->m_type, this->m_buffer);
        glBufferData(this->m_type, data.size_bytes(), data.data(), GL_STATIC_DRAW);
    }

    template<typename T>
    Buffer<T>::~Buffer() {
        glDeleteBuffers(1, &this->m_buffer);
    }

    template<typename T>
    void Buffer<T>::bind() const {
        glBindBuffer(this->m_type, this->m_buffer);
    }

    template<typename T>
    void Buffer<T>::unbind() const {
        glBindBuffer(this->m_type, 0);
    }

    template<typename T>
    size_t Buffer<T>::getSize() const {
        return this->m_size;
    }

    template<typename T>
    void Buffer<T>::draw() const {
        glDrawElements(GL_TRIANGLES, this->m_size, getType<T>(), nullptr);
    }

    template class Buffer<float>;
    template class Buffer<u32>;


    VertexArray::VertexArray() {
        glGenVertexArrays(1, &this->m_array);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &this->m_array);
    }

    void VertexArray::bind() const {
        glBindVertexArray(this->m_array);
    }

    void VertexArray::unbind() const {
        glBindVertexArray(0);
    }


    Texture::Texture(u32 width, u32 height) : m_texture(0), m_width(width), m_height(height) {
        glGenTextures(1, &this->m_texture);
        glBindTexture(GL_TEXTURE_2D, this->m_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture::~Texture() {
        if (this->m_texture != 0)
            glDeleteTextures(1, &this->m_texture);
    }

    void Texture::bind() const {
        glBindTexture(GL_TEXTURE_2D, this->m_texture);
    }

    void Texture::unbind() const {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint Texture::getTexture() const {
        return this->m_texture;
    }

    u32 Texture::getWidth() const {
        return this->m_width;
    }

    u32 Texture::getHeight() const {
        return this->m_height;
    }

    void Texture::release() {
        this->m_texture = 0;
    }


    FrameBuffer::FrameBuffer() {
        glGenFramebuffers(1, &this->m_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, this->m_frameBuffer);

        glGenRenderbuffers(1, &this->m_renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, this->m_renderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 720);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->m_renderBuffer);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    FrameBuffer::~FrameBuffer() {
        glDeleteFramebuffers(1, &this->m_frameBuffer);
        glDeleteRenderbuffers(1, &this->m_renderBuffer);
    }

    void FrameBuffer::bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, this->m_frameBuffer);
    }

    void FrameBuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::attachTexture(const Texture &texture) const {
        glBindFramebuffer(GL_FRAMEBUFFER, this->m_frameBuffer);
        texture.bind();

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getTexture(), 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

}