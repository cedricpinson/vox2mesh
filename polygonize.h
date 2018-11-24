#pragma once

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

template <typename T>
struct vec3 {
    T v[3];

    vec3()
        : v{0, 0, 0}
    {}
    vec3(T x, T y, T z)
        : v{x, y, z}
    {}

    inline vec3& operator+=(const vec3& rhs)
    {
        v[0] += rhs.v[0];
        v[1] += rhs.v[1];
        v[2] += rhs.v[2];
        return *this;
    }
    inline T& operator[](int index) { return v[index]; }
    inline const T operator[](int index) const { return v[index]; }
    inline vec3 operator+(const vec3& rhs) const { return vec3(v[0] + rhs.v[0], v[1] + rhs.v[1], v[2] + rhs.v[2]); }
};
typedef vec3<float> fvec3;
typedef vec3<uint8_t> ucvec3;
typedef vec3<int> ivec3;
typedef uint8_t MaterialID;

struct Face {
    int v[4];
    Face(int x, int y, int z, int w)
        : v{x, y, z, w}
    {}
    inline int& operator[](int index) { return v[index]; }
    inline int operator[](int index) const { return v[index]; }

    inline Face& operator+=(const int index)
    {
        v[0] += index;
        v[1] += index;
        v[2] += index;
        v[3] += index;
        return *this;
    }
};

struct VoxelBuffer {
    std::vector<fvec3> vertexes;
    std::vector<fvec3> normals;
    std::vector<Face> faces;
};

typedef std::map<MaterialID, VoxelBuffer> VoxelGroup;

struct VoxScene;

void polygonize(std::vector<VoxelGroup>& groups, const VoxScene& voxScene);
int writeOBJ(const VoxelGroup& group, const char* path);
