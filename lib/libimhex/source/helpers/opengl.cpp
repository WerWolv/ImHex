#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/utils/guards.hpp>


namespace hex::gl {

    Matrix<float,4,4> GetOrthographicMatrix( float viewWidth, float viewHeight, float nearVal,float farVal, bool actionType)
    {
        int sign =1;
        if (actionType)
            sign=-1;
        //float left = leftRight.x;
        //float right = leftRight.y;
        //float down = upDown.x;
        //float up = upDown.y;
        Matrix<float,4,4> result(0);
        //result.updateElement(0,0,sign /(right-left))
        result.updateElement(0,0,sign / viewWidth);
        //result.updateElement(0,3,sign * (right + left)/(right - left));
        //result.updateElement(1,1, sign /(up-down));
        result.updateElement(1,1, sign / viewHeight);
        //result.updateElement(1,3, sign * (up + down)/(up - down));

        result.updateElement(2,2,-sign * 2/(farVal - nearVal));
        result.updateElement(3,2,-sign * (farVal + nearVal)/(farVal - nearVal));

        result.updateElement(3,3,sign);
        return result;
    }


    Matrix<float,4,4> GetPerspectiveMatrix(	float viewWidth, float viewHeight, float nearVal,float farVal, bool actionType)
    {
        int sign =1;
        if (actionType)
            sign=-1;
        //float left = leftRight.x;
        //float right = leftRight.y;
        //float down = upDown.x;
        //float up = upDown.y;
        //T aspect=(right-left)/(top-bottom);
        //T f = nearVal/top;
        Matrix<float,4,4> result(0);
        //   T f = 1.0 / tan(fovy / 2.0); tan(fovy / 2.0) = top / near; fovy = 2 * atan2(top,near)

        //result.updateElement(0,0,sign * nearVal/(right-left));
        //result.updateElement(1,1, sign * nearVal/(up-down));
        result.updateElement(0,0,sign * nearVal/viewWidth);
        result.updateElement(1,1, sign * nearVal/viewHeight);
        result.updateElement(2,2,-sign * (farVal + nearVal)/(farVal - nearVal));
        result.updateElement(3,2,-sign * 2*farVal*nearVal/(farVal - nearVal));
        result.updateElement(2,3,-sign);

        return result;
    }

    Shader::Shader(std::string_view vertexSource, std::string_view fragmentSource) {
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
        if (this->m_program != 0)
            glDeleteProgram(this->m_program);
    }

    Shader::Shader(Shader &&other) noexcept {
        this->m_program = other.m_program;
        other.m_program = 0;
    }

    Shader& Shader::operator=(Shader &&other) noexcept {
        this->m_program = other.m_program;
        other.m_program = 0;
        return *this;
    }

    void Shader::bind() const {
        glUseProgram(this->m_program);
    }

    void Shader::unbind() const {
        glUseProgram(0);
    }

    void Shader::setUniform(std::string_view name, const int &value) {
        glUniform1i(getUniformLocation(name), value);
    }

    void Shader::setUniform(std::string_view name, const float &value) {
        glUniform1f(getUniformLocation(name), value);
    }


    GLint Shader::getUniformLocation(std::string_view name) {
        auto uniform = this->m_uniforms.find(name.data());
        if (uniform == this->m_uniforms.end()) {
            auto location = glGetUniformLocation(this->m_program, name.data());
            if (location == -1) {
                log::warn("Uniform '{}' not found in shader", name);
                return -1;
            }

            this->m_uniforms[name.data()] = location;
            uniform = this->m_uniforms.find(name.data());
        }

        return uniform->second;
    }

    void Shader::compile(GLuint shader, std::string_view source) {
        auto sourcePtr = source.data();

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
        glBindBuffer(this->m_type, 0);
    }

    template<typename T>
    Buffer<T>::~Buffer() {
        glDeleteBuffers(1, &this->m_buffer);
    }

    template<typename T>
    Buffer<T>::Buffer(Buffer &&other) noexcept {
        this->m_buffer = other.m_buffer;
        this->m_size = other.m_size;
        this->m_type = other.m_type;
        other.m_buffer = -1;
    }

    template<typename T>
    Buffer<T>& Buffer<T>::operator=(Buffer &&other) noexcept {
        this->m_buffer = other.m_buffer;
        this->m_size = other.m_size;
        this->m_type = other.m_type;
        other.m_buffer = -1;
        return *this;
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
    void Buffer<T>::draw(unsigned primitive) const {
        switch (this->m_type) {
            case GL_ARRAY_BUFFER:
                glDrawArrays(primitive, 0, this->m_size);
                break;
            case GL_ELEMENT_ARRAY_BUFFER:
                glDrawElements(primitive, this->m_size, impl::getType<T>(), nullptr);
                 break;
        }
    }

    template<typename T>
    void Buffer<T>::update(std::span<T> data) {
        glBindBuffer(this->m_type, this->m_buffer);
        glBufferSubData(this->m_type, 0, data.size_bytes(), data.data());
        glBindBuffer(this->m_type, 0);
    }

    template class Buffer<float>;
    template class Buffer<u32>;
    template class Buffer<u16>;
    template class Buffer<u8>;

    VertexArray::VertexArray() {
        glGenVertexArrays(1, &this->m_array);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &this->m_array);
    }

    VertexArray::VertexArray(VertexArray &&other) noexcept {
        this->m_array = other.m_array;
        other.m_array = -1;
    }

