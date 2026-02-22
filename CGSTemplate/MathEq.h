#pragma once
#include "Defines.h"
#include "Windows.h"

float degreetoRadians(float degrees);

matrix4x4 matrixMultiplicationMatrix(matrix4x4& m1, matrix4x4& m2) {
    matrix4x4 matrix;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            matrix.m[i][j] = m1.m[i][0] * m2.m[0][j] + m1.m[i][1] * m2.m[1][j] + m1.m[i][2] * m2.m[2][j] + m1.m[i][3] * m2.m[3][j];
        }
    }
    return matrix;
}

vec4 matrixMultiplicationVec(const matrix4x4& mat, const vec4& vec) {
    vec4 result;
    result.x = mat.m[0][0] * vec.x + mat.m[1][0] * vec.y + mat.m[2][0] * vec.z + mat.m[3][0] * vec.w;
    result.y = mat.m[0][1] * vec.x + mat.m[1][1] * vec.y + mat.m[2][1] * vec.z + mat.m[3][1] * vec.w;
    result.z = mat.m[0][2] * vec.x + mat.m[1][2] * vec.y + mat.m[2][2] * vec.z + mat.m[3][2] * vec.w;
    result.w = mat.m[0][3] * vec.x + mat.m[1][3] * vec.y + mat.m[2][3] * vec.z + mat.m[3][3] * vec.w;
    return result;
}

vertex multiplicationVert(matrix3x3 m, vertex& i) {
    vertex v;
    v.pos.x = (i.pos.x * m.axisX.x) + (i.pos.y * m.axisY.x) + (i.pos.z * m.axisZ.x);
    v.pos.y = (i.pos.x * m.axisX.y) + (i.pos.y * m.axisY.y) + (i.pos.z * m.axisZ.y);
    v.pos.z = (i.pos.x * m.axisX.z) + (i.pos.y * m.axisY.z) + (i.pos.z * m.axisZ.z);
    return v;
}

vertex matrixMultiplicationVert(matrix4x4 m, vertex& i) {
    vertex v;
    v.pos.x = (i.pos.x * m.axisX.x) + (i.pos.y * m.axisY.x) + (i.pos.z * m.axisZ.x) + (i.pos.w * m.axisW.x);
    v.pos.y = (i.pos.x * m.axisX.y) + (i.pos.y * m.axisY.y) + (i.pos.z * m.axisZ.y) + (i.pos.w * m.axisW.y);
    v.pos.z = (i.pos.x * m.axisX.z) + (i.pos.y * m.axisY.z) + (i.pos.z * m.axisZ.z) + (i.pos.w * m.axisW.z);
    v.pos.w = (i.pos.x * m.axisX.w) + (i.pos.y * m.axisY.w) + (i.pos.z * m.axisZ.w) + (i.pos.w * m.axisW.w);
    return v;
}

matrix4x4 projectionMatrixMath(float FOV, float aspect, float farplane, float nearplane) {
    double yScale = 1 / tan(degreetoRadians(FOV / 2));
    double xScale = yScale * (aspect);
    matrix4x4 ProjectionMatrix = {
        xScale,0,0,0,
        0,yScale,0,0,
        0,0,(farplane / (farplane - nearplane)),1,
        0,0,-((farplane * nearplane) / (farplane - nearplane)),0
    };
    return ProjectionMatrix;
}

matrix4x4 MatrixIdentity() {
    matrix4x4 m;
    m.axisX.x = 1;
    m.axisX.y = 0;
    m.axisX.z = 0;
    m.axisX.w = 0;
    m.axisY.x = 0;
    m.axisY.y = 1;
    m.axisY.z = 0;
    m.axisY.w = 0;
    m.axisZ.x = 0;
    m.axisZ.y = 0;
    m.axisZ.z = 1;
    m.axisZ.w = 0;
    m.axisW.x = 0;
    m.axisW.y = 0;
    m.axisW.z = 0;
    m.axisW.w = 1;
    return m;
}

matrix4x4 matrixTranslation(vec4 trans) {
    matrix4x4 matrix = MatrixIdentity();
    matrix.axisW = trans;
    return matrix;
}

float degreetoRadians(float degrees) {
    float result = degrees * (3.14159 / 180.0F);
    return result;
}

matrix3x3 matrix3Inverse(const matrix3x3& m) {
    float determ = m.xx * (m.yy * m.zz - m.zy * m.yz)
        - m.xy * (m.yx * m.zz - m.yz * m.zx)
        + m.xz * (m.yx * m.zy - m.yy * m.zx);
    float invDeterm = 1.0f / determ;
    matrix3x3 inv;
    inv.xx = (m.yy * m.zz - m.zy * m.yz) * invDeterm;
    inv.xy = (m.xz * m.zy - m.xy * m.zz) * invDeterm;
    inv.xz = (m.xy * m.yz - m.xz * m.yy) * invDeterm;
    inv.yx = (m.yz * m.zx - m.yx * m.zz) * invDeterm;
    inv.yy = (m.xx * m.zz - m.xz * m.zx) * invDeterm;
    inv.yz = (m.yx * m.xz - m.xx * m.yz) * invDeterm;
    inv.zx = (m.yx * m.zy - m.zx * m.yy) * invDeterm;
    inv.zy = (m.zx * m.xy - m.xx * m.zy) * invDeterm;
    inv.zz = (m.xx * m.yy - m.yx * m.xy) * invDeterm;
    return inv;
}

vec3 matrix3x3MulVec3(const matrix3x3& m, const vec3& v) {
    vec3 ans = { (m.xx * v.x) + (m.yx * v.y) + (m.zx * v.z), (m.xy * v.x) + (m.yy * v.y) + (m.zy * v.z), (m.xz * v.x) + (m.yz * v.y) + (m.zz * v.z) };
    return ans;
}

matrix4x4 matrix4Inverse(matrix4x4 trix) {
    matrix4x4 result = { 0 };
    matrix3x3 trans = { trix.xx,trix.yx,trix.zx,trix.xy,trix.yy,trix.zy,trix.xz,trix.yz,trix.zz };
    vec3 posi = { trix.wx,trix.wy,trix.wz };
    vec3 invPosi = matrix3x3MulVec3(trans, posi);
    invPosi.x = -invPosi.x;
    invPosi.y = -invPosi.y;
    invPosi.z = -invPosi.z;
    result.xx = trans.xx;
    result.yx = trans.yx;
    result.zx = trans.zx;
    result.wx = 0;
    result.xy = trans.xy;
    result.yy = trans.yy;
    result.zy = trans.zy;
    result.wy = 0;
    result.xz = trans.xz;
    result.yz = trans.yz;
    result.zz = trans.zz;
    result.wz = 0;
    result.wx = invPosi.x;
    result.wy = invPosi.y;
    result.wz = invPosi.z;
    result.ww = 1;
    return result;
}

matrix4x4 matrixRotationX(float rotX) {
    float radianX = degreetoRadians(rotX);
    matrix4x4 X = {
        1,       0,                0,                     0,
        0,       cos(radianX),     sin(radianX) * -1 ,    0,
        0,       sin(radianX),     cos(radianX),          0,
        0,       0,                0,                     1
    };
    return X;
}

matrix4x4 matrixRotationY(matrix4x4 m, float rotY) {
    float radianY = degreetoRadians(rotY);
    matrix4x4 Y = {
        cos(radianY),  0, sin(radianY), 0,
        0,       1, 0,      0,
        -sin(radianY), 0, cos(radianY), 0,
        0,       0, 0,      1
    };
    return matrixMultiplicationMatrix(m, Y);
}

float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}
