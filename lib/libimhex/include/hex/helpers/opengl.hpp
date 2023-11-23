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
                this->m_data[i] = val;
        }

        Vector(std::array<T, Size> data) : m_data(data) { }
        Vector(Vector &&other) noexcept : m_data(std::move(other.m_data)) { }
        Vector(const Vector &other) : m_data(other.m_data) { }

        T &operator[](size_t index) { return this->m_data[index]; }
        const T &operator[](size_t index) const { return this->m_data[index]; }

        std::array<T, Size> &asArray()  { return this->m_data; }

        T *data() { return this->m_data.data(); }
        const T *data() const { return this->m_data.data(); }

        [[nodiscard]] size_t size() const { return this->m_data.size(); }

        auto operator=(const Vector& other) {
            for (size_t i = 0; i < Size; i++)
                this->m_data[i] = other[i];
            return *this;
        }

        auto operator+=(const Vector& other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] += other[i];
            return copy;
        }

        auto operator+=(const T scalar) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
        copy[i] += scalar;
        return copy;
    }

        auto operator-=(Vector other) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] -= other[i];
            return copy;
        }


        auto operator-=(const T scalar) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i] -= scalar;
            return copy;
        }

        Vector operator*=(const T scalar) {
            auto copy = *this;
            for (size_t i = 0; i < Size; i++)
                copy[i]  = scalar * copy[i];
            return copy;
        }

        auto dot(const Vector& other) {
            T result = 0;
            for (size_t i = 0; i < Size; i++)
                result += this->m_data[i] * other[i];
            return result;
        }

        auto cross(const Vector& other) {
            static_assert(Size == 3, "Cross product is only defined for 3D vectors");
            return Vector({this->m_data[1] * other[2] - this->m_data[2] * other[1],
                           this->m_data[2] * other[0] - this->m_data[0] * other[2],
                           this->m_data[0] * other[1] - this->m_data[1] * other[0]});
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
                if (this->m_data[i] != other[i])
                    return false;
            return true;
        }

    private:
        std::array<T, Size> m_data;
    };

    template<typename T, size_t Size>
    auto operator*(const Vector<T, Size>& lhs, const T scalar) {
        auto copy = lhs;
        for (size_t i = 0; i < Size; i++)
            copy[i] *= scalar;
        return copy;
    }

    template<typename T, size_t Size>
    auto operator*(const T scalar, const Vector<T, Size>& rhs) {
        auto copy = rhs;
        for (size_t i = 0; i < Size; i++)
            copy[i] *= scalar;
        return copy;
    }

    template<typename T, size_t Size>
    auto operator+(const Vector<T, Size>& lhs, const T scalar) {
        auto copy = lhs;
        for (size_t i = 0; i < Size; i++)
            copy[i] += scalar;
        return copy;
    }

    template<typename T, size_t Size>
    auto dot(const Vector<T, Size>& lhs, const Vector<T, Size>& rhs) {
        auto copy = lhs;
        return copy.dot(rhs);
    }

    template<typename T, size_t Size>
    auto operator==(const Vector<T, Size>& lhs, const Vector<T, Size>& rhs) {
        auto copy = lhs;
        return copy == rhs;
    }

    template<typename T, size_t Size>
    auto operator+(const Vector<T, Size>& lhs, const Vector<T, Size>& rhs) {
        auto copy = lhs;
        for (size_t i = 0; i < Size; i++)
            copy[i] += rhs[i];
        return copy;
    }

     template<typename T, size_t Size>
    auto operator-(const Vector<T, Size>& lhs, const Vector<T, Size>& rhs) {
        auto copy = lhs;
        for (size_t i = 0; i < Size; i++)
            copy[i] -= rhs[i];
        return copy;
    }

    template<typename T>
    auto cross(const Vector<T, 3>& lhs, const Vector<T, 3>& rhs) {
        auto copy = lhs;
        return copy.cross(rhs);
    }

    template<typename T, size_t ROWS, size_t COLS>
    class Matrix {
    public:
        Matrix(const T &init) {
            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    mat[i*COLS+j] = init;
        }

        Matrix(const Matrix &A) {
            mat = A.mat;
        }

        virtual ~Matrix() {}

        size_t getRows() const {
            return ROWS;
        }

        size_t getColumns() const {
            return COLS;
        }

        T *data() { return this->mat.data(); }

        const T *data() const { return this->mat.data(); }


        T &getElement(int row,int col) {
            return this->mat[row*COLS+col];
        }

        Vector<T,ROWS> getColumn(int col) {
            Vector<T,ROWS> result;
            for (size_t i = 0; i < ROWS; i++)
                result[i] = this->mat[i*COLS+col];
            return result;
        }

        Vector<T,COLS> getRow(int row) {
            Vector<T,COLS> result;
            for (size_t i = 0; i < COLS; i++)
                result[i] = this->mat[row*COLS+i];
            return result;
        }

        void updateRow(int row, Vector<T,COLS> values) {
            for (size_t i = 0; i < COLS; i++)
                this->mat[row*COLS+i] = values[i];
        }

        void updateColumn(int col, Vector<T,ROWS> values) {
            for (size_t i = 0; i < ROWS; i++)
                this->mat[i*COLS+col] = values[i];
        }

        void updateElement( int row,int col, T value) {
            this->mat[row*COLS + col] = value;
        }

        T &operator()( const int &row,const int &col) {
            return this->mat[row*COLS + col];
        }

        const T &operator()(const unsigned& row,const unsigned& col ) const {
            return this->mat[row*COLS + col];
        }

        Matrix& operator=(const Matrix& A) {
            if (&A == this)
                return *this;

            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    mat[i*COLS+j] = A(i, j);
            return *this;
        }

        Matrix operator+(const Matrix& A) {
            Matrix result(0.0);

            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    result(i, j) = this->mat[i*COLS+j] + A(i, j);
            return result;
        }

        Matrix operator-(const Matrix& A) {
            Matrix result(0.0);

            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    result(i, j) = this->mat[i*COLS+j] - A(i, j);
            return result;
        }

        void getCoFactors(std::array<T, ROWS*COLS> array, std::array<T, ROWS*COLS> &temp, int size, int c, int r) {
            int current_row = 0;
            int current_col = 0;

            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    if (i != r && j != c) {
                        temp[ current_row*COLS+current_col] = array[i*COLS+j];
                        current_col++;
                        if (current_col == size - 1) {
                            current_col = 0;
                            current_row++;
                        }
                    }
                }
            }
        }

        T iterateDet(std::array<T, ROWS* COLS> array, int size)
        {
            T determinant = 0;
            if(size == 2)
                return array[0]*array[3] - array[1]*array[2];
            else if(size == 3)
                return array[0]*array[4]*array[8] + array[1]*array[5]*array[6] + array[2]*array[3]*array[7]
                       - array[2]*array[4]*array[6] - array[1]*array[3]*array[8] - array[0]*array[5]*array[7];
            else if(size == 4)
                return array[0]*array[5]*array[10]*array[15] + array[1]*array[6]*array[11]*array[12] + array[2]*array[7]*array[8]*array[13] + array[3]*array[4]*array[9]*array[14]
                       - array[3]*array[6]*array[9]*array[12] - array[2]*array[5]*array[8]*array[15] - array[1]*array[4]*array[11]*array[14] - array[0]*array[7]*array[10]*array[13]
                       - array[0]*array[6]*array[11]*array[13] - array[1]*array[7]*array[8]*array[14] - array[2]*array[4]*array[10]*array[15] - array[3]*array[5]*array[9]*array[12];
            else if(size == 5)
                return array[0]*array[6]*array[12]*array[18]*array[24] + array[1]*array[7]*array[13]*array[19]*array[20] + array[2]*array[8]*array[14]*array[15]*array[21] + array[3]*array[9]*array[10]*array[16]*array[22] + array[4]*array[5]*array[11]*array[17]*array[23]
                       - array[4]*array[7]*array[10]*array[19]*array[21] - array[3]*array[6]*array[11]*array[18]*array[22] - array[2]*array[5]*array[14]*array[17]*array[24] - array[1]*array[9]*array[12]*array[16]*array[23] - array[0]*array[8]*array[13]*array[15]*array[22]
                       - array[0]*array[7]*array[14]*array[16]*array[20] - array[1]*array[5]*array[12]*array[19]*array[24] - array[2]*array[6]*array[13]*array[17]*array[20] - array[3]*array[8]*array[11]*array[15]*array[23] - array[4]*array[9]*array[10]*array[18]*array[21]
                       - array[4]*array[6]*array[13]*array[16]*array[23] - array[3]*array[5]*array[14]*array[18]*array[20] - array[2]*array[9]*array[10]*array[17]*array[21] - array[1]*array[8]*array[11]*array[19]*array[22] - array[0]*array[5]*array[11]*array[18]*array[24]
                       - array[0]*array[6]*array[9]*array[17]*array[22] - array[1]*array[7]*array[10]*array[15]*array[24] - array[2]*array[8]*array[12]*array[16]*array[21] - array[3]*array[4]*array[13]*array[17]*array[22] - array[4]*array[5]*array[12]*array[15]*array[23]
                       - array[4]*array[7]*array[9]*array[13]*array[20] - array[3]*array[8]*array[10]*array[14]*array[23] - array[2]*array[4]*array[11]*array[19]*array[23] - array[1]*array[5]*array[9]*array[18]*array[22] - array[0]*array[6]*array[10]*array[17]*array[21]
                       - array[0]*array[7]*array[11]*array[15]*array[22] - array[1]*array[4]*array[12]*array[16]*array[20] - array[2]*array[5]*array[13]*array[14]*array[21] - array[3]*array[6]*array[8]*array[15]*array[20] - array[4]*array[7]*array[9]*array[11]*array[18]
                          - array[4]*array[5]*array[10]*array[12]*array[19] - array[3]*array[7]*array[8]*array[12]*array[17] - array[2]*array[6]*array[9]*array[13]*array[18] - array[1]*array[5]*array[8]*array[14]*array[19] - array[0]*array[6]*array[9]*array[11]*array[17]
                            - array[0]*array[5]*array[7]*array[13]*array[18] - array[1]*array[6]*array[8]*array[12]*array[19] - array[2]*array[4]*array[9]*array[11]*array[19] - array[3]*array[5]*array[7]*array[12]*array[18] - array[4]*array[6]*array[8]*array[10]*array[17]
                                 - array[4]*array[5]*array[9]*array[11]*array[16] - array[3]*array[6]*array[7]*array[9]*array[16] - array[2]*array[5]*array[8]*array[10]*array[17] - array[1]*array[4]*array[7]*array[11]*array[17] - array[0]*array[5]*array[8]*array[9]*array[17]
                                     - array[0]*array[4]*array[6]*array[11]*array[18] - array[1]*array[5]*array[6]*array[8]*array[19] - array[2]*array[6]*array[7]*array[9]*array[15] - array[3]*array[7]*array[8]*array[10]*array[16] - array[4]*array[8]*array[9]*array[11]*array[12]
                                         - array[4]*array[6]*array[10]*array[8]*array[13] - array[3]*array[7]*array[9]*array[8]*array[14] - array[2]*array[5]*array[11]*array[9]*array[14] - array[1]*array[6]*array[10]*array[9]*array[15] - array[0]*array[7]*array[11]*array[8]*array[16]
                                         - array[0]*array[6]*array[9]*array[10]*array[17] - array[1]*array[7]*array[10]*array[8]*array[18] - array[2]*array[8]*array[11]*array[9]*array[12] - array[3]*array[9]*array[10]*array[8]*array[13] - array[4]*array[10]*array[11]*array[9]*array[14]
                                             - array[4]*array[9]*array[10]*array[11]*array[8];



            std::array<T, ROWS* COLS> temp;
            T sign = 1;

            for(int h=0; h<size; h++)
            {
                this->getCoFactors(array, temp, size, 0, h);
                determinant += sign * array[h] * this->iterateDet(temp, size-1);
                sign = -sign;
            }
            return determinant;
        }

        T getDeterminant(int size)
        {
            if(ROWS != COLS)
            {
                static_assert(ROWS==COLS, "ERROR: Matrix is not square");
                return 0;
            }
            std::array<T, ROWS* COLS> array;

            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    array[i*COLS+j] = this->mat[i*COLS+j];

            T det = this->iterateDet(array, size);
            return det;
        }

        Matrix  scalarTimesMatrix(T scalar)
        {
            Matrix  result(0);
            for (size_t i = 0; i < ROWS; i++)
            {
                for (size_t j = 0; j < COLS; j++)
                {
                    T newVal = scalar * this->mat[i*COLS+j];
                    result.updateElement(i, j, newVal);
                }
            }
            return result;
        }


        Matrix  scalarPlusMatrix(T scalar)
        {
            Matrix  result(0);
            for (size_t i = 0; i < ROWS; i++)
            {
                for (size_t j = 0; j < COLS; j++)
                {
                    T newVal = scalar + this->mat[i*COLS+j];
                    result.updateElement(i, j, newVal);
                }
            }
            return result;
        }


        Matrix  scalarMinusMatrix(T scalar)
        {
            Matrix  result(0);
            for (size_t i = 0; i < ROWS; i++)
            {
                for (size_t j = 0; j < COLS; j++)
                {
                    T newVal = this->mat[i*COLS+j] - scalar;
                    result.updateElement(i, j, newVal);
                }
            }
            return result;
        }

        Vector<T,ROWS> diagonalVector()
        {
            Vector<T,ROWS> d(0);
            if(ROWS != COLS)
            {
                static_assert(ROWS==COLS,"ERROR: Matrix is not square!");
                return d;
            }
            for (size_t i = 0; i < ROWS; i++)
            {
                for (size_t j = 0; j < COLS; j++)
                {
                    if(i == j)
                    {
                        T val = this->getElement(i, j);
                        d.push_back(val);
                    }
                }
            }
            return d;
        }

        static Matrix  identity()
        {
            Matrix  I( 0);
            for (size_t i = 0; i < ROWS; i++)
                for (size_t j = 0; j < COLS; j++)
                    if(i == j)
                        I.updateElement(i, j, 1);
            return I;
        }

        Matrix  transpose()
        {
            Matrix  t(0);
            for (size_t i = 0; i < COLS; i++)
                for (size_t j = 0; j < ROWS; j++)
                    t.updateElement(i, j, this->mat[j*ROWS+i]);

            return t;
        }

        Matrix  adjoint()
        {
            Matrix  adjnt(0);
            int size = COLS;
            T sign = 1;

            if(ROWS != COLS)
            {
                static_assert( ROWS==COLS,"ERROR: Matrix is not square!");
                return adjnt;
            }

            std::array<T, ROWS* COLS> array;
            for(size_t g=0; g<ROWS; g++)
                for(size_t j=0; j<COLS; j++)
                    array[g*COLS+j] = this->mat[g*COLS+j];
            std::array<T, ROWS* COLS> temp;

            for(size_t h=0; h<ROWS; h++)
            {
                for(size_t k=0; k<COLS; k++)
                {
                    this->getCoFactors(array, temp, size, h, k);

                    sign = ((h+k)%2==0)? 1: -1;

                    T partial_det = sign * this->iterateDet(temp, size-1);
                    adjnt.updateElement(k, h, partial_det);
                }
            }
            return adjnt;
        }

        Matrix  inverse()
        {
            Matrix inv(0);
            int size = ROWS;

            if(ROWS != COLS)
            {
                static_assert(ROWS == COLS ,"ERROR: Matrix is not square!");
                return inv;
            }

            T det = this->getDeterminant(size);

            if(det == 0)
            {
                printf("ERROR: Singular matrix, not possible to compute its determinant");
                return inv;
            }

            Matrix adj(0);
            adj = this->adjoint();

            for (size_t i = 0; i < ROWS; i++)
            {
                for (size_t j = 0; j < COLS; j++)
                {
                    T adj_val = adj.getElement(i, j);
                    T newVal = adj_val/det;
                    inv.updateElement(i, j, newVal);
                }
            }
            return inv;
        }

    private:
        std::array<T,ROWS*COLS> mat;
    };

    template<typename T, size_t ROWS, size_t N, size_t COLS>
    Matrix<T,ROWS, COLS >  operator*(const Matrix<T, ROWS,N > &A, const Matrix<T, N,COLS > &B ) {
        Matrix<T,ROWS, COLS >  result(0.0);
        for (size_t i = 0; i < ROWS; i++)
            for (size_t j = 0; j < COLS; j++)
                for (size_t k = 0; k < N; k++)
                    result(i, j) += A(i,k) * B(k, j);
        return result;
    }

    template<typename T, size_t ROWS, size_t COLS>
    Matrix<T,ROWS, COLS >  operator*(const Vector<T, ROWS> &a,const  Vector<T, COLS> &b) {
        Matrix<T,ROWS, COLS >  result(0);
        for (size_t i = 0; i < ROWS; i++)
            for (size_t j = 0; j < COLS; j++)
                result.updateElement(i, j, a[i] * b[j]);
        return result;
    }

    template<typename T, size_t ROWS, size_t COLS>
    Vector<T, ROWS> operator*(const Matrix<T,ROWS,COLS> &A, const Vector<T, COLS> &b) {
        Vector<T, ROWS> result(0);
        for (size_t i = 0; i < ROWS; i++)
            for (size_t j = 0; j < COLS; j++)
                result[i] += A(i,j) * b[j];
        return result;
    }

    template<typename T, size_t ROWS, size_t COLS>
    Vector<T, COLS> operator*( const Vector<T, ROWS> &b, const Matrix<T,ROWS,COLS> &A) {
        Vector<T, COLS> result(0);
        for (size_t i = 0; i < ROWS; i++)
            for (size_t j = 0; j < COLS; j++)
                result[j] += b[i]*A(i,j);
        return result;
    }

    Matrix<float,4, 4 > lookAt( const Vector<float, 3> &cameraPosition, const Vector<float, 3> &target,
                                  const Vector<float, 3> &up) {
        Vector<float, 3> zAxis3 = (cameraPosition-target);
        zAxis3.normalize();
        Vector<float, 3> xAxis3 = cross(up, zAxis3).normalize();
        Vector<float, 3> yAxis3 = cross(zAxis3, xAxis3).normalize();
        Vector<float, 4> zAxis(0);
        Vector<float, 4> xAxis(0);
        Vector<float, 4> yAxis(0);
        Vector<float, 4> cameraPosition4(0);
        for (size_t i = 0; i < 3; i++)
        {
            zAxis[i] = zAxis3[i];
            xAxis[i] = xAxis3[i];
            yAxis[i] = yAxis3[i];
            cameraPosition4[i] = cameraPosition[i];
        }

        Matrix<float,4, 4 > result(0);

        result.updateColumn(0, xAxis);
        result.updateColumn(1, yAxis);
        result.updateColumn(2, zAxis);
        result.updateColumn(3, cameraPosition4);


        return result.inverse();
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
    Matrix<T, 4,4 > getRotationMatrix(Vector<T,3> ypr, bool radians, RotationSequence rotationSequence)
    {
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
    Matrix<T, 4,4 > getRotationMatrixFromVectorAngle(Vector<T,4> rotationVector, bool radians) {
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
    T findValue(Vector<T,3> ypr, MatrixElements matrixElement, RotationSequence rotationSequence)
    {
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
    Matrix<T, 4,4 > getTransformMatrix(Vector<T,3> xyz, Vector<T,3> ypr, bool radians)
    {
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
    Vector<T,3> getTranslationVector(Matrix<T, 4,4 > transform_matrix)
    {
        Vector<T,3> xyz;

        xyz.push_back(transform_matrix.getElement(0,3));
        xyz.push_back(transform_matrix.getElement(1,3));
        xyz.push_back(transform_matrix.getElement(2,3));

        return xyz;
    }

    template<typename T>
    Vector<T,3> getYprVector(Matrix<T, 4,4 > transform_matrix)
    {
        Vector<T,3> result;

        Matrix<T,3,3> rotation(0);
        for(int i=0; i<3; i++)
            for(int j=0; j<3; j++)
                rotation.updateElement(i, j, transform_matrix.getElement(i, j));

        T sy = sqrt(rotation.getElement(0,0) * rotation.getElement(0,0) +  rotation.getElement(1,0) * rotation.getElement(1,0) );

        bool singular = sy < 1e-6;

        T x, y, z;
        if (!singular)
        {
            x =  atan2(rotation.getElement(1,0), rotation.getElement(0,0));
            y = atan2(-rotation.getElement(2,0), sy);
            z = atan2(rotation.getElement(2,1), rotation.getElement(2,2));
        }
        else
        {
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
    static Matrix<T,4,4> GetObliqueMatrix(	T width, T height,T nearVal,T farVal, bool actionType = false)
    {
        int sign =1;
        if (actionType)
            sign=-1;
        Matrix<T,4,4> result(0);
        result.updateElement(0,0,sign * nearVal/width);
        //result.updateElement(0,2,sign * (right+left)/(right - left));

        result.updateElement(1,1, sign *  nearVal/height);
       // result.updateElement(1,2,sign * (top+bottom)/(top - bottom));

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
            if(N==2)
                glUniform2f(getUniformLocation(name), value[0], value[1]);
            else if(N==3)
                glUniform3f(getUniformLocation(name), value[0], value[1], value[2]);
            else if(N==4)
                glUniform4f(getUniformLocation(name), value[0], value[1], value[2],value[3]);
        }

        template<size_t N>
        void setUniform(std::string_view name, Matrix<float, N,N> &value){
            glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, (GLfloat *)(value.data()));
        }

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

        void draw(unsigned primitive) const;
        size_t getSize() const;
        void update(std::span<T> data);
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

    class AxesVectors {
    public:
        AxesVectors();
        std::vector<float>& getVertices() {
            return vertices;
        }
        std::vector<float>& getColors() {
            return colors;
        }
        std::vector<u8>& getIndices() {
            return indices;
        }
    private:
        std::vector<float> vertices;
        std::vector<float> colors;
        std::vector<u8> indices;

    };

    class AxesBuffers {
    public:
        AxesBuffers(VertexArray& axesVertexArray, AxesVectors axesVectors);
        gl::Buffer<float>& getVertices() {
            return vertices;
        }
        gl::Buffer<float>& getColors() {
            return colors;
        }
        gl::Buffer<u8>& getIndices() {
            return indices;
        }
    private:
        gl::Buffer<float> vertices;
        gl::Buffer<float> colors;
        gl::Buffer<u8> indices;
    };

    class GridVectors {
    public:
        GridVectors(int sliceCount);

        int getSlices() {
            return slices;
        }

        std::vector<float>& getVertices() {
            return vertices;
        }

        std::vector<float>& getColors() {
            return colors;
        }

        std::vector<u8>& getIndices() {
            return indices;
        }

    private:
        int slices;
        std::vector<float> vertices;
        std::vector<float> colors;
        std::vector<u8> indices;
    };

    class GridBuffers {
    public:
        GridBuffers(VertexArray &gridVertexArray, GridVectors gridVectors);
        gl::Buffer<float>& getVertices() {
            return vertices;
        }
        gl::Buffer<float>& getColors() {
            return colors;
        }
        gl::Buffer<u8>& getIndices() {
            return indices;
        }
    private:
        gl::Buffer<float> vertices;
        gl::Buffer<float> colors;
        gl::Buffer<u8> indices;
    };

    class LightSourceVectors {
    public:
        LightSourceVectors(int res);
        void moveTo(Vector<float, 3> position);
        int resolution;
        float radius;
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> colors;
        std::vector<u8> indices;
    };

    class LightSourceBuffers {
    public:
        LightSourceBuffers(VertexArray &sourceVertexArray, LightSourceVectors sourceVectors);
        void moveVertices(VertexArray &sourceVertexArray, LightSourceVectors& sourceVectors);
        gl::Buffer<float> vertices;
        //gl::Buffer<float> normals;
        gl::Buffer<float> colors;
        gl::Buffer<u8> indices;
    };

}