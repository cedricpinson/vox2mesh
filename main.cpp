#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

typedef unsigned char MaterialID;

struct VoxelData {
  unsigned char v[4];
  inline unsigned char &operator[](int index) { return v[index]; }
  inline const unsigned char operator[](int index) const { return v[index]; }
};

struct cmpVoxel {
  bool operator()(const VoxelData &a, const VoxelData &b) const {
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

struct fvec3 {
  float v[3];

  fvec3(float x, float y, float z) : v{x, y, z} {}
  /** Unary vector add. Slightly more efficient because no temporary
   * intermediate object.
   */
  inline fvec3 &operator+=(const fvec3 &rhs) {
    v[0] += rhs.v[0];
    v[1] += rhs.v[1];
    v[2] += rhs.v[2];
    return *this;
  }
  inline float &operator[](int index) { return v[index]; }
  inline const float operator[](int index) const { return v[index]; }
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
  std::vector<Face> faces;
};

typedef std::map<MaterialID, VoxelBuffer> VoxelGroup;
struct VoxelScene {
  VoxelGroup voxelGroup;
};

// face 0 x+: 3 2 6 7
// face 1 y+: 1 5 6 2
// face 2 z+: 0 1 2 3
// face 0 x-: 4 5 1 0
// face 1 y-: 4 0 3 7
// face 2 z-: 7 6 5 4
Face facesVoxel[] = {{7, 6, 2, 3}, {2, 6, 5, 1}, {3, 2, 1, 0},
                     {0, 1, 5, 4}, {7, 3, 0, 4}, {4, 5, 6, 7}};
fvec3 vertexesVoxel[] = {
    {-0.5, -0.5, 0.5},  {-0.5, 0.5, 0.5},  {0.5, 0.5, 0.5},  {0.5, -0.5, 0.5},
    {-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, -0.5, -0.5}};

int readVoxels(std::vector<VoxelData> &voxelList, const char *path) {

  FILE *fp = fopen(path, "rb");

  int nb = 0;
  fread(&nb, 4, 1, fp);
  printf("nb voxels %d\n", nb);
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

  for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
    const std::vector<fvec3> &vertexes = it->second.vertexes;
    const std::vector<Face> &faces = it->second.faces;
    for (int i = 0; i < vertexes.size(); i++) {
      fprintf(fp, "v %f %f %f\n", vertexes[i][0], vertexes[i][1],
              vertexes[i][2]);
    }
    vertexesOffset.push_back(vertexes.size());
    totalFaces += faces.size();
  }

  int indexGroup = 0;
  fprintf(fp, "\n//Faces %d\n", totalFaces);
  for (VoxelGroup::const_iterator it = group.begin(); it != group.end(); it++) {
    int vertexOffset = vertexesOffset[indexGroup++] + 1;

    const std::vector<Face> &faces = it->second.faces;
    fprintf(fp, "group material_%d\n", indexGroup);
    for (int i = 0; i < faces.size(); i++) {
      fprintf(fp, "f %d %d %d %d\n", vertexOffset + faces[i][0],
              vertexOffset + faces[i][1], vertexOffset + faces[i][2],
              vertexOffset + faces[i][3]);
    }
  }

  fclose(fp);
  return 0;
}

int main(int argc, char **argv) {

  std::vector<VoxelData> voxelList;
  float voxelSize = 0.5;

  const char *path = argv[1];
  if (readVoxels(voxelList, path)) {
    printf("error\n");
    return 1;
  }

  VoxelScene scene;

#if 0

  typedef std::map<VoxelData, bool, cmpVoxel> VoxelMap;
  VoxelMap voxelMap;

  // fill the map
  for (int i = 0; i < voxelList.size(); i++) {
    voxelMap[voxelList[i]] = true;
  }

  // remove useless useless voxel
  // skip this part for now, I will do it later
  for (auto it = voxelMap.begin(); it != voxelMap.end();) {
    const VoxelData &voxel = (*it).first;

    // check in each direction to remove voxel
    bool visible = true;
    for (int axis = 0; axis < 3; axis++) {
      int value = voxel[axis];
      int endValue;

      endValue = 255 - value;
      for (int i = value + 1; i < endValue; i++) {
        VoxelData search = voxel;
        search[axis] = i;
        if (voxelMap.find(search) != voxelMap.end()) {
          visible = false;
          break;
        }
      }

      if (!visible) {
        endValue = 0 - value;
        for (int i = value - 1; i >= endValue; i--) {
          VoxelData search = voxel;
          search[axis] = i;
          if (voxelMap.find(search) == voxelMap.end()) {
            visible = false;
            break;
          }
        }
      }
    }

    if (!visible) {
      voxelMap.erase(it);
      printf("remove voxel %d %d %d\n", voxel[0], voxel[1], voxel[2]);
    } else {
      it++;
    }
  }
#endif

  auto voxelGroup = scene.voxelGroup;

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
      fvec3 v = vertexesVoxel[j];
      v += voxelPosition;
      vertexes.push_back(v);
    }

    // insert all faces
    for (int f = 0; f < 6; f++) {
      Face face = facesVoxel[f];
      face += vertexBaseIndex;
      faces.push_back(face);
    }
  }

  // write an obj
  return writeOBJ(voxelGroup, "./voxel.obj");
}
