#include "polygonize.h"
#include "VoxReader.h"

typedef uint8_t VoxelFaceFlags;

struct VoxelData {
    uint8_t v[4];
    VoxelData(uint8_t x, uint8_t y, uint8_t z, uint8_t material)
        : v{x, y, z, material}
    {}
    VoxelData() {}
    inline uint8_t& operator[](int index) { return v[index]; }
    inline uint8_t operator[](int index) const { return v[index]; }
};

struct cmpVoxel {
    bool operator()(const ucvec3& a, const ucvec3& b) const
    {
        if (a[0] < b[0]) {
            return true;
        } else if (a[0] == b[0]) {
            if (a[1] < b[1]) {
                return true;
            } else if (a[1] == b[1]) {
                if (a[2] < b[2])
                    return true;
            }
        }
        return false;
    }
};
typedef std::map<ucvec3, MaterialID, cmpVoxel> VoxelMap;
typedef std::map<ucvec3, VoxelFaceFlags, cmpVoxel> VoxelMapFlags;

// face 0 x+: 3 2 6 7
// face 1 y+: 1 5 6 2
// face 2 z+: 0 1 2 3
// face 0 x-: 4 5 1 0
// face 1 y-: 4 0 3 7
// face 2 z-: 7 6 5 4
Face FacesVoxel[] = {{7, 6, 2, 3}, {2, 6, 5, 1}, {3, 2, 1, 0}, {0, 1, 5, 4}, {7, 3, 0, 4}, {4, 5, 6, 7}};

fvec3 NormalFace[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};

fvec3 VertexesVoxel[] = {{-0.5, -0.5, 0.5},  {-0.5, 0.5, 0.5},  {0.5, 0.5, 0.5},  {0.5, -0.5, 0.5},
                         {-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, -0.5, -0.5}};

enum FaceFlag : uint8_t {
    NONE = 0,
    PX = 1 << 0,
    PY = 1 << 1,
    PZ = 1 << 2,
    NX = 1 << 3,
    NY = 1 << 4,
    NZ = 1 << 5,
    ALL = PX | PY | PZ | NX | NY | NZ
};
ivec3 VoxelDirection[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};

void polygonize(VoxelGroup& voxelGroup, const VoxModel& voxModel)
{

    VoxelMap voxelMap;
    VoxelMapFlags voxelMapFlags;

    ucvec3 min(255, 255, 255);
    ucvec3 max(0, 0, 0);
    // fill the map
    for (const VoxelPos& voxelData : voxModel) {
        min[0] = voxelData[0] < min[0] ? voxelData[0] : min[0];
        min[1] = voxelData[1] < min[1] ? voxelData[1] : min[1];
        min[2] = voxelData[2] < min[2] ? voxelData[2] : min[2];

        max[0] = voxelData[0] > max[0] ? voxelData[0] : max[0];
        max[1] = voxelData[1] > max[1] ? voxelData[1] : max[1];
        max[2] = voxelData[2] > max[2] ? voxelData[2] : max[2];

        ucvec3 voxelPosition(voxelData[0], voxelData[1], voxelData[2]);

        voxelMapFlags[voxelPosition] = FaceFlag::NONE;
        voxelMap[voxelPosition] = voxelData[3];
    }
    printf("bbox %d %d %d - %d %d %d\n", (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);

    for (int x = min[0]; x <= max[0]; x++)
        for (int y = min[1]; y <= max[1]; y++)
            for (int z = min[2]; z <= max[2]; z++) {
                ucvec3 voxelPosition(x, y, z);
                VoxelMapFlags::iterator voxel = voxelMapFlags.find(voxelPosition);
                if (voxel == voxelMapFlags.end())
                    continue;

                // check +x -x
                ucvec3 nextVoxel;
                for (int axis = 0; axis < 3; axis++) {
                    nextVoxel = voxelPosition;

                    if (voxelPosition[axis] == min[axis]) {
                        voxel->second = voxel->second | (1 << (axis + 3));
                    } else {
                        nextVoxel[axis] = voxelPosition[axis] - 1;
                        if (voxelMapFlags.find(nextVoxel) == voxelMapFlags.end()) {
                            voxel->second = voxel->second | (1 << (axis + 3));
                        }
                    }

                    if (voxelPosition[axis] == max[axis]) {
                        voxel->second = voxel->second | (1 << axis);
                    } else {
                        nextVoxel[axis] = voxelPosition[axis] + 1;
                        if (voxelMapFlags.find(nextVoxel) == voxelMapFlags.end()) {
                            voxel->second = voxel->second | (1 << axis);
                        }
                    }
                }
            }

    int nbFaces = 0;
    for (VoxelMapFlags::iterator it = voxelMapFlags.begin(); it != voxelMapFlags.end(); it++) {
        ucvec3 position = it->first;
        MaterialID materialID = voxelMap[position];
        uint8_t faceFlags = it->second;

        if (!faceFlags)
            continue;

        VoxelBuffer& voxelBuffer = voxelGroup[materialID];
        std::vector<fvec3>& vertexes = voxelBuffer.vertexes;
        std::vector<fvec3>& normals = voxelBuffer.normals;
        std::vector<Face>& faces = voxelBuffer.faces;

        fvec3 voxelPosition((float)position[0], (float)position[1], (float)position[2]);

        // insert all faces
        for (int f = 0; f < 6; f++) {
            if ((1 << f) & faceFlags) {
                Face face = FacesVoxel[f];

                int vertexBaseIndex = vertexes.size();
                fvec3 normal = NormalFace[f];

                for (int j = 0; j < 4; j++) {
                    fvec3 v = VertexesVoxel[face[j]];
                    v += voxelPosition;
                    vertexes.push_back(v);
                    normals.push_back(normal);
                }

                Face f(0, 1, 2, 3);
                f += vertexBaseIndex;
                faces.push_back(f);
                nbFaces++;
            }
        }
    }
    printf("Faces %d - Vertexes %d\n", nbFaces, nbFaces * 4);
}

void polygonize(std::vector<VoxelGroup>& groups, const VoxScene& voxScene)
{
    for (const VoxModel& voxelModel : voxScene.voxels) {
        VoxelGroup group;
        polygonize(group, voxelModel);
        groups.push_back(group);
    }
}

// void createBruteForce(VoxelScene &scene, const VoxelList &voxelList) {
//   VoxelGroup &voxelGroup = scene.voxelGroup;
//   for (int i = 0; i < voxelList.size(); i++) {
//     const VoxelData &voxel = voxelList[i];

//     MaterialID materialID = voxel[3];
//     VoxelBuffer &voxelBuffer = voxelGroup[materialID];
//     std::vector<fvec3> &vertexes = voxelBuffer.vertexes;
//     std::vector<Face> &faces = voxelBuffer.faces;

//     fvec3 voxelPosition((float)voxel[0], (float)voxel[1], (float)voxel[2]);

//     int vertexBaseIndex = vertexes.size();
//     // computes all vertex cube in world space
//     for (int j = 0; j < 8; j++) {
//       fvec3 v = VertexesVoxel[j];
//       v += voxelPosition;
//       vertexes.push_back(v);
//     }

//     // insert all faces
//     for (int f = 0; f < 6; f++) {
//       Face face = FacesVoxel[f];
//       face += vertexBaseIndex;
//       faces.push_back(face);
//     }
//   }
// }

int writeOBJ(const VoxelGroup& group, const char* path)
{
    FILE* fp = fopen(path, "w");

    std::vector<int> vertexesOffset;
    vertexesOffset.push_back(0);
    int totalFaces = 0;

    bool hasNormal = false;

    for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
        const std::vector<fvec3>& vertexes = it->second.vertexes;
        const std::vector<Face>& faces = it->second.faces;
        if (it->second.normals.size())
            hasNormal = true;
        for (unsigned int i = 0; i < vertexes.size(); i++) {
            fprintf(fp, "v %f %f %f\n", vertexes[i][0], vertexes[i][1], vertexes[i][2]);
        }

        vertexesOffset.push_back(vertexes.size());
        totalFaces += faces.size();
    }

    if (hasNormal) {
        for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
            const std::vector<fvec3>& normals = it->second.normals;
            for (unsigned int i = 0; i < normals.size(); i++) {
                fprintf(fp, "vn %f %f %f\n", normals[i][0], normals[i][1], normals[i][2]);
            }
        }
    }

    int indexGroup = 0;
    fprintf(fp, "\n//Faces %d\n", totalFaces);
    for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
        int vertexOffset = vertexesOffset[indexGroup] + 1;

        const std::vector<Face>& faces = it->second.faces;
        fprintf(fp, "g material_%d\n", indexGroup);

        if (hasNormal) {
            for (unsigned int i = 0; i < faces.size(); i++) {
                int v0 = vertexOffset + faces[i][0];
                int v1 = vertexOffset + faces[i][1];
                int v2 = vertexOffset + faces[i][2];
                int v3 = vertexOffset + faces[i][3];
                fprintf(fp, "f %d//%d %d//%d %d//%d %d//%d\n", v0, v0, v1, v1, v2, v2, v3, v3);
            }
        } else {
            for (unsigned int i = 0; i < faces.size(); i++) {
                fprintf(fp, "f %d %d %d %d\n", vertexOffset + faces[i][0], vertexOffset + faces[i][1],
                        vertexOffset + faces[i][2], vertexOffset + faces[i][3]);
            }
        }
        indexGroup++;
    }

    fclose(fp);
    return 0;
}
