#include <getopt.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

void printUsage() {
  const char *text = "vox2obj is a tool to convert vox to obj\n"
                     "usages:\n"
                     " vox2obj [options] input.vox output.vox\n"
                     "\n"
                     "Options:\n"
                     " -d [--disable-optimization] dont clean the faces, write "
                     "6 faces per voxel\n";

  printf("%s", text);
}

struct Options {
  const char *inputFile = 0;
  const char *outputFile = "output.obj";
  int cleanFaces = 1;
};

int parseArgument(Options &options, int argc, char **argv) {
  int opt;
  int optionIndex = 0;

  static const char *OPTSTR = "hd";
  static const struct option OPTIONS[] = {
      {"help", no_argument, nullptr, 'h'},
      {"disable-cleaning", no_argument, nullptr, 'd'},
      {nullptr, 0, 0, 0} // termination of the option list
  };

  while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &optionIndex)) >= 0) {
    const char *arg = optarg ? optarg : "";
    if (opt == -1)
      break;

    switch (opt) {
    default:
    case 'h':
      printUsage();
      exit(0);
      break;
    case 'd':
      options.cleanFaces = 0;
      break;
    }
  }
  return optind;
}

typedef uint8_t MaterialID;
typedef uint8_t VoxelFaceFlags;

template <typename T> struct vec3 {
  T v[3];

  vec3() : v{0, 0, 0} {}
  vec3(T x, T y, T z) : v{x, y, z} {}

  inline vec3 &operator+=(const vec3 &rhs) {
    v[0] += rhs.v[0];
    v[1] += rhs.v[1];
    v[2] += rhs.v[2];
    return *this;
  }
  inline T &operator[](int index) { return v[index]; }
  inline const T operator[](int index) const { return v[index]; }
  inline vec3 operator+(const vec3 &rhs) const {
    return vec3(v[0] + rhs.v[0], v[1] + rhs.v[1], v[2] + rhs.v[2]);
  }
};
typedef vec3<float> fvec3;
typedef vec3<uint8_t> ucvec3;
typedef vec3<int> ivec3;

struct VoxelData {
  uint8_t v[4];
  VoxelData(uint8_t x, uint8_t y, uint8_t z, uint8_t material)
      : v{x, y, z, material} {}
  VoxelData() {}
  inline uint8_t &operator[](int index) { return v[index]; }
  inline const uint8_t operator[](int index) const { return v[index]; }
};

