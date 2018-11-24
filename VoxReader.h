#pragma once

#include <bitset>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

struct VoxelPos {
    uint8_t v[4];
    VoxelPos()
        : v{0, 0, 0, 0}
    {}
    inline uint8_t& operator[](int index) { return v[index]; }
    inline uint8_t operator[](int index) const { return v[index]; }
};

typedef std::vector<VoxelPos> VoxModel;
typedef std::vector<int> VoxPalette;

struct VoxGroup {
    int nodeId;
    std::string name;
    bool hidden;
    int numChildren;
    std::vector<int> children;
};

struct VoxShape {
    int nodeId;
    std::string name;
    bool hidden;
    int numModels;
    std::map<int, std::pair<std::string, std::string>> models;
};

struct VoxTransform {
    struct Frame {
        std::vector<float> rotation = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        int translation[3] = {0, 0, 0};
    };

    int nodeId;
    std::string name;
    bool hidden;
    int childNodeId;
    int reservedId;
    int layerId;
    int numFrames;
    Frame initialFrame;
};

struct VoxMaterial {
    enum MATERIAL_TYPE { DIFFUSE, METAL, GLASS, EMIT };

    MATERIAL_TYPE type;
    float weight = -1.0;
    float rough = -1.0;
    float spec = -1.0;
    float ior = -1.0;
    float att = -1.0;
    float flux = -1.0;
    float plastic = -1.0;

    void setFromProperty(const std::string& key, const std::string& value)
    {
        if (key == "_type") {
            if (value == "_diffuse") {
                type = DIFFUSE;
            } else if (value == "_metal") {
                type = METAL;
            } else if (value == "_glass") {
                type = GLASS;
            } else if (value == "_emit") {
                type = EMIT;
            }
        }

        if (key == "_weight") {
            weight = stof(value);
        } else if (key == "_rough") {
            rough = stof(value);
        } else if (key == "_spec") {
            spec = stof(value);
        } else if (key == "_ior") {
            ior = stof(value);
        } else if (key == "_att") {
            att = stof(value);
        } else if (key == "_flux") {
            flux = stof(value);
        } else if (key == "_plastic") {
            plastic = stof(value);
        }
    }

    bool operator==(const VoxMaterial& other)
    {
        if (type != other.type || weight != other.weight || rough != other.rough || spec != other.spec ||
            ior != other.ior || att != other.att || flux != other.flux || plastic != other.plastic)
            return false;

        return true;
    }
};

struct VoxScene {
    uint32_t sizeX, sizeY, sizeZ;
    std::vector<VoxModel> voxels;
    std::vector<VoxPalette> palettes;
    std::map<int, VoxMaterial> materials;
    std::vector<VoxGroup> groups;
    std::vector<VoxTransform> transforms;
    std::vector<VoxShape> shapes;
};

class VoxReader {

  public:
    VoxReader() {}
    ~VoxReader() {}

    bool readFile(const std::string& filename);
    VoxScene getVoxelScene() { return _voxScene; }

    bool decodeChunk(const std::string& chunkName, const uint8_t* content, unsigned int size);
    void readChunk(const uint8_t* bytes, unsigned int size);
    bool decodeSizeChunk(const uint8_t* content);

    bool loadVoxelsData(const uint8_t* bytes, size_t size);
    void parseRotation(const std::bitset<7>& bitset, float* values);
    float* decodeRotation(const uint8_t rotation, float* values);
    int decodeInt(const uint8_t* content, int& currentPos);
    std::string decodeString(const uint8_t* content, int& currentPos);
    void decodeTransform(const uint8_t* content);
    void decodeGroup(const uint8_t* content);
    void decodeShape(const uint8_t* content);
    bool decodePosChunk(const uint8_t* content);
    bool decodePaletteChunk(const uint8_t* content, unsigned int size);
    bool isFloatProp(const std::string& property);
    bool isStringProp(const std::string& property);
    bool decodeMaterialChunk(const uint8_t* content);

  private:
    VoxScene _voxScene;
};