    VertexArray& VertexArray::operator=(VertexArray &&other) noexcept {
        this->m_array = other.m_array;
        other.m_array = -1;
        return *this;
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

    Texture::Texture(Texture &&other) noexcept {
        this->m_texture = other.m_texture;
        other.m_texture = -1;

        this->m_width = other.m_width;
        this->m_height = other.m_height;
    }

    Texture& Texture::operator=(Texture &&other) noexcept {
        this->m_texture = other.m_texture;
        other.m_texture = -1;
        return *this;
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

    GLuint Texture::release() {
        auto copy = this->m_texture;
        this->m_texture = -1;

        return copy;
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

    FrameBuffer::FrameBuffer(FrameBuffer &&other) noexcept {
        this->m_frameBuffer = other.m_frameBuffer;
        other.m_frameBuffer = -1;
        this->m_renderBuffer = other.m_renderBuffer;
        other.m_renderBuffer = -1;
    }

    FrameBuffer& FrameBuffer::operator=(FrameBuffer &&other) noexcept {
        this->m_frameBuffer = other.m_frameBuffer;
        other.m_frameBuffer = -1;
        this->m_renderBuffer = other.m_renderBuffer;
        other.m_renderBuffer = -1;
        return *this;
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

    AxesVectors::AxesVectors() {
        vertices.resize(36);
        colors.resize(48);
        indices.resize(18);

        // Vertices are x,y,z. Colors are RGBA. Indices are for the ends of each segment
        // Entries with value zero are unneeded but kept to help keep track of location
        // x-axis
        //vertices[0]=0.0F;  vertices[1]= 0.0F   vertices[2] = 0.0F;  // shaft base
        vertices[3] = 1.0F;//vertices[4]= 0.0F   vertices[5] = 0.0F;  // shaft tip
        vertices[6] = 0.9F;                      vertices[8] = 0.05F; // arrow base
        vertices[9] = 0.9F;                      vertices[11]=-0.05F; // arrow base
        // y-axis
        //vertices[12]=0.0F;  vertices[13] = 0.0F;  vertices[14]=0.0F;// shaft base
                              vertices[16] = 1.0F;//vertices[17]=0.0F;// shaft tip
        vertices[18] = 0.05F; vertices[19] = 0.9F;//vertices[20]=0.0F;// arrow base
        vertices[21] =-0.05F; vertices[22] = 0.9F;//vertices[23]=0.0F;// arrow base
        // z-axis
        //vertices[24]=0.0F;   vertices[25]=0.0F  vertices[26] = 0.0F; // shaft base
                                                  vertices[29] = 1.0F; // shaft tip
        vertices[30] = 0.05F;                     vertices[32] = 0.9F; // arrow base
        vertices[33] =-0.05F;                     vertices[35] = 0.9F; // arrow base
        // x-axis colors
        colors[0]  = 0.7F;colors[3]  = 1.0F;
        colors[4]  = 0.7F;colors[7]  = 1.0F;
        colors[8]  = 0.7F;colors[11] = 1.0F;
        colors[12] = 0.7F;colors[15] = 1.0F;
        // y-axis colors
        colors[17] = 0.7F;colors[19] = 1.0F;
        colors[21] = 0.7F;colors[23] = 1.0F;
        colors[25] = 0.7F;colors[27] = 1.0F;
        colors[29] = 0.7F;colors[31] = 1.0F;
        // z-axis colors
        colors[34] = 0.7F;colors[35] = 1.0F;
        colors[38] = 0.7F;colors[39] = 1.0F;
        colors[42] = 0.7F;colors[43] = 1.0F;
        colors[46] = 0.7F;colors[47] = 1.0F;
        // indices  for x
                        indices[1]  = 1;
        indices[2] = 2; indices[3]  = 1;
        indices[4] = 3; indices[5]  = 1;
        // indices for y
        indices[6] =  4; indices[7]  = 5;
        indices[8] =  6; indices[9]  = 5;
        indices[10] = 7; indices[11] = 5;
        // indices for z
        indices[12] =  8; indices[13] = 9;
        indices[14] = 10; indices[15] = 9;
        indices[16] = 11; indices[17] = 9;
    }

    AxesBuffers::AxesBuffers( VertexArray& axesVertexArray, AxesVectors axesVectors) {
        vertices = {};
        colors = {};
        indices = {};

        axesVertexArray.bind();

        vertices = gl::Buffer<float>(gl::BufferType::Vertex, axesVectors.getVertices());
        colors = gl::Buffer<float>(gl::BufferType::Vertex, axesVectors.getColors());
        indices = gl::Buffer<u8>(gl::BufferType::Index, axesVectors.getIndices());

        axesVertexArray.addBuffer(0, vertices);
        axesVertexArray.addBuffer(1, colors, 4);

        vertices.unbind();
        colors.unbind();
        indices.unbind();
        axesVertexArray.unbind();
    }


    GridVectors::GridVectors(int sliceCount) {
        slices = sliceCount;
        vertices.resize((slices + 1) * (slices + 1) * 3);
        colors.resize((slices + 1) * (slices + 1) * 4);
        indices.resize(slices  * slices  * 6 + slices * 2);
        int k = 0;
        int l = 0;
        for (int j = 0; j <= slices; ++j) {
            float z = 2.0f * (float) j / (float) slices - 1.0f;
            for (int i = 0; i <= slices; ++i) {
                vertices[k   ] = 2.0f * (float) i / (float) slices - 1.0f;
                vertices[k + 1] = 0.0f;
                vertices[k + 2] = z;
                k += 3;
                colors[l    ] = 0.5f;
                colors[l + 1] = 0.5f;
                colors[l + 2] = 0.5f;
                colors[l + 3] = 0.3f;
                l += 4;
            }
        }
        k = 0;
        for (int j = 0; j < slices; ++j) {
            int row1 = j * (slices + 1);
            int row2 = (j + 1) * (slices + 1);

            for (int i = 0; i < slices; ++i) {
                indices[k    ] = row1 + i;
                indices[k + 1] = row1 + i + 1;
                indices[k + 2] = row1 + i + 1;
                indices[k + 3] = row2 + i + 1;
                indices[k + 4] = row2 + i + 1;
                indices[k + 5] = row2 + i;
                k += 6;

                if (i == 0) {
                    indices[k    ] = row2 + i;
                    indices[k + 1] = row1 + i;
                    k += 2;
                }
            }
        }
    }

    GridBuffers::GridBuffers(VertexArray& gridVertexArray, GridVectors gridVectors) {
        vertices = {};
        colors = {};
        indices = {};

        gridVertexArray.bind();

        vertices = gl::Buffer<float>(gl::BufferType::Vertex, gridVectors.getVertices());
        indices  = gl::Buffer<u8>(gl::BufferType::Index, gridVectors.getIndices());
        colors   = gl::Buffer<float>(gl::BufferType::Vertex, gridVectors.getColors());

        gridVertexArray.addBuffer(0, vertices);
        gridVertexArray.addBuffer(1, colors,4);

        vertices.unbind();
        colors.unbind();
        indices.unbind();
        gridVertexArray.unbind();
        }
    }

    hex::gl::LightSourceVectors::LightSourceVectors(int res) {
        resolution = res;
        radius = 0.05f;
        vertices.resize((res + 1) * (res + 1) * 3);
        normals.resize((res + 1) * (res + 1) * 3);
        colors.resize((res + 1) * (res + 1) * 4);
        indices.resize((res + 1) * (res + 1) * 6);


        auto constexpr two_pi = std::numbers::pi * 2.0f;
        auto constexpr half_pi = std::numbers::pi / 2.0f;
        auto dv = two_pi / resolution;
        auto du = std::numbers::pi / resolution;

        float x,y,z,xy;
        int k,l,m,n;
        k=l=m=n=0;
        //vertical: pi/2 to  -pi/2
        for (int i = 0; i < (resolution +1); i += 1) {
            float u = half_pi - i * du;
            z  = sin(u);
            xy = cos(u);
            //horizontal: 0  to  2pi
            for (int j = 0; j < (resolution+1); j += 1) {
                float v = j * dv;
                x = xy * cos(v);
                y = xy * sin(v);

                normals[m    ] = x;
                normals[m + 1] = y;
                normals[m + 2] = z;

                vertices[m    ] = radius * x;
                vertices[m + 1] = radius * y;
                vertices[m + 2] = radius * z;

                m += 3;

                colors[n    ] = 1.0f;;
                colors[n + 1] = 1.0f;
                colors[n + 2] = 1.0f;
                colors[n + 3] = 1.0f;
                n += 4;

                if (i == 0) {
                    indices[l    ] = k;
                    indices[l + 1] = k + resolution;
                    indices[l + 2] = k + resolution + 1;
                    l += 3;
                    k += 1;
                } else if (i < resolution) {
                    indices[l    ] = k;
                    indices[l + 1] = k + resolution + 1;
                    indices[l + 2] = k + 1;

                    indices[l + 3] = k + 1;
                    indices[l + 4] = k + resolution + 1;
                    indices[l + 5] = k + resolution + 2;

                    k += 1;
                    l += 6;
                } else {
                    indices[l    ] = k;
                    indices[l + 1] = k + resolution + 1;
                    indices[l + 2] = k + 1;
                    l += 3;
                    k += 1;
                }
            }
        }
    }

    void hex::gl::LightSourceVectors::moveTo(Vector<float, 3> positionVector) {

        int k = 0;
        for (int i = 0; i < (resolution + 1)*(resolution + 1); i += 1) {
            vertices[k    ] = radius * normals[k    ] + positionVector[0];
            vertices[k + 1] = radius * normals[k + 1] + positionVector[1];
            vertices[k + 2] = radius * normals[k + 2] + positionVector[2];
            k += 3;
        }
    }

    hex::gl::LightSourceBuffers::LightSourceBuffers(VertexArray &sourceVertexArray, LightSourceVectors sourceVectors) {

        sourceVertexArray.bind();

        vertices = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.vertices);
        indices  = gl::Buffer<u8>(gl::BufferType::Index, sourceVectors.indices);
        //normals  = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.normals);
        colors   = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.colors);

        sourceVertexArray.addBuffer(0, vertices);
        sourceVertexArray.addBuffer(1, colors,4);
        //sourceVertexArray.addBuffer(1, normals);

        vertices.unbind();
        //normals.unbind();
        colors.unbind();
        indices.unbind();
        sourceVertexArray.unbind();
    }

    void hex::gl::LightSourceBuffers::moveVertices(VertexArray& sourceVertexArray, LightSourceVectors& sourceVectors) {

        sourceVertexArray.bind();
        vertices.update(sourceVectors.vertices);
        sourceVertexArray.addBuffer(0, vertices);
        sourceVertexArray.unbind();
    }


