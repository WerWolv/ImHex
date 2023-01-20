#pragma once

#include <hex.hpp>

#include <hex/helpers/concepts.hpp>

#include <span>
#include <string>

#include <imgui_impl_opengl3_loader.h>

namespace hex::gl {

    namespace {

        template<typename T>
        GLuint getType() {
            if constexpr (std::is_same_v<T, float>)
                return GL_FLOAT;
            else if constexpr (std::is_same_v<T, u32>)
                return GL_UNSIGNED_INT;
            else
                static_assert(hex::always_false<T>::value, "Unsupported type");
        }

    }

    class Shader {
    public:
        Shader(const std::string &vertexSource, const std::string &fragmentSource);
        ~Shader();

        void bind() const;
        void unbind() const;

    private:
        void compile(GLuint shader, const std::string &source);

    private:
        GLuint m_program;
    };

    enum class BufferType {
        Vertex = GL_ARRAY_BUFFER,
        Index = GL_ELEMENT_ARRAY_BUFFER
    };

    template<typename T>
    class Buffer {
    public:
        Buffer(BufferType type, std::span<T> data);
        ~Buffer();

        void bind() const;
        void unbind() const;

        void draw() const;

        size_t getSize() const;
    private:
        GLuint m_buffer;
        size_t m_size;
        GLuint m_type;
    };

    extern template class Buffer<float>;
    extern template class Buffer<u32>;

    class VertexArray {
    public:
        VertexArray();
        ~VertexArray();

        template<typename T>
        void addBuffer(const Buffer<T> &buffer) const {
            glVertexAttribPointer(0, buffer.getSize() / sizeof(T), getType<T>(), GL_FALSE, 3 * sizeof(T), nullptr);
            glEnableVertexAttribArray(0);
        }

        void bind() const;
        void unbind() const;

    private:
        GLuint m_array;
    };

    class Texture {
    public:
        Texture(u32 width, u32 height);
        ~Texture();

        void bind() const;
        void unbind() const;

        GLuint getTexture() const;
        u32 getWidth() const;
        u32 getHeight() const;

        void release();
    private:
        GLuint m_texture;
        u32 m_width, m_height;
    };

    class FrameBuffer {
    public:
        FrameBuffer();
        ~FrameBuffer();

        void bind() const;
        void unbind() const;

        void attachTexture(const Texture &texture) const;

    private:
        GLuint m_frameBuffer, m_renderBuffer;
    };


}