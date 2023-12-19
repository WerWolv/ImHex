#pragma once

#include <hex.hpp>

#include <hex/helpers/concepts.hpp>

#include <cmath>
#include <vector>
#include <map>
#include <span>
#include <string>

#include <opengl_support.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

namespace hex::gl {

    namespace impl {

        template<typename T>
        GLuint getType() {
            if constexpr (std::is_same_v<T, float>)
                return GL_FLOAT;
            else if constexpr (std::is_same_v<T, u8>)
                return GL_UNSIGNED_BYTE;
            else if constexpr (std::is_same_v<T, u16>)
                return GL_UNSIGNED_SHORT;
            else if constexpr (std::is_same_v<T, u32>)
                return GL_UNSIGNED_INT;
            else {
                static_assert(hex::always_false<T>::value, "Unsupported type");
                return 0;
            }
        }

    }

    template<typename T, size_t Size>
    class Vector {
    public:
        Vector() = default;
        Vector(const T val) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] = val;
        }

        Vector(std::array<T, Size> data) : m_data(data) { }
        Vector(Vector &&other) noexcept : m_data(std::move(other.m_data)) { }
        Vector(const Vector &other) : m_data(other.m_data) { }

        T &operator[](size_t index) { return m_data[index]; }
        const T &operator[](size_t index) const { return m_data[index]; }

        std::array<T, Size> &asArray()  { return m_data; }

        T *data() { return m_data.data(); }
        const T *data() const { return m_data.data(); }

        [[nodiscard]] size_t size() const { return m_data.size(); }

        auto operator=(const Vector& other) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] = other[i];
            return *this;
        }

        auto operator+=(const Vector& other) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] += other.m_data[i];

            return *this;
        }

        auto operator+=(const T scalar) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] += scalar;

            return *this;
        }

        auto operator-=(Vector other) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] -= other.m_data[i];

            return *this;
        }


        auto operator-=(const T scalar) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] -= scalar;

            return *this;
        }

        Vector operator*=(const T scalar) {
            for (size_t i = 0; i < Size; i++)
                m_data[i] *= scalar;

            return *this;
        }

        auto operator*(const T scalar) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] *= scalar;
            return copy;
        }

        auto operator+(const Vector& other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] += other[i];
            return copy;
        }

        auto operator-(const Vector& other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] -= other[i];
            return copy;
        }

        auto dot(const Vector& other) {
            T result = 0;
            for (size_t i = 0; i < Size; i++)
                result += m_data[i] * other[i];
            return result;
        }

        auto cross(const Vector& other) {
            static_assert(Size == 3, "Cross product is only defined for 3D vectors");
            return Vector({m_data[1] * other[2] - m_data[2] * other[1],
                           m_data[2] * other[0] - m_data[0] * other[2],
                           m_data[0] * other[1] - m_data[1] * other[0]});
        }

        auto magnitude() {
            return std::sqrt(this->dot(*this));
        }

        auto normalize() {
            auto copy = *this;
             auto length = copy.magnitude();
            for (size_t i = 0; i < Size; i++)
                copy[i] /= length;
            return copy;
        }

        auto operator==(const Vector& other) {
            for (size_t i = 0; i < Size; i++)
                if (m_data[i] != other[i])
                    return false;
            return true;
        }

    private:
        std::array<T, Size> m_data;
    };

    template<typename T, size_t Rows, size_t Columns>
    class Matrix {
    public:
        Matrix(const T &init) {
            for (size_t i = 0; i < Rows; i++)
                for (size_t j = 0; j < Columns; j++)
                    mat[i * Columns + j] = init;
        }

        Matrix(const Matrix &A) {
            mat = A.mat;
        }

        virtual ~Matrix() {}

        size_t getRows() const {
            return Rows;
        }

        size_t getColumns() const {
            return Columns;
        }

        T *data() { return this->mat.data(); }

        const T *data() const { return this->mat.data(); }


        T &getElement(int row,int col) {
            return this->mat[row*Columns+col];
        }

        Vector<T,Rows> getColumn(int col) {
            Vector<T,Rows> result;
            for (size_t i = 0; i < Rows; i++)
                result[i] = this->mat[i*Columns+col];
            return result;
        }

        Vector<T,Columns> getRow(int row) {
            Vector<T,Columns> result;
            for (size_t i = 0; i < Columns; i++)
                result[i] = this->mat[row*Columns+i];
            return result;
        }

        void updateRow(int row, Vector<T,Columns> values) {
            for (size_t i = 0; i < Columns; i++)
                this->mat[row*Columns+i] = values[i];
        }

        void updateColumn(int col, Vector<T,Rows> values) {
            for (size_t i = 0; i < Rows; i++)
                this->mat[i*Columns+col] = values[i];
        }

        void updateElement( int row,int col, T value) {
            this->mat[row*Columns + col] = value;
        }

        T &operator()( const int &row,const int &col) {
            return this->mat[row*Columns + col];
        }

        const T &operator()(const unsigned& row,const unsigned& col ) const {
            return this->mat[row*Columns + col];
        }

        Matrix& operator=(const Matrix& A) {
            if (&A == this)
                return *this;

            for (size_t i = 0; i < Rows; i++)
                for (size_t j = 0; j < Columns; j++)
                    mat[i*Columns+j] = A(i, j);
            return *this;
        }

        Matrix operator+(const Matrix& A) {
            Matrix result(0.0);

            for (size_t i = 0; i < Rows; i++)
                for (size_t j = 0; j < Columns; j++)
                    result(i, j) = this->mat[i*Columns+j] + A(i, j);
            return result;
        }

        Matrix operator-(const Matrix& A) {
            Matrix result(0.0);

            for (size_t i = 0; i < Rows; i++)
                for (size_t j = 0; j < Columns; j++)
                    result(i, j) = this->mat[i*Columns+j] - A(i, j);
            return result;
        }

        static Matrix identity() {
            Matrix  I(0);
            for (size_t i = 0; i < Rows; i++)
                for (size_t j = 0; j < Columns; j++)
                    if(i == j)
                        I.updateElement(i, j, 1);
            return I;
        }

        Matrix transpose() {
            Matrix  t(0);
            for (size_t i = 0; i < Columns; i++)
                for (size_t j = 0; j < Rows; j++)
                    t.updateElement(i, j, this->mat[j*Rows+i]);

            return t;
        }

    private:
        std::array<T,Rows * Columns> mat;
    };

    template<typename T, size_t Rows, size_t Columns, size_t OtherDimension>
    Matrix<T,Rows, Columns> operator*(const Matrix<T, Rows, OtherDimension> &A, const Matrix<T, OtherDimension, Columns> &B) {
        Matrix<T, Rows, Columns> result(0.0);

        for (size_t i = 0; i < Rows; i++)
            for (size_t j = 0; j < Columns; j++)
                for (size_t k = 0; k < OtherDimension; k++)
                    result(i, j) += A(i,k) * B(k, j);

        return result;
    }

    template<typename T, size_t Rows, size_t Columns>
    Matrix<T, Rows, Columns> operator*(const Vector<T, Rows> &a, const Vector<T, Columns> &b) {
        Matrix<T, Rows, Columns> result(0);

        for (size_t i = 0; i < Rows; i++)
            for (size_t j = 0; j < Columns; j++)
                result.updateElement(i, j, a[i] * b[j]);

        return result;
    }

    template<typename T, size_t Rows, size_t Columns>
    Vector<T, Rows> operator*(const Matrix<T, Rows, Columns> &A, const Vector<T, Columns> &b) {
        Vector<T, Rows> result(0);

        for (size_t i = 0; i < Rows; i++)
            for (size_t j = 0; j < Columns; j++)
                result[i] += A(i, j) * b[j];

        return result;
    }

    template<typename T, size_t Rows, size_t Columns>
    Vector<T, Columns> operator*(const Vector<T, Rows> &b, const Matrix<T,Rows,Columns> &A) {
        Vector<T, Columns> result(0);

        for (size_t i = 0; i < Rows; i++)
            for (size_t j = 0; j < Columns; j++)
                result[j] += b[i] * A(i, j);

        return result;
    }

// Convert horizontal (Xh) vertical (Yv)  and spin (Zs) angles to a rotation matrix.
// Xh: Horizontal rotation, also known as heading or yaw.
// Yv: Vertical rotation, also known as pitch or elevation.
// Zs: Spin rotation, also known as intrinsic rotation, roll or bank.
// Each column of the rotation matrix represents left, up and forward axis.
// Angles of rotation are lowercase (x,y,z) in radians and the rotation matrix is uppercase (X,Y,Z).
// S = sin, C = cos
// The order of rotation is Yaw->Pitch->Roll (Zs*Yv*Xh)
//        Zs             Yv          Xh
// | Cz -Sz  0  0| |Cy  0 Sy  0| |1  0   0  0|   | Cz -Sz 0 0| | Cy Sy*Sx  Sy*Cx 0|
// | Sz  Cz  0  0|*| 0  1  0  0|*|0 Cx -Sx  0| = | Sz  Cz 0 0|*| 0  Cx    -Sx    0|
// |  0   0  1  0| |-Sy 0 Cy  0| |0 Sx  Cx  0|   | 0   0  1 0| |-Sy Sx*Cy  Cx*Cy 0|
// |  0   0  0  1| | 0  0  0  1| |0  0   0  1|   | 0   0  0 1| | 0  0      0     1|
//    Left      Up         Forward
// | Cz*Cy Cz*Sy*Sx-Sz*Cx Sz*Sx+Cz*Sy*Cx 0|
// | Sz*Cy Sz*Sy*Sx+Cz*Cx Cz*Sy*Sx-Sz*Cx 0|
// |-Sy*Cx Cy*Cx          Sy             0|
// | 0     0              0              1|

// The order of rotation is Pitch->Yaw->Roll (Zs*Xh*Yv)
//        Zs           Xh              Yv
// | Cz -Sz  0  0| |1  0   0  0| |Cy  0 Sy  0|   | Cz -Sz 0 0| | Cy    0   Sy    0|
// | Sz  Cz  0  0|*|0 Cx -Sx  0|*| 0  1  0  0| = | Sz  Cz 0 0|*| Sx*Sy Cx -Sx*Cy 0|=
// |  0   0  1  0| |0 Sx  Cx  0| |-Sy 0 Cy  0|   | 0   0  1 0| |-Cx*Sy Sx  Cx*Cy 0|
// |  0   0  0  1| |0  0   0  1| | 0  0  0  1|   | 0   0  0 1| | 0     0   0     1|
//    Left           Up     Forward
// | Cz*Cy-Sz*Sx*Sy -Sz*Cx Cz*Sy+Sz*Sx*Cy 0|
// | Sz*Cy+Cz*Sx*Sy  Cz*Cx Sz*Sy-Cz*Sx*Cy 0|
// |-Cx*Sy           Sx    Cx*Cy          0|
// | 0                0    0              1|


// The order of rotation is Roll->Pitch->Yaw (Xh*Yv*Zs)
//      Xh             Yv            Zs
// |1  0   0  0| | Cy  0 Sy  0| |Cz -Sz 0  0|   |1  0   0  0| | Cy*Cz -Cy*Sz  Sy  0|
// |0 Cx -Sx  0|*|  0  1  0  0|*|Sz  Cz 0  0| = |0 Cx -Sx  0|*| Sz     Cz      0  0|
// |0 Sx  Cx  0| |-Sy  0 Cy  0| | 0   0 1  0|   |0 Sx  Cx  0| |-Sy*Cz  Sy*Sz  Cy  0|
// |0  0   0  1| |  0  0  0  1| | 0   0 0  1|   |0  0   0  1| |  0      0     0   1|
//    Left              Up              Forward
//    | Cy*Cz          -Cy*Sz           Sy     0|
//   =| Sx*Sy*Cz+Cx*Sz -Sx*Sy*Sz+Cx*Cz -Sx*Cy  0|
//    |-Cx*Sy*Cz+Sx*Sz  Cx*Sy*Sz+Sx*Cz  Cx*Cy  0|
//    |  0              0               0      1|
// just write final answer from here on
// The order of rotation is Pitch->Roll->Yaw (Xh*Zs*Yv)
//         Left           Up    Forward
//         |Cz*Cy         -Sz    Cz*Sy          0|
//Xh*Zs*Yv=|Cx*Cy*Sz+Sx*Sy Cx*Cz Cx*Sz*Sy-Cy*Sx 0|
//         |Cy*Sx*Sz-Cx*Sy Cz*Sx Sx*Sz*Sy+Cx*Cy 0|
//         |0			   0	 0              1|
// The order of rotation is Roll->Yaw->Pitch (Yv*Xh*Zs)
//         Left           Up              Forward
//         |Cy*Cz+Sy*Sx*Sz  Cz*Sy*Sx-Cy*Sz Cx*Sy  0|
//Yv*Xh*Zs=|Cx*Sz	  		 Cx*Cz		   -Sx    0|
//         |Cy*Sx*Sz-Cz*Sy	 Cy*Cz*Sx+Sy*Sz Cy*Cx 0|
//         |0				 0			    0     1|
// The order of rotation is Yaw->Roll->Pitch (Yv*Zs*Xh)
//           Left    Up             Forward
//          |Cy*Cz	Sy*Sx-Cy*Cx*Sz	Cx*Sy+Cy*Sz*Sx 0|
//Yv*Zs*Xh= |Sz		Cz*Cx		   -Cz*Sx          0|
//          |-Cz*Sy	Cy*Sx+Cx*Sy*Sz	Cy*Cx-Sy*Sz*Sx 0|
//          |0		0				0              1|

    enum RotationSequence {
       XYZ,
       XZY,
       YXZ,
       YZX,
       ZXY,
       ZYX
    };

    template<typename T>
    Matrix<T, 4, 4> getRotationMatrix(Vector<T,3> ypr, bool radians, RotationSequence rotationSequence) {
        Matrix<T,4,4> rotation(0);

        T Sx, Cx, Sy, Cy, Sz, Cz;
        Vector<T,3> angles = ypr;
        if(!radians)
            angles *= M_PI/180;

        Sx = -sin(angles[0]); Cx = cos(angles[0]);
        Sy = -sin(angles[1]); Cy = cos(angles[1]);
        Sz = -sin(angles[2]); Cz = cos(angles[2]);

        switch (rotationSequence) {
            case ZXY:
                // | Cz*Cy-Sz*Sx*Sy -Sz*Cx Cz*Sy+Sz*Sx*Cy 0|
                // | Sz*Cy+Cz*Sx*Sy  Cz*Cx Sz*Sy-Cz*Sx*Cy 0|
                // |-Cx*Sy           Sx    Cx*Cy          0|
                // | 0                0    0              1|
                rotation.updateElement(0, 0, Cz * Cy - Sz * Sx * Sy);
                rotation.updateElement(0, 1, -Sz * Cx);
                rotation.updateElement(0, 2, Cz * Sy + Sz * Sx * Cy);

                rotation.updateElement(1, 0, Sz * Cy + Cz * Sx * Sy);
                rotation.updateElement(1, 1, Cz * Cx);
                rotation.updateElement(1, 2, Sz * Sy - Cz * Sx * Cy);

                rotation.updateElement(2, 0, -Cx * Sy);
                rotation.updateElement(2, 1, Sx);
                rotation.updateElement(2, 2, Cx * Cy);
                break;
            case ZYX:
                // | Cz*Cy Cz*Sy*Sx-Sz*Cx Sz*Sx+Cz*Sy*Cx 0|
                // | Sz*Cy Sz*Sy*Sx+Cz*Cx Sz*Sy*Cx-Cz*Sx 0|
                // |-Sy    Cy*Sx          Cy*Cx          0|
                // | 0     0              0              1|
                rotation.updateElement(0, 0, Cz * Cy);
                rotation.updateElement(0, 1, Sx * Sy * Cz - Sz * Cx);
                rotation.updateElement(0, 2, Sz * Sx + Cz * Sy * Cx);

                rotation.updateElement(1, 0, Sz * Cy);
                rotation.updateElement(1, 1, Sz * Sy * Sx + Cz * Cx);
                rotation.updateElement(1, 2, Sz * Sy * Cx - Cz * Sx);

                rotation.updateElement(2, 0, -Sy);
                rotation.updateElement(2, 1, Cy * Sx);
                rotation.updateElement(2, 2, Cy*Cx);
                break;
            case XYZ:
                //    | Cy*Cz          -Cy*Sz           Sy     0|
                //   =| Sx*Sy*Cz+Cx*Sz -Sx*Sy*Sz+Cx*Cz -Sx*Cy  0|
                //    |-Cx*Sy*Cz+Sx*Sz  Cx*Sy*Sz+Sx*Cz  Cx*Cy  0|
                //    |  0              0               0      1|
                rotation.updateElement(0, 0,  Cy * Cz);
                rotation.updateElement(0, 1, -Cy * Sz);
                rotation.updateElement(0, 2,  Sy);

                rotation.updateElement(1, 0,  Sx * Sy * Cz + Cx * Sz);
                rotation.updateElement(1, 1, -Sx * Sy * Sz + Cx * Cz);
                rotation.updateElement(1, 2, -Sx * Cy);

                rotation.updateElement(2, 0, -Cx * Sy * Cz + Sx * Sz);
                rotation.updateElement(2, 1,  Cx * Sy * Sz + Sx * Cz);
                rotation.updateElement(2, 2,  Cx * Cy);
                break;
            case XZY:
                //         |Cz*Cy         -Sz    Cz*Sy          0|
                //Xh*Zs*Yv=|Cx*Cy*Sz+Sx*Sy Cx*Cz Cx*Sz*Sy-Cy*Sx 0|
                //         |Cy*Sx*Sz-Cx*Sy Cz*Sx Sx*Sz*Sy+Cx*Cy 0|
                //         |0			   0	 0              1|
                rotation.updateElement(0, 0, Cy * Cz);
                rotation.updateElement(0, 1, -Sz);
                rotation.updateElement(0, 2, Cz * Sy);

                rotation.updateElement(1, 0, Cx * Cy * Sz + Sx * Sy);
                rotation.updateElement(1, 1, Cx * Cz);
                rotation.updateElement(1, 2, Cx * Sy * Sz - Sx * Cy);

                rotation.updateElement(2, 0, Sx * Cy * Sz - Cx * Sy);
                rotation.updateElement(2, 1, Sx * Cz);
                rotation.updateElement(2, 2, Sx * Sy * Sz + Cx * Cy);
                break;
            case YXZ:
                //         |Cy*Cz+Sy*Sx*Sz  Cz*Sy*Sx-Cy*Sz Cx*Sy  0|
                //Yv*Xh*Zs=|Cx*Sz	  		 Cx*Cz		   -Sx    0|
                //         |Cy*Sx*Sz-Cz*Sy	 Cy*Cz*Sx+Sy*Sz Cy*Cx 0|
                //         |0				 0			    0     1|
                rotation.updateElement(0, 0,  Cy*Cz+Sy*Sx*Sz );
                rotation.updateElement(0, 1,  Cz*Sy*Sx-Cy*Sz);
                rotation.updateElement(0, 2,  Sy*Cx);

                rotation.updateElement(1, 0,  Cx*Sz);
                rotation.updateElement(1, 1,  Cx*Cz);
                rotation.updateElement(1, 2, -Sx);

                rotation.updateElement(2, 0,  Cy*Sx*Sz-Cz*Sy);
                rotation.updateElement(2, 1,  Cy*Cz*Sx+Sy*Sz);
                rotation.updateElement(2, 2,  Cy*Cx);
                break;
            case YZX:
                //          |Cy*Cz	Sy*Sx-Cy*Cx*Sz	Cx*Sy+Cy*Sz*Sx 0|
                //Yv*Zs*Xh= |Sz		Cz*Cx		   -Cz*Sx          0|
                //          |-Cz*Sy	Cy*Sx+Cx*Sy*Sz	Cy*Cx-Sy*Sz*Sx 0|
                //          |0		0				0              1|
                rotation.updateElement(0, 0,  Cy*Cz);
                rotation.updateElement(0, 1,  Sy*Sx-Cy*Cx*Sz);
                rotation.updateElement(0, 2,  Cx*Sy+Cy*Sz*Sx);

                rotation.updateElement(1, 0,  Sz);
                rotation.updateElement(1, 1,  Cz*Cx);
                rotation.updateElement(1, 2, -Cz*Sx);

                rotation.updateElement(2, 0, -Cz*Sy);
                rotation.updateElement(2, 1,  Cy*Sx+Cx*Sy*Sz);
                rotation.updateElement(2, 2,  Cy*Cx-Sy*Sz*Sx);
                break;


        }

        rotation.updateElement(3, 3, 1);

        return rotation;
    }

    template<typename T>
    Matrix<T, 4, 4> getRotationMatrixFromVectorAngle(Vector<T, 4> rotationVector, bool radians) {
        Vector<T,3> rotationVector3 = {{rotationVector[0], rotationVector[1], rotationVector[2]}};
        T theta = rotationVector3.magnitude();
        if (!radians)
            theta *= M_PI / 180;
        Vector<T,3> axis = rotationVector3;
        if (theta != 0)
            axis = axis.normalize();
        Matrix<T,4,4> rotation = Matrix<T,4,4>::identity();
        T S = sin(theta);
        T C = cos(theta);
        T OMC = 1 - C;
        T a00 = axis[0] * axis[0] * OMC;
        T a01 = axis[0] * axis[1] * OMC;
        T a02 = axis[0] * axis[2] * OMC;
        T a10 = axis[1] * axis[0] * OMC;
        T a11 = axis[1] * axis[1] * OMC;
        T a12 = axis[1] * axis[2] * OMC;
        T a20 = axis[2] * axis[0] * OMC;
        T a21 = axis[2] * axis[1] * OMC;
        T a22 = axis[2] * axis[2] * OMC;
        T a0S = axis[0] * S;
        T a1S = axis[1] * S;
        T a2S = axis[2] * S;

        rotation.updateElement(0, 0, C + a00);
        rotation.updateElement(0, 1, a01 - a2S);
        rotation.updateElement(0, 2, a02 + a1S);
        rotation.updateElement(1, 0, a10 + a2S);
        rotation.updateElement(1, 1, C + a11);
        rotation.updateElement(1, 2, a12 - a0S);
        rotation.updateElement(2, 0, a20 - a1S);
        rotation.updateElement(2, 1, a21 + a0S);
        rotation.updateElement(2, 2, C + a22);
        return rotation;

    }

    enum class MatrixElements {
        r00, r01, r02,
        r10, r11, r12,
        r20, r21, r22,
    };

    template<typename T>
    T findValue(Vector<T,3> ypr, MatrixElements matrixElement, RotationSequence rotationSequence) {
        T Sx, Cx, Sy, Cy, Sz, Cz;
        Vector<T,3> angles = ypr;


        Sx = sin(angles[0]); Cx = cos(angles[0]);
        Sy = sin(angles[1]); Cy = cos(angles[1]);
        Sz = sin(angles[2]); Cz = cos(angles[2]);

        switch (rotationSequence) {
            case ZXY:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cz * Cy - Sz * Sx * Sy;
                    case MatrixElements::r01:
                        return -Sz * Cx;
                    case MatrixElements::r02:
                        return Cz * Sy + Sz * Sx * Cy;
                    case MatrixElements::r10:
                        return Sz * Cy + Cz * Sx * Sy;
                    case MatrixElements::r11:
                        return Cz * Cx;
                    case MatrixElements::r12:
                        return Sz * Sy - Cz * Sx * Cy;
                    case MatrixElements::r20:
                        return -Cx * Sy;
                    case MatrixElements::r21:
                        return Sx;
                    case MatrixElements::r22:
                        return Cx * Cy;
                }
                break;
            case ZYX:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cz * Cy;
                    case MatrixElements::r01:
                        return Sx * Sy * Cz + Cx * Sz;
                    case MatrixElements::r02:
                        return -Cx * Sy * Cz + Sx * Sz;
                    case MatrixElements::r10:
                        return Cz * Sy;
                    case MatrixElements::r11:
                        return Sx * Sy * Sz - Cx * Cz;
                    case MatrixElements::r12:
                        return Cx * Sy * Sz + Sx * Cz;
                    case MatrixElements::r20:
                        return -Sy;
                    case MatrixElements::r21:
                        return Cy * Sx;
                    case MatrixElements::r22:
                        return Cy * Cx;
                }
                break;
            case XYZ:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cy * Cz;
                    case MatrixElements::r01:
                        return -Cy * Sz;
                    case MatrixElements::r02:
                        return Sy;
                    case MatrixElements::r10:
                        return Sx * Sy * Cz + Cx * Sz;
                    case MatrixElements::r11:
                        return -Sx * Sy * Sz + Cx * Cz;
                    case MatrixElements::r12:
                        return -Sx * Cy;
                    case MatrixElements::r20:
                        return -Cx * Sy * Cz + Sx * Sz;
                    case MatrixElements::r21:
                        return Cx * Sy * Sz + Sx * Cz;
                    case MatrixElements::r22:
                        return Cx * Cy;
                }
                break;
            case XZY:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cy * Cz;
                    case MatrixElements::r01:
                        return -Sz;
                    case MatrixElements::r02:
                        return Cz * Sy;
                    case MatrixElements::r10:
                        return Cx * Cy * Sz + Sx * Sy;
                    case MatrixElements::r11:
                        return Cx * Cz;
                    case MatrixElements::r12:
                        return Cx * Sy * Sz - Sx * Cy;
                    case MatrixElements::r20:
                        return Sx * Cy * Sz - Cx * Sy;
                    case MatrixElements::r21:
                        return Sx * Cz;
                    case MatrixElements::r22:
                        return Sx * Sy * Sz + Cx * Cy;
                }
                break;
            case YXZ:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cy * Cz + Sy * Sx * Sz;
                    case MatrixElements::r01:
                        return Cz * Sy * Sx - Cy * Sz;
                    case MatrixElements::r02:
                        return Cx * Sy;
                    case MatrixElements::r10:
                        return Cx * Sz;
                    case MatrixElements::r11:
                        return Cx * Cz;
                    case MatrixElements::r12:
                        return -Sx;
                    case MatrixElements::r20:
                        return -Cz * Sy + Cy * Sx * Sz;
                    case MatrixElements::r21:
                        return Cy * Cz * Sx + Sy * Sz;
                    case MatrixElements::r22:
                        return Cy * Cx;
                }
                break;
            case YZX:
                switch (matrixElement) {
                    case MatrixElements::r00:
                        return Cy * Cz;
                    case MatrixElements::r01:
                        return Sy * Sx - Cy * Cx * Sz;
                    case MatrixElements::r02:
                        return Cx * Sy + Cy * Sz * Sx;
                    case MatrixElements::r10:
                        return Sz;
                    case MatrixElements::r11:
                        return Cx * Cz;
                    case MatrixElements::r12:
                        return -Cz * Sx;
                    case MatrixElements::r20:
                        return -Cz * Sy;
                    case MatrixElements::r21:
                        return Cy * Sx + Cx * Sy * Sz;
                    case MatrixElements::r22:
                        return Cy * Cx - Sy * Sz * Sx;
                }
                break;
        }
        return 0;
    }

    template<typename T>
    Matrix<T, 4,4 > getTransformMatrix(Vector<T,3> xyz, Vector<T,3> ypr, bool radians) {
        Matrix<T,4,4> transform( 0);

        Matrix<T,3,3> rotation = getRotationMatrix(ypr, radians);

        for(int i=0; i<3; i++)
            for(int j=0; j<3; j++)
                transform.updateElement(i, j, rotation.getElement(i, j));

        transform.updateElement(0,3, xyz[0]);
        transform.updateElement(1,3, xyz[1]);
        transform.updateElement(2,3, xyz[2]);
        transform.updateElement(3,3, 1);

        return transform;
    }

    template<typename T>
    Vector<T,3> getTranslationVector(Matrix<T, 4,4 > transform_matrix) {
        Vector<T,3> xyz;

        xyz.push_back(transform_matrix.getElement(0,3));
        xyz.push_back(transform_matrix.getElement(1,3));
        xyz.push_back(transform_matrix.getElement(2,3));

        return xyz;
    }

    template<typename T>
    Vector<T,3> getYprVector(Matrix<T, 4,4 > transform_matrix) {
        Vector<T,3> result;

        Matrix<T,3,3> rotation(0);
        for(int i=0; i<3; i++)
            for(int j=0; j<3; j++)
                rotation.updateElement(i, j, transform_matrix.getElement(i, j));

        T sy = sqrt(rotation.getElement(0,0) * rotation.getElement(0,0) +  rotation.getElement(1,0) * rotation.getElement(1,0) );

        bool singular = sy < 1e-6;

        T x, y, z;
        if (!singular) {
            x =  atan2(rotation.getElement(1,0), rotation.getElement(0,0));
            y = atan2(-rotation.getElement(2,0), sy);
            z = atan2(rotation.getElement(2,1), rotation.getElement(2,2));
        }
        else {
            x = 0;
            y = atan2(-rotation.getElement(2,0), sy);
            z =  atan2(-rotation.getElement(1,2), rotation.getElement(1,1));
        }

        result.push_back(x);
        result.push_back(y);
        result.push_back(z);

        return result;
    }

    Matrix<float,4,4> GetPerspectiveMatrix(	float viewWidth, float viewHeight, float nearVal, float farVal, bool actionType = false);
    Matrix<float,4,4> GetOrthographicMatrix( float viewWidth, float viewHeight, float nearVal, float farVal, bool actionType = false);


    template<typename T>
    static Matrix<T,4,4> GetObliqueMatrix(	T width, T height,T nearVal,T farVal, bool actionType = false) {
        int sign =1;
        if (actionType)
            sign=-1;
        Matrix<T,4,4> result(0);
        result.updateElement(0,0,sign * nearVal/width);
        result.updateElement(1,1, sign *  nearVal/height);
        result.updateElement(2,2,sign * (farVal + nearVal)/( farVal - nearVal ));
        result.updateElement(3,2,sign * 2*farVal * nearVal/( farVal - nearVal ));
        result.updateElement(2,3,-sign);

        return result;
    }


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

        void setUniform(std::string_view name, const int &value);
        void setUniform(std::string_view name, const float &value);

        template<size_t N>
        void setUniform(std::string_view name, const Vector<float, N> &value) {
            if (N == 2)
                glUniform2f(getUniformLocation(name), value[0], value[1]);
            else if (N == 3)
                glUniform3f(getUniformLocation(name), value[0], value[1], value[2]);
            else if (N == 4)
                glUniform4f(getUniformLocation(name), value[0], value[1], value[2],value[3]);
        }

        template<size_t N>
        void setUniform(std::string_view name, Matrix<float, N, N> &value){
            glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, value.data());
        }

    private:
        void compile(GLuint shader, std::string_view source) const;
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
        Buffer(BufferType type, std::span<const T> data);
        ~Buffer();
        Buffer(const Buffer&) = delete;
        Buffer(Buffer&& other) noexcept;

        Buffer& operator=(const Buffer&) = delete;
        Buffer& operator=(Buffer&& other) noexcept;

        void bind() const;
        void unbind() const;

        void draw(unsigned primitive) const;
        size_t getSize() const;
        void update(std::span<const T> data);
    private:
        GLuint m_buffer = 0;
        size_t m_size = 0;
        GLuint m_type = 0;
    };

    extern template class Buffer<float>;
    extern template class Buffer<u32>;
    extern template class Buffer<u16>;
    extern template class Buffer<u8>;

    class VertexArray {
    public:
        VertexArray();
        ~VertexArray();
        VertexArray(const VertexArray&) = delete;
        VertexArray(VertexArray&& other) noexcept;

        VertexArray& operator=(const VertexArray&) = delete;
        VertexArray& operator=(VertexArray&& other) noexcept;

        template<typename T>
        void addBuffer(u32 index, const Buffer<T> &buffer, u32 size = 3) const {
            glEnableVertexAttribArray(index);
            buffer.bind();
            glVertexAttribPointer(index, size, gl::impl::getType<T>(), GL_FALSE, size * sizeof(T), nullptr);
            buffer.unbind();
        }

        void bind() const;
        void unbind() const;

    private:
        GLuint m_array = 0;
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
        FrameBuffer(u32 width, u32 height);
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

    class AxesVectors {
    public:
        AxesVectors();

        const std::vector<float>& getVertices() const {
            return m_vertices;
        }

        const std::vector<float>& getColors() const {
            return m_colors;
        }

        const std::vector<u8>& getIndices() const {
            return m_indices;
        }

    private:
        std::vector<float> m_vertices;
        std::vector<float> m_colors;
        std::vector<u8> m_indices;

    };

    class AxesBuffers {
    public:
        AxesBuffers(const VertexArray& axesVertexArray, const AxesVectors &axesVectors);

        const gl::Buffer<float>& getVertices() const {
            return m_vertices;
        }

        const gl::Buffer<float>& getColors() const {
            return m_colors;
        }

        const gl::Buffer<u8>& getIndices() const {
            return m_indices;
        }

    private:
        gl::Buffer<float> m_vertices;
        gl::Buffer<float> m_colors;
        gl::Buffer<u8> m_indices;
    };

    class GridVectors {
    public:
        GridVectors(int sliceCount);

        u32 getSlices() const {
            return m_slices;
        }

        const std::vector<float>& getVertices() const {
            return m_vertices;
        }

        const std::vector<float>& getColors() const {
            return m_colors;
        }

        const std::vector<u8>& getIndices() const {
            return m_indices;
        }

    private:
        u32 m_slices;
        std::vector<float> m_vertices;
        std::vector<float> m_colors;
        std::vector<u8> m_indices;
    };

    class GridBuffers {
    public:
        GridBuffers(const VertexArray &gridVertexArray, const GridVectors &gridVectors);

        const gl::Buffer<float>& getVertices() const {
            return m_vertices;
        }

        const gl::Buffer<float>& getColors() const {
            return m_colors;
        }

        const gl::Buffer<u8>& getIndices() const {
            return m_indices;
        }

    private:
        gl::Buffer<float> m_vertices;
        gl::Buffer<float> m_colors;
        gl::Buffer<u8> m_indices;
    };

    class LightSourceVectors {
    public:
        LightSourceVectors(int res);

        void moveTo(const Vector<float, 3> &position);

        const std::vector<float>& getVertices() const {
            return m_vertices;
        }

        const std::vector<float>& getNormals() const {
            return m_normals;
        }

        const std::vector<float>& getColors() const {
            return m_colors;
        }

        const std::vector<u16>& getIndices() const {
            return m_indices;
        }

        void setColor(float r, float g, float b) {
            for (u32 i = 4; i < m_colors.size(); i += 4) {
                m_colors[i - 4] = r;
                m_colors[i - 3] = g;
                m_colors[i - 2] = b;
                m_colors[i - 1] = 1.0F;
            }
        }

    private:
        int m_resolution;
        float m_radius;

        std::vector<float> m_vertices;
        std::vector<float> m_normals;
        std::vector<float> m_colors;
        std::vector<u16> m_indices;
    };

    class LightSourceBuffers {
    public:
        LightSourceBuffers(const VertexArray &sourceVertexArray, const LightSourceVectors &sourceVectors);

        void moveVertices(const VertexArray &sourceVertexArray, const LightSourceVectors& sourceVectors);
        void updateColors(const VertexArray& sourceVertexArray, const LightSourceVectors& sourceVectors);

        const gl::Buffer<float>& getVertices() const {
            return m_vertices;
        }

        const gl::Buffer<float>& getNormals() const {
            return m_normals;
        }

        const gl::Buffer<float>& getColors() const {
            return m_colors;
        }

        const gl::Buffer<u16>& getIndices() const {
            return m_indices;
        }

    private:
        gl::Buffer<float> m_vertices;
        gl::Buffer<float> m_normals;
        gl::Buffer<float> m_colors;
        gl::Buffer<u16> m_indices;
    };

}