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

    template<typename T, size_t Size>
    class Vector {
    public:
        Vector() = default;
        Vector(std::array<T, Size> data) : m_data(data) { }

        T &operator[](size_t index) { return this->m_data[index]; }
        const T &operator[](size_t index) const { return this->m_data[index]; }

        T *data() { return this->m_data.data(); }
        const T *data() const { return this->m_data.data(); }

        [[nodiscard]] size_t size() const { return this->m_data.size(); }

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

        auto operator==(const Vector<T, Size>& other) {
            for (size_t i = 0; i < Size; i++)
                if (this->m_data[i] != other[i])
                    return false;
            return true;
        }

    private:
        std::array<T, Size> m_data;
    };


    class Shader {
    public:
        Shader() = default;
        Shader(std::string_view vertexSource, std::string_view fragmentSource);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader(Shader&& other) noexcept;

        Shader& operator=(const Shader&) = delete;
        Shader& operator=(Shader&& other) noexcept;

        void bind() const;
        void unbind() const;

        void setUniform(std::string_view name, const float &value);
        void setUniform(std::string_view name, const Vector<float, 3> &value);

    private:
        void compile(GLuint shader, std::string_view source);
        GLint getUniformLocation(std::string_view name);

    private:
        GLuint m_program = 0;
        std::map<std::string, GLint> m_uniforms;
    };

    enum class BufferType {
        Vertex = GL_ARRAY_BUFFER,
        Index = GL_ELEMENT_ARRAY_BUFFER
    };

    template<typename T>
    class Buffer {
    public:
        Buffer() = default;
        Buffer(BufferType type, std::span<T> data);
        ~Buffer();
        Buffer(const Buffer&) = delete;
        Buffer(Buffer&& other) noexcept;

        Buffer& operator=(const Buffer&) = delete;
        Buffer& operator=(Buffer&& other) noexcept;

        void bind() const;
        void unbind() const;

        void draw() const;

        size_t getSize() const;
    private:
        GLuint m_buffer = 0;
        size_t m_size = 0;
        GLuint m_type = 0;
    };

    extern template class Buffer<float>;
    extern template class Buffer<u32>;

    class VertexArray {
    public:
        VertexArray();
        ~VertexArray();
        VertexArray(const VertexArray&) = delete;
        VertexArray(VertexArray&& other) noexcept;

        VertexArray& operator=(const VertexArray&) = delete;
        VertexArray& operator=(VertexArray&& other) noexcept;

        template<typename T>
        void addBuffer(u32 index, const Buffer<T> &buffer) const {
            glEnableVertexAttribArray(index);
            buffer.bind();
            glVertexAttribPointer(index, 3, getType<T>(), GL_FALSE, 3 * sizeof(T), nullptr);
            buffer.unbind();
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
        Texture(const Texture&) = delete;
        Texture(Texture&& other) noexcept;

        Texture& operator=(const Texture&) = delete;
        Texture& operator=(Texture&& other) noexcept;

        void bind() const;
        void unbind() const;

        GLuint getTexture() const;
        u32 getWidth() const;
        u32 getHeight() const;

        GLuint release();
    private:
        GLuint m_texture;
        u32 m_width, m_height;
    };

    class FrameBuffer {
    public:
        FrameBuffer();
        ~FrameBuffer();
        FrameBuffer(const FrameBuffer&) = delete;
        FrameBuffer(FrameBuffer&& other) noexcept;

        FrameBuffer& operator=(const FrameBuffer&) = delete;
        FrameBuffer& operator=(FrameBuffer&& other) noexcept;

        void bind() const;
        void unbind() const;

        void attachTexture(const Texture &texture) const;

    private:
        GLuint m_frameBuffer, m_renderBuffer;
    };

}