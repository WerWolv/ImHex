#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/utils/guards.hpp>

#include <numbers>

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

        m_program = glCreateProgram();

        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);

        int result = false;
        glGetProgramiv(m_program, GL_LINK_STATUS, &result);
        if (!result) {
            std::vector<char> log(512);
            glGetShaderInfoLog(m_program, log.size(), nullptr, log.data());
            log::error("Failed to link shader: {}", log.data());
        }
    }

    Shader::~Shader() {
        if (m_program != 0)
            glDeleteProgram(m_program);
    }

    Shader::Shader(Shader &&other) noexcept {
        m_program = other.m_program;
        other.m_program = 0;
    }

    Shader& Shader::operator=(Shader &&other) noexcept {
        m_program = other.m_program;
        other.m_program = 0;
        return *this;
    }

    void Shader::bind() const {
        glUseProgram(m_program);
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
        auto uniform = m_uniforms.find(name.data());
        if (uniform == m_uniforms.end()) {
            auto location = glGetUniformLocation(m_program, name.data());
            if (location == -1) {
                log::warn("Uniform '{}' not found in shader", name);
                return -1;
            }

            m_uniforms[name.data()] = location;
            uniform = m_uniforms.find(name.data());
        }

        return uniform->second;
    }

    void Shader::compile(GLuint shader, std::string_view source) const {
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
    Buffer<T>::Buffer(BufferType type, std::span<const T> data) : m_size(data.size()), m_type(GLuint(type)) {
        glGenBuffers(1, &m_buffer);
        glBindBuffer(m_type, m_buffer);
        glBufferData(m_type, data.size_bytes(), data.data(), GL_STATIC_DRAW);
        glBindBuffer(m_type, 0);
    }

    template<typename T>
    Buffer<T>::~Buffer() {
        glDeleteBuffers(1, &m_buffer);
    }

    template<typename T>
    Buffer<T>::Buffer(Buffer &&other) noexcept {
        m_buffer = other.m_buffer;
        m_size = other.m_size;
        m_type = other.m_type;
        other.m_buffer = -1;
    }

    template<typename T>
    Buffer<T>& Buffer<T>::operator=(Buffer &&other) noexcept {
        m_buffer = other.m_buffer;
        m_size = other.m_size;
        m_type = other.m_type;
        other.m_buffer = -1;
        return *this;
    }

    template<typename T>
    void Buffer<T>::bind() const {
        glBindBuffer(m_type, m_buffer);
    }

    template<typename T>
    void Buffer<T>::unbind() const {
        glBindBuffer(m_type, 0);
    }

    template<typename T>
    size_t Buffer<T>::getSize() const {
        return m_size;
    }

    template<typename T>
    void Buffer<T>::draw(unsigned primitive) const {
        switch (m_type) {
            case GL_ARRAY_BUFFER:
                glDrawArrays(primitive, 0, m_size);
                break;
            case GL_ELEMENT_ARRAY_BUFFER:
                glDrawElements(primitive, m_size, impl::getType<T>(), nullptr);
                 break;
        }
    }

    template<typename T>
    void Buffer<T>::update(std::span<const T> data) {
        glBindBuffer(m_type, m_buffer);
        glBufferSubData(m_type, 0, data.size_bytes(), data.data());
        glBindBuffer(m_type, 0);
    }

    template class Buffer<float>;
    template class Buffer<u32>;
    template class Buffer<u16>;
    template class Buffer<u8>;

    VertexArray::VertexArray() {
        glGenVertexArrays(1, &m_array);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &m_array);
    }

    VertexArray::VertexArray(VertexArray &&other) noexcept {
        m_array = other.m_array;
        other.m_array = -1;
    }

    VertexArray& VertexArray::operator=(VertexArray &&other) noexcept {
        m_array = other.m_array;
        other.m_array = -1;
        return *this;
    }

    void VertexArray::bind() const {
        glBindVertexArray(m_array);
    }

    void VertexArray::unbind() const {
        glBindVertexArray(0);
    }


    Texture::Texture(u32 width, u32 height) : m_texture(0), m_width(width), m_height(height) {
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture::~Texture() {
        if (m_texture != 0)
            glDeleteTextures(1, &m_texture);
    }

    Texture::Texture(Texture &&other) noexcept {
        m_texture = other.m_texture;
        other.m_texture = -1;

        m_width = other.m_width;
        m_height = other.m_height;
    }

    Texture& Texture::operator=(Texture &&other) noexcept {
        m_texture = other.m_texture;
        other.m_texture = -1;
        return *this;
    }

    void Texture::bind() const {
        glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    void Texture::unbind() const {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint Texture::getTexture() const {
        return m_texture;
    }

    u32 Texture::getWidth() const {
        return m_width;
    }

    u32 Texture::getHeight() const {
        return m_height;
    }

    GLuint Texture::release() {
        auto copy = m_texture;
        m_texture = -1;

        return copy;
    }

    FrameBuffer::FrameBuffer(u32 width, u32 height) {
        glGenFramebuffers(1, &m_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);

        glGenRenderbuffers(1, &m_renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_renderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBuffer);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    FrameBuffer::~FrameBuffer() {
        glDeleteFramebuffers(1, &m_frameBuffer);
        glDeleteRenderbuffers(1, &m_renderBuffer);
    }

    FrameBuffer::FrameBuffer(FrameBuffer &&other) noexcept {
        m_frameBuffer = other.m_frameBuffer;
        other.m_frameBuffer = -1;
        m_renderBuffer = other.m_renderBuffer;
        other.m_renderBuffer = -1;
    }

    FrameBuffer& FrameBuffer::operator=(FrameBuffer &&other) noexcept {
        m_frameBuffer = other.m_frameBuffer;
        other.m_frameBuffer = -1;
        m_renderBuffer = other.m_renderBuffer;
        other.m_renderBuffer = -1;
        return *this;
    }

    void FrameBuffer::bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    }

    void FrameBuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::attachTexture(const Texture &texture) const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        texture.bind();

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getTexture(), 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    AxesVectors::AxesVectors() {
        m_vertices.resize(36);
        m_colors.resize(48);
        m_indices.resize(18);

        // Vertices are x,y,z. Colors are RGBA. Indices are for the ends of each segment
        // Entries with value zero are unneeded but kept to help keep track of location
        // x-axis
        //vertices[0]=0.0F;  vertices[1]= 0.0F   vertices[2] = 0.0F;  // shaft base
        m_vertices[3] = 1.0F;//vertices[4]= 0.0F   vertices[5] = 0.0F;  // shaft tip
        m_vertices[6] = 0.9F;                      m_vertices[8] = 0.05F; // arrow base
        m_vertices[9] = 0.9F;                      m_vertices[11]=-0.05F; // arrow base
        // y-axis
        //vertices[12]=0.0F;  vertices[13] = 0.0F;  vertices[14]=0.0F;// shaft base
                              m_vertices[16] = 1.0F;//vertices[17]=0.0F;// shaft tip
        m_vertices[18] = 0.05F; m_vertices[19] = 0.9F;//vertices[20]=0.0F;// arrow base
        m_vertices[21] =-0.05F; m_vertices[22] = 0.9F;//vertices[23]=0.0F;// arrow base
        // z-axis
        //vertices[24]=0.0F;   vertices[25]=0.0F  vertices[26] = 0.0F; // shaft base
                                                  m_vertices[29] = 1.0F; // shaft tip
        m_vertices[30] = 0.05F;                     m_vertices[32] = 0.9F; // arrow base
        m_vertices[33] =-0.05F;                     m_vertices[35] = 0.9F; // arrow base
        // x-axis colors
        m_colors[0]  = 0.7F; m_colors[3]  = 1.0F;
        m_colors[4]  = 0.7F; m_colors[7]  = 1.0F;
        m_colors[8]  = 0.7F; m_colors[11] = 1.0F;
        m_colors[12] = 0.7F; m_colors[15] = 1.0F;
        // y-axis colors
        m_colors[17] = 0.7F; m_colors[19] = 1.0F;
        m_colors[21] = 0.7F; m_colors[23] = 1.0F;
        m_colors[25] = 0.7F; m_colors[27] = 1.0F;
        m_colors[29] = 0.7F; m_colors[31] = 1.0F;
        // z-axis colors
        m_colors[34] = 0.7F; m_colors[35] = 1.0F;
        m_colors[38] = 0.7F; m_colors[39] = 1.0F;
        m_colors[42] = 0.7F; m_colors[43] = 1.0F;
        m_colors[46] = 0.7F; m_colors[47] = 1.0F;
        // indices  for x
        m_indices[0]  = 0;  m_indices[1]  = 1;
        m_indices[2]  = 2;  m_indices[3]  = 1;
        m_indices[4]  = 3;  m_indices[5]  = 1;
        // indices for y
        m_indices[6]  = 4;  m_indices[7]  = 5;
        m_indices[8]  = 6;  m_indices[9]  = 5;
        m_indices[10] = 7;  m_indices[11] = 5;
        // indices for z
        m_indices[12] =  8; m_indices[13] = 9;
        m_indices[14] = 10; m_indices[15] = 9;
        m_indices[16] = 11; m_indices[17] = 9;
    }

    AxesBuffers::AxesBuffers(const VertexArray& axesVertexArray, const AxesVectors &axesVectors) {
        m_vertices = {};
        m_colors = {};
        m_indices = {};

        axesVertexArray.bind();

        m_vertices = gl::Buffer<float>(gl::BufferType::Vertex, axesVectors.getVertices());
        m_colors = gl::Buffer<float>(gl::BufferType::Vertex, axesVectors.getColors());
        m_indices = gl::Buffer<u8>(gl::BufferType::Index, axesVectors.getIndices());

        axesVertexArray.addBuffer(0, m_vertices);
        axesVertexArray.addBuffer(1, m_colors, 4);

        m_vertices.unbind();
        m_colors.unbind();
        m_indices.unbind();
        axesVertexArray.unbind();
    }


    GridVectors::GridVectors(int sliceCount) {
        m_slices = sliceCount;
        m_vertices.resize((m_slices + 1) * (m_slices + 1) * 3);
        m_colors.resize((m_slices + 1) * (m_slices + 1) * 4);
        m_indices.resize(m_slices  * m_slices  * 6 + m_slices * 2);
        int k = 0;
        int l = 0;
        for (u32 j = 0; j <= m_slices; ++j) {
            float z = 2.0F * float(j) / float(m_slices) - 1.0F;
            for (u32 i = 0; i <= m_slices; ++i) {
                m_vertices[k   ] = 2.0F * float(i) / float(m_slices) - 1.0F;
                m_vertices[k + 1] = 0.0F;
                m_vertices[k + 2] = z;
                k += 3;
                m_colors[l    ] = 0.5F;
                m_colors[l + 1] = 0.5F;
                m_colors[l + 2] = 0.5F;
                m_colors[l + 3] = 0.3F;
                l += 4;
            }
        }
        k = 0;
        for (u32 j = 0; j < m_slices; ++j) {
            int row1 = j * (m_slices + 1);
            int row2 = (j + 1) * (m_slices + 1);

            for (u32 i = 0; i < m_slices; ++i) {
                m_indices[k    ] = row1 + i;
                m_indices[k + 1] = row1 + i + 1;
                m_indices[k + 2] = row1 + i + 1;
                m_indices[k + 3] = row2 + i + 1;
                m_indices[k + 4] = row2 + i + 1;
                m_indices[k + 5] = row2 + i;
                k += 6;

                if (i == 0) {
                    m_indices[k    ] = row2 + i;
                    m_indices[k + 1] = row1 + i;
                    k += 2;
                }
            }
        }
    }

    GridBuffers::GridBuffers(const VertexArray& gridVertexArray, const GridVectors &gridVectors) {
        m_vertices = {};
        m_colors = {};
        m_indices = {};

        gridVertexArray.bind();

        m_vertices = gl::Buffer<float>(gl::BufferType::Vertex, gridVectors.getVertices());
        m_indices  = gl::Buffer<u8>(gl::BufferType::Index, gridVectors.getIndices());
        m_colors   = gl::Buffer<float>(gl::BufferType::Vertex, gridVectors.getColors());

        gridVertexArray.addBuffer(0, m_vertices);
        gridVertexArray.addBuffer(1, m_colors,4);

        m_vertices.unbind();
        m_colors.unbind();
        m_indices.unbind();
        gridVertexArray.unbind();
    }

    hex::gl::LightSourceVectors::LightSourceVectors(int res) {
        m_resolution = res;
        auto res_sq = m_resolution * m_resolution;
        m_radius = 0.05f;
        m_vertices.resize((res_sq  + 2) * 3);
        m_normals.resize((res_sq + 2) * 3);
        m_colors.resize((res_sq + 2) * 4);
        m_indices.resize(res_sq * 6);


        constexpr auto TwoPi = std::numbers::pi_v<float> * 2.0F;
        constexpr auto HalfPi = std::numbers::pi_v<float> / 2.0F;
        const auto dv = TwoPi / m_resolution;
        const auto du = std::numbers::pi_v<float> / (m_resolution + 1);

        m_normals[0] = 0;
        m_normals[1] = 0;
        m_normals[2] = 1;

        m_vertices[0] = 0;
        m_vertices[1] = 0;
        m_vertices[2] = m_radius;

        m_colors[0] = 1.0;
        m_colors[1] = 1.0;
        m_colors[2] = 1.0;
        m_colors[3] = 1.0;

        // Vertical: pi/2 to  -pi/2
        for (int i = 0; i < m_resolution; i += 1) {
            float u = HalfPi - (i + 1) * du;
            float z  = std::sin(u);
            float xy = std::cos(u);

            // Horizontal: 0  to  2pi
            for (int j = 0; j < m_resolution; j += 1) {
                float v = j * dv;
                float x = xy * std::cos(v);
                float y = xy * std::sin(v);

                i32 n = (i * m_resolution + j + 1) * 3;
                m_normals[n] = x;
                m_normals[n + 1] = y;
                m_normals[n + 2] = z;

                m_vertices[n] = m_radius * x;
                m_vertices[n + 1] = m_radius * y;
                m_vertices[n + 2] = m_radius * z;

                n = (i * m_resolution + j + 1) * 4;
                m_colors[n] = 1.0F;
                m_colors[n + 1] = 1.0F;
                m_colors[n + 2] = 1.0F;
                m_colors[n + 3] = 1.0F;
            }
        }

        i32 n = ((res_sq + 1) * 3);
        m_normals[n    ] = 0;
        m_normals[n + 1] = 0;
        m_normals[n + 2] = -1;

        m_vertices[n    ] = 0;
        m_vertices[n + 1] = 0;
        m_vertices[n + 2] = -m_radius;

        n = ((res_sq + 1) * 4);
        m_colors[n    ] = 1.0;
        m_colors[n + 1] = 1.0;
        m_colors[n + 2] = 1.0;
        m_colors[n + 3] = 1.0;

        // that was the easy part, indices are a bit more complicated
        // and may need some explaining. The RxR grid slices the globe
        // into longitudes which are the vertical slices and latitudes
        // which are the horizontal slices. The latitudes are all full
        // circles except for the poles, so we don't count them as part
        // of the grid. That means that there are R+2 latitudes and R
        // longitudes.Between consecutive latitudes we have 2*R triangles.
        // Since we have R true latitudes there are R-1 spaces between them so
        // between the top and the bottom we have 2*R*(R-1) triangles.
        // the top and bottom have R triangles each, so we have a total of
        // 2*R*(R-1) + 2*R = 2*R*R triangles. Each triangle has 3 vertices,
        // so we have 6*R*R indices.

        // The North Pole is index 0 and the South Pole is index 6*res*res -1
        // The first row of vertices is 1 to res, the second row is res+1 to 2*res etc.

        // First, the North Pole
        for (int i = 0; i < m_resolution; i += 1) {
            m_indices[i * 3] = 0;
            m_indices[i * 3 + 1] = i + 1;
            if (i == m_resolution - 1)
                m_indices[i * 3 + 2] = 1;
            else
                m_indices[i * 3 + 2] = (i + 2);
        }
        // Now the spaces between true latitudes
        for (int i = 0; i < m_resolution - 1; i += 1) {
            // k is the index of the first vertex of the i-th latitude
            i32 k = i * m_resolution + 1;
            // When we go a full circle we need to connect the last vertex to the first, so
            // we do R-1 first because their indices can be computed easily
            for (int j = 0; j < m_resolution - 1; j += 1) {
                // We store the indices of the array where the vertices were store
                // in the triplets that make the triangles. These triplets are stored in
                // an array that has indices itself which can be confusing.
                // l keeps track of the indices of the array that stores the triplets
                // each i brings 6R and each j 6. 3R from the North Pole.
                i32 l = (i * m_resolution + j) * 6 + 3 * m_resolution;

                m_indices[l    ] = k + j;
                m_indices[l + 1] = k + j + m_resolution + 1;
                m_indices[l + 2] = k + j + 1;

                m_indices[l + 3] = k + j;
                m_indices[l + 4] = k + j + m_resolution;
                m_indices[l + 5] = k + j + m_resolution + 1;
            }
            // Now the last vertex of the i-th latitude is connected to the first
            i32 l = (( i + 1) * m_resolution - 1) * 6 + 3 * m_resolution;

            m_indices[l    ] = k + m_resolution  - 1;
            m_indices[l + 1] = k + m_resolution;
            m_indices[l + 2] = k;

            m_indices[l + 3] = k + m_resolution - 1;
            m_indices[l + 4] = k + 2 * m_resolution - 1;
            m_indices[l + 5] = k + m_resolution;

        }
        // Now the South Pole
        i32 k = (m_resolution-1) * m_resolution + 1;
        i32 l = 3 * m_resolution * ( 2 * m_resolution - 1);
        for (int i = 0; i < m_resolution; i += 1) {
            if (i == m_resolution -1)
                m_indices[l + i * 3] = k;
            else
                m_indices[l + i * 3] = k + i + 1;

            m_indices[l + i * 3 + 1] = k + i;
            m_indices[l + i * 3 + 2] = k + m_resolution;
        }
    }

    void LightSourceVectors::moveTo(const Vector<float, 3> &position) {
        auto vertexCount = m_vertices.size();

        for (unsigned k = 0; k < vertexCount; k += 3) {
            m_vertices[k    ] = m_radius * m_normals[k    ] + position[0];
            m_vertices[k + 1] = m_radius * m_normals[k + 1] + position[1];
            m_vertices[k + 2] = m_radius * m_normals[k + 2] + position[2];
        }
    }

    LightSourceBuffers::LightSourceBuffers(const VertexArray &sourceVertexArray, const LightSourceVectors &sourceVectors) {
        sourceVertexArray.bind();

        m_vertices = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.getVertices());
        m_indices  = gl::Buffer<u16>(gl::BufferType::Index, sourceVectors.getIndices());
        m_normals  = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.getNormals());
        m_colors   = gl::Buffer<float>(gl::BufferType::Vertex, sourceVectors.getColors());

        sourceVertexArray.addBuffer(0, m_vertices);
        sourceVertexArray.addBuffer(1, m_normals);
        sourceVertexArray.addBuffer(2, m_colors, 4);

        m_vertices.unbind();
        m_normals.unbind();
        m_colors.unbind();
        m_indices.unbind();
        sourceVertexArray.unbind();
    }

    void LightSourceBuffers::moveVertices(const VertexArray& sourceVertexArray, const LightSourceVectors& sourceVectors) {
        sourceVertexArray.bind();

        m_vertices.update(sourceVectors.getVertices());
        sourceVertexArray.addBuffer(0, m_vertices);

        sourceVertexArray.unbind();
    }

    void LightSourceBuffers::updateColors(const VertexArray& sourceVertexArray, const LightSourceVectors& sourceVectors) {
        sourceVertexArray.bind();

        m_colors.update(sourceVectors.getColors());
        sourceVertexArray.addBuffer(2, m_colors, 4);

        sourceVertexArray.unbind();
    }

}