struct cmpVoxel {
  bool operator()(const ucvec3 &a, const ucvec3 &b) const {
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

struct Face {
  int v[4];
  Face(int x, int y, int z, int w) : v{x, y, z, w} {}
  inline int &operator[](int index) { return v[index]; }
  inline const int operator[](int index) const { return v[index]; }

  inline Face &operator+=(const int index) {
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

typedef std::vector<VoxelData> VoxelList;

typedef std::map<MaterialID, VoxelBuffer> VoxelGroup;
struct VoxelScene {
  VoxelGroup voxelGroup;
};

typedef std::map<ucvec3, MaterialID, cmpVoxel> VoxelMap;
typedef std::map<ucvec3, VoxelFaceFlags, cmpVoxel> VoxelMapFlags;

// face 0 x+: 3 2 6 7
// face 1 y+: 1 5 6 2
// face 2 z+: 0 1 2 3
// face 0 x-: 4 5 1 0
// face 1 y-: 4 0 3 7
// face 2 z-: 7 6 5 4
Face FacesVoxel[] = {{7, 6, 2, 3}, {2, 6, 5, 1}, {3, 2, 1, 0},
                     {0, 1, 5, 4}, {7, 3, 0, 4}, {4, 5, 6, 7}};

fvec3 NormalFace[] = {{1, 0, 0},  {0, 1, 0},  {0, 0, 1},
                      {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};

fvec3 VertexesVoxel[] = {
    {-0.5, -0.5, 0.5},  {-0.5, 0.5, 0.5},  {0.5, 0.5, 0.5},  {0.5, -0.5, 0.5},
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
ivec3 VoxelDirection[] = {{1, 0, 0},  {0, 1, 0},  {0, 0, 1},
                          {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};

int readVoxels(VoxelList &voxelList, const char *path) {

  FILE *fp = fopen(path, "rb");

  int nb = 0;
  fread(&nb, 4, 1, fp);
  printf("read %d voxels\n", nb);
  voxelList.reserve(nb);
  for (int i = 0; i < nb; i++) {
    VoxelData v;
    fread(&v, 4, 1, fp);
    voxelList.push_back(v);
  }

  fclose(fp);
  return 0;
}

int writeOBJ(const VoxelGroup &group, const char *path) {
  FILE *fp = fopen(path, "w");

  std::vector<int> vertexesOffset;
  vertexesOffset.push_back(0);
  int totalFaces = 0;

  bool hasNormal = false;

  for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
    const std::vector<fvec3> &vertexes = it->second.vertexes;
    const std::vector<Face> &faces = it->second.faces;
    if (it->second.normals.size())
      hasNormal = true;
    for (int i = 0; i < vertexes.size(); i++) {
      fprintf(fp, "v %f %f %f\n", vertexes[i][0], vertexes[i][1],
              vertexes[i][2]);
    }

    vertexesOffset.push_back(vertexes.size());
    totalFaces += faces.size();
  }

  if (hasNormal) {
    for (VoxelGroup::const_iterator it = group.begin(); it != group.end();
         it++) {
      const std::vector<fvec3> &normals = it->second.normals;
      for (int i = 0; i < normals.size(); i++) {
        fprintf(fp, "vn %f %f %f\n", normals[i][0], normals[i][1],
                normals[i][2]);
      }
    }
  }

  int indexGroup = 0;
  fprintf(fp, "\n//Faces %d\n", totalFaces);
  for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
    int vertexOffset = vertexesOffset[indexGroup] + 1;

    const std::vector<Face> &faces = it->second.faces;
    fprintf(fp, "g material_%d\n", indexGroup);

    if (hasNormal) {
      for (int i = 0; i < faces.size(); i++) {
        int v0 = vertexOffset + faces[i][0];
        int v1 = vertexOffset + faces[i][1];
        int v2 = vertexOffset + faces[i][2];
        int v3 = vertexOffset + faces[i][3];
        fprintf(fp, "f %d//%d %d//%d %d//%d %d//%d\n", v0, v0, v1, v1, v2, v2,
                v3, v3);
      }
    } else {
      for (int i = 0; i < faces.size(); i++) {
        fprintf(fp, "f %d %d %d %d\n", vertexOffset + faces[i][0],
                vertexOffset + faces[i][1], vertexOffset + faces[i][2],
                vertexOffset + faces[i][3]);
      }
    }
    indexGroup++;
  }

  fclose(fp);
  return 0;
}

bool cleanVoxelFace(VoxelMapFlags &voxelMapFlags, VoxelMapFlags::iterator it) {

  VoxelFaceFlags voxelFlag = it->second;
  ucvec3 voxelPosition8 = it->first;

  ivec3 voxelPosition(voxelPosition8[0], voxelPosition8[1], voxelPosition8[2]);

  for (int i = 0; i < 6; i++) {

    ivec3 positionNextVoxel = voxelPosition + VoxelDirection[i];
    int axis = i % 3;
    if (positionNextVoxel[axis] < 0 || positionNextVoxel[axis] > 255)
      continue;

    ucvec3 search((uint8_t)positionNextVoxel[0], (uint8_t)positionNextVoxel[1],
                  (uint8_t)positionNextVoxel[2]);
    auto otherVoxel = voxelMapFlags.find(search);
    if (otherVoxel == voxelMapFlags.end())
      continue;

    uint8_t otherVoxelFlag = otherVoxel->second;
    otherVoxelFlag = otherVoxelFlag & ~(1 << (i + 3));
    if (!otherVoxelFlag) {
      voxelMapFlags.erase(otherVoxel);
    } else {
      otherVoxel->second = otherVoxelFlag;
    }

    voxelFlag = voxelFlag & ~(1 << i);
    if (!voxelFlag) {
      return true;
    }
  }
  return false;
}

void polygonize(VoxelScene &scene, const VoxelList &voxelList) {

  VoxelMap voxelMap;
  VoxelMapFlags voxelMapFlags;
  VoxelGroup &voxelGroup = scene.voxelGroup;

  // fill the map
  for (const VoxelData &voxelData : voxelList) {
    ucvec3 voxelPosition(voxelData[0], voxelData[1], voxelData[2]);
    voxelMapFlags[voxelPosition] = FaceFlag::NONE;
    voxelMap[voxelPosition] = voxelData[3];
  }
  ucvec3 min = voxelMapFlags.begin()->first;
  ucvec3 max = voxelMapFlags.rbegin()->first;
  printf("bbox %d %d %d - %d %d %d\n", (int)min[0], (int)min[1], (int)min[2],
         (int)max[0], (int)max[1], (int)max[2]);

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
  int nbVertexes = 0;
  for (VoxelMapFlags::iterator it = voxelMapFlags.begin();
       it != voxelMapFlags.end(); it++) {
    ucvec3 position = it->first;
    MaterialID materialID = voxelMap[position];
    uint8_t faceFlags = it->second;

    if (!faceFlags)
      continue;

    VoxelBuffer &voxelBuffer = voxelGroup[materialID];
    std::vector<fvec3> &vertexes = voxelBuffer.vertexes;
    std::vector<fvec3> &normals = voxelBuffer.normals;
    std::vector<Face> &faces = voxelBuffer.faces;

    fvec3 voxelPosition((float)position[0], (float)position[1],
                        (float)position[2]);

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

void clean(VoxelScene &scene, const VoxelList &voxelList) {

  VoxelMap voxelMap;
  VoxelMapFlags voxelMapFlags;
  VoxelGroup &voxelGroup = scene.voxelGroup;

  // fill the map
  for (const VoxelData &voxelData : voxelList) {
    ucvec3 voxelPosition(voxelData[0], voxelData[1], voxelData[2]);
    voxelMapFlags[voxelPosition] = FaceFlag::ALL;
    voxelMap[voxelPosition] = voxelData[3];
  }

  // traverse now to remove odd faces
  ucvec3 min = voxelMapFlags.begin()->first;
  ucvec3 max = voxelMapFlags.rbegin()->first;
  printf("bbox %d %d %d - %d %d %d\n", (int)min[0], (int)min[1], (int)min[2],
         (int)max[0], (int)max[1], (int)max[2]);

  for (VoxelMapFlags::iterator it = voxelMapFlags.begin();
       it != voxelMapFlags.end();) {
    if (cleanVoxelFace(voxelMapFlags, it)) {
      it = voxelMapFlags.erase(it);
    } else {
      it++;
    }
  }

  int nbFaces = 0;
  int nbVertexes = 0;
  for (VoxelMapFlags::iterator it = voxelMapFlags.begin();
       it != voxelMapFlags.end(); it++) {
    ucvec3 position = it->first;
    MaterialID materialID = voxelMap[position];
    uint8_t faceFlags = it->second;

    VoxelBuffer &voxelBuffer = voxelGroup[materialID];
    std::vector<fvec3> &vertexes = voxelBuffer.vertexes;
    std::vector<Face> &faces = voxelBuffer.faces;

    fvec3 voxelPosition((float)position[0], (float)position[1],
                        (float)position[2]);

    int vertexBaseIndex = vertexes.size();
    // computes all vertex cube in world space
    // TODO: clean vertexes not used
    for (int j = 0; j < 8; j++) {
      fvec3 v = VertexesVoxel[j];
      v += voxelPosition;
      vertexes.push_back(v);
      nbVertexes++;
    }

    // insert all faces
    for (int f = 0; f < 6; f++) {
      if ((1 << f) & faceFlags) {
        Face face = FacesVoxel[f];
        face += vertexBaseIndex;
        faces.push_back(face);
        nbFaces++;
      }
    }
  }
  printf("Voxels %d - Faces %d - Vertexes %d\n", (int)voxelMapFlags.size(),
         nbFaces, nbVertexes);
}

void createBruteForce(VoxelScene &scene, const VoxelList &voxelList) {
  VoxelGroup &voxelGroup = scene.voxelGroup;
  for (int i = 0; i < voxelList.size(); i++) {
    const VoxelData &voxel = voxelList[i];

    MaterialID materialID = voxel[3];
    VoxelBuffer &voxelBuffer = voxelGroup[materialID];
    std::vector<fvec3> &vertexes = voxelBuffer.vertexes;
    std::vector<Face> &faces = voxelBuffer.faces;

    fvec3 voxelPosition((float)voxel[0], (float)voxel[1], (float)voxel[2]);

    int vertexBaseIndex = vertexes.size();
    // computes all vertex cube in world space
    for (int j = 0; j < 8; j++) {
      fvec3 v = VertexesVoxel[j];
      v += voxelPosition;
      vertexes.push_back(v);
    }

    // insert all faces
    for (int f = 0; f < 6; f++) {
      Face face = FacesVoxel[f];
      face += vertexBaseIndex;
      faces.push_back(face);
    }
  }
}

int main(int argc, char **argv) {

  Options options;
  int optionIndex = parseArgument(options, argc, argv);
  int numArgs = argc - optionIndex;

  if (numArgs < 1) {
    printUsage();
    return 1;
  }

  options.inputFile = argv[optionIndex];
  if (numArgs > 1) {
    options.outputFile = argv[optionIndex + 1];
  }

  VoxelList voxelList;

  if (readVoxels(voxelList, options.inputFile)) {
    printf("error reading voxels\n");
    return 1;
  }

  VoxelScene scene;

  if (options.cleanFaces) {
    // clean(scene, voxelList);
    polygonize(scene, voxelList);
  } else {
    createBruteForce(scene, voxelList);
  }

  // write an obj
  return writeOBJ(scene.voxelGroup, options.outputFile);
}
