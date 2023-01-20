#pragma once

#include <hex.hpp>

#include <hex/helpers/concepts.hpp>

#include <cmath>
#include <map>
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

        void setUniform(const std::string &name, const float &value);

    private:
        void compile(GLuint shader, const std::string &source);

    private:
        GLuint m_program;
        std::map<std::string, GLint> m_uniforms;
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
        void addBuffer(u32 index, const Buffer<T> &buffer) const {
            glEnableVertexAttribArray(index);
            buffer.bind();
            glVertexAttribPointer(index, 3, getType<T>(), GL_FALSE, 3 * sizeof(T), nullptr);
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

    template<typename T, size_t Size>
    class Vector {
    public:
        Vector() = default;
        Vector(std::array<T, Size> data) : m_data(data) { }

        T &operator[](size_t index) { return this->m_data[index]; }
        const T &operator[](size_t index) const { return this->m_data[index]; }

        T *data() { return this->m_data.data(); }
        const T *data() const { return this->m_data.data(); }

        size_t size() const { return this->m_data.size(); }

        auto operator+(const Vector<T, Size>& other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] += other[i];
            return copy;
        }

        auto operator-(const Vector<T, Size>& other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] -= other[i];
            return copy;
        }

        auto dot(const Vector<T, Size>& other) {
            T result = 0;
            for (size_t i = 0; i < Size; i++)
                result += this->m_data[i] * other[i];
            return result;
        }

        auto cross(const Vector<T, Size>& other) {
            static_assert(Size == 3, "Cross product is only defined for 3D vectors");
            return Vector<T, Size>({ this->m_data[1] * other[2] - this->m_data[2] * other[1], this->m_data[2] * other[0] - this->m_data[0] * other[2], this->m_data[0] * other[1] - this->m_data[1] * other[0] });
        }

        auto normalize() {
            auto copy = *this;
            auto length = std::sqrt(copy.dot(copy));
            for (size_t i = 0; i < Size; i++)
                copy[i] /= length;
            return copy;
        }

    private:
        std::array<T, Size> m_data;
    };


}