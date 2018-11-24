#include "VoxReader.h"

#include <bitset>
#include <cstring>
#include <fstream>
#include <stdio.h>

typedef std::map<std::string, std::string> VoxDict;
typedef std::pair<std::string, std::string> string_pair;
typedef std::pair<std::string, float> string_float_pair;

#ifdef DEBUG
static void print(const VoxTransform& o)
{
    printf("-----VoxTransform --- : %d\n", o.nodeId);
    printf("    -- nodeId : %d\n", o.nodeId);
    printf("    -- name : %s\n", o.name.c_str());
    printf("    -- hidden : %d\n", o.hidden);
    printf("    -- childNodeId : %d\n", o.childNodeId);
    printf("    -- reservedId : %d\n", o.reservedId);
    printf("    -- layerId : %d\n", o.layerId);
    printf("    -- numFrames : %d\n", o.numFrames);

    for (int i = 0; i < o.numFrames; ++i) {
        printf(" ++ FRAME ++ \n");
        printf("    + Translation %d %d %d\n", o.initialFrame.translation[0], o.initialFrame.translation[1],
               o.initialFrame.translation[2]);

        printf("    + rotation ");
        for (unsigned int r = 0; r < 9; ++r) {
            printf("%f ", o.initialFrame.rotation[r]);
        }
        printf(" ++++++ \n");
    }

    printf("\n\n");
}

static void print(const VoxGroup& o)
{
    printf("-----VoxGroup --- : \n");
    printf("    -- nodeId : %d\n", o.nodeId);
    printf("    -- name : %s\n", o.name.c_str());
    printf("    -- hidden : %d\n", o.hidden);
    printf("    -- childNodeId : %d\n", o.numChildren);

    if (o.numChildren) {
        printf("[ %d", o.children[0]);
        for (int i = 1; i < o.numChildren; ++i) {
            printf(", %d", o.children[i]);
        }
        printf(" ]\n\n");
    }
}

static void print(const VoxShape& o)
{
    printf("-----VoxShape --- : \n");
    printf("    -- nodeId : %d\n", o.nodeId);
    printf("    -- name : %s\n", o.name.c_str());
    printf("    -- hidden : %d\n", o.hidden);
    printf("    -- numModels : %d\n", o.numModels);

    printf("[ ");

    for (auto m = o.models.begin(); m != o.models.end(); ++m) {
        printf("[%d => %s .. %s], ", m->first, m->second.first.c_str(), m->second.second.c_str());
    }

    printf("]\n\n");
}

static void print(const VoxScene& o)
{
    printf("Scene: \n");
    printf(" Size: %d %d %d\n", o.sizeX, o.sizeY, o.sizeZ);
    printf("  - Voxels: %lu\n", o.voxels.size());
    printf("  - Palette: %lu\n", o.palettes.size());
    printf("  - Materials: %lu\n", o.materials.size());
    printf("  - Groups: %lu\n", o.groups.size());
    printf("  - Transforms: %lu\n", o.transforms.size());
    printf("  - Shapes: %lu\n", o.shapes.size());
    printf("-------------\n");
}
#endif

bool isSupportedChunk(const std::string& chunkName)
{
    return true;
    return (chunkName == "MAIN" || chunkName == "SIZE" || chunkName == "XYZI" || chunkName == "RGBA" ||
            chunkName == "nGRP" || chunkName == "nTRN" || chunkName == "nSHP");
}

static std::string readChunk(const uint8_t* bytes)
{
    char chunkId[4];
    memcpy(&chunkId, bytes, 4);
    return std::string(reinterpret_cast<const char*>(&chunkId[0]), 4);
}

bool VoxReader::decodeChunk(const std::string& chunkName, const uint8_t* content, unsigned int size)
{
    if (chunkName == "SIZE") {
        decodeSizeChunk(content);
    } else if (chunkName == "XYZI") {
        decodePosChunk(content);
    } else if (chunkName == "LAYR") {
        printf("- Unsupported LAYR (no spec)\n");
    } else if (chunkName == "RGBA") {
        decodePaletteChunk(content, size);
    } else if (chunkName == "MATL") {
        decodeMaterialChunk(content);
    } else if (chunkName == "rOBJ") {
        printf("- Unsupported rOBJ (no spec)\n");
    } else if (chunkName == "nTRN") {
        decodeTransform(content);
    } else if (chunkName == "nGRP") {
        decodeGroup(content);
    } else if (chunkName == "nSHP") {
        decodeShape(content);
    }

    return true;
}

void VoxReader::readChunk(const uint8_t* bytes, unsigned int size)
{

    std::string chunkName = ::readChunk(bytes);

    uint32_t chunkContentSize = *((uint32_t*)(bytes + 4));
    uint32_t childChunkContentSize = *((uint32_t*)(bytes + 8));

    if (!isSupportedChunk(chunkName)) {
        printf("Unsupported chunk %s\n", chunkName.c_str());
    } else {
        if (chunkContentSize > 0) {
            uint8_t chunkContent[chunkContentSize];
            memcpy(&chunkContent, bytes + 12, chunkContentSize);
            decodeChunk(chunkName, &chunkContent[0], chunkContentSize);
        }

        if (childChunkContentSize > 0) {
            uint8_t childChunkContent[childChunkContentSize];
            memcpy(&childChunkContent, bytes + 12 + chunkContentSize, childChunkContentSize);
            readChunk(&childChunkContent[0], childChunkContentSize);
        }

        // Chunk is read, now read the rest of the data
        int remainingSize = size - 12 - chunkContentSize - childChunkContentSize;
        if (remainingSize > 0) {
            readChunk(&bytes[12 + chunkContentSize + childChunkContentSize], remainingSize);
        }
    }
}

bool VoxReader::decodeSizeChunk(const uint8_t* content)
{
    memcpy(&_voxScene.sizeX, content, 4);
    memcpy(&_voxScene.sizeY, content + 4, 4);
    memcpy(&_voxScene.sizeZ, content + 8, 4);
    return true;
}

bool VoxReader::readFile(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        printf("Failed to open file\n");
        return false;
    }
    fseek(fp, 0, SEEK_END);

    int size = ftell(fp);
    if (size == 0) {
        printf("File is empty\n");
        return false;
    }

    std::vector<uint8_t> buf(size);

    fseek(fp, 0, SEEK_SET);
    fread(&buf[0], size, 1, fp);
    fclose(fp);

    return loadVoxelsData(&buf[0], size);
}

bool VoxReader::loadVoxelsData(const uint8_t* bytes,
                               size_t size) // ADD structure
{
    if (bytes[0] != 'V' || bytes[1] != 'O' || bytes[2] != 'X' || bytes[3] != ' ') {
        printf("It's not a vox file!\n");
        return false;
    }

    int version;
    memcpy(&version, bytes + 4, 4);
    printf("Version: %d\n", version);

    std::string chunkIdStr = ::readChunk(bytes + 8);
    printf("ChunkId: %s\n", chunkIdStr.c_str());

    if (strcmp(chunkIdStr.c_str(), "MAIN") == 0) {
        readChunk(&bytes[8], size - 12);
    } else {
        printf("No main chunk found, aborting.\n");
        return false;
    }

#ifdef DEBUG
    print(_voxScene);
#endif
    return true;
}

static void decodeRotation(const uint8_t rotation, float* values)
{
    values[0] = values[1] = values[2] = values[3] = values[4] = values[5] = values[6] = values[7] = values[8] = 0.0f;

    float signed1 = rotation & (1 << 4) ? -1 : 1;
    float signed2 = rotation & (1 << 5) ? -1 : 1;
    float signed3 = rotation & (1 << 6) ? -1 : 1;

    int index1 = rotation & 3;
    int index2 = (rotation >> 2) & 3;
    int index3 = 0;

    if (index1 == 0 || index2 == 0) {
        index3 = 1;
        if (index2 == 1 || index2 == 1)
            index3 = 2;
    }

    values[index1] = signed1;
    values[3 + index2] = signed2;
    values[6 + index3] = signed3;
}

uint32_t VoxReader::decodeInt(const uint8_t* content, int& currentPos)
{
    uint32_t value;
    memcpy(&value, &content[currentPos], 4);
    currentPos += 4;
    return value;
}

std::string VoxReader::decodeString(const uint8_t* content, int& currentPos)
{
    uint32_t size;
    memcpy(&size, &content[currentPos], 4);
    std::string strVal((char*)&content[currentPos + 4], size);
    currentPos += 4 + size;
    return strVal;
}

/*
=================================
(1) Transform Node Chunk : "nTRN"

int32   : node id
DICT    : node attributes
      (_name : string)
      (_hidden : 0/1)
int32   : child node id
int32   : reserved id (must be -1)
int32   : layer id
int32   : num of frames (must be 1)

// for each frame
{
DICT    : frame attributes
      (_r : int8) ROTATION, see (c)
      (_t : int32x3) translation
}xN*/

struct TransformTranslate {
    TransformTranslate()
        : v{0, 0, 0}
    {}
    int32_t v[3];
    inline int32_t& operator[](int index) { return v[index]; }
    inline int32_t operator[](int index) const { return v[index]; }
};

static TransformTranslate convertStringToVec3(const std::string& vectorString)
{
    TransformTranslate vector;
    std::size_t current, previous = 0;

    current = vectorString.find(' ');
    for (unsigned int i = 0; i < 3; ++i) {
        vector[i] = stoi(vectorString.substr(previous, current - previous));
        previous = current + 1;
        current = vectorString.find(' ', previous);
    }

    return vector;
}

void VoxReader::decodeTransform(const uint8_t* content)
{
    VoxTransform transform;

    int currentPos = 0;
    transform.nodeId = decodeInt(content, currentPos);

    // DICT: get keyval pair
    int keyvalpair = decodeInt(content, currentPos);
    for (int i = 0; i < keyvalpair; ++i) {
        std::string key = decodeString(content, currentPos);
        if (key == "_name") {
            transform.name = decodeString(content, currentPos);
        } else {
            std::string hidden = decodeString(content, currentPos);
        }
    }

    transform.childNodeId = decodeInt(content, currentPos);
    transform.reservedId = decodeInt(content, currentPos);
    transform.layerId = decodeInt(content, currentPos);
    transform.numFrames = decodeInt(content, currentPos);

    // DICT: get keyval pair
    keyvalpair = decodeInt(content, currentPos);
    for (int i = 0; i < keyvalpair; ++i) {
        std::string key = decodeString(content, currentPos);
        if (key == "_r") {
            decodeRotation(content[currentPos], &transform.initialFrame.rotation[0]);
            currentPos++;
        } else {
            std::string translationString = decodeString(content, currentPos);
            TransformTranslate values = convertStringToVec3(translationString);
            transform.initialFrame.translation[0] = values[0];
            transform.initialFrame.translation[1] = values[1];
            transform.initialFrame.translation[2] = values[2];
        }
    }

#ifdef DEBUG
    print(transform);
#endif
    _voxScene.transforms.push_back(transform);
}
/*=================================
(2) Group Node Chunk : "nGRP"

int32   : node id
DICT    : node attributes
int32   : num of children nodes

// for each child
{
int32   : child node id
}xN*/

void VoxReader::decodeGroup(const uint8_t* content)
{
    VoxGroup group;
    int currentPos = 0;
    group.nodeId = decodeInt(content, currentPos);

    int keyvalpair = decodeInt(content, currentPos);
    for (int i = 0; i < keyvalpair; ++i) {
        std::string key = decodeString(content, currentPos);
        if (key == "_name") {
            group.name = decodeString(content, currentPos);
        } else {
            std::string hidden = decodeString(content, currentPos);
        }
    }

    group.numChildren = decodeInt(content, currentPos);

    std::vector<int> children;
    children.reserve(group.numChildren);
    for (int i = 0; i < group.numChildren; ++i) {
        children.push_back(decodeInt(content, currentPos));
    }

    group.children = children;
    _voxScene.groups.push_back(group);
#ifdef DEBUG
    print(group);
#endif
}

/*=================================
(3) Shape Node Chunk : "nSHP"

int32   : node id
DICT    : node attributes
int32   : num of models (must be 1)

// for each model
{
int32   : model id
DICT    : model attributes : reserved
}xN*/

void VoxReader::decodeShape(const uint8_t* content)
{
    VoxShape shape;
    int currentPos = 0;
    shape.nodeId = decodeInt(content, currentPos);

    int keyvalpair = decodeInt(content, currentPos);
    for (int i = 0; i < keyvalpair; ++i) {
        std::string key = decodeString(content, currentPos);
        if (key == "_name") {
            shape.name = decodeString(content, currentPos);
        } else {
            std::string hidden = decodeString(content, currentPos);
        }
    }

    shape.numModels = decodeInt(content, currentPos);
    std::map<int, std::pair<std::string, std::string>> models;
    for (int i = 0; i < shape.numModels; ++i) {
        int modelId = decodeInt(content, currentPos);
        int subKeyvalpair = decodeInt(content, currentPos);
        std::string name, hidden;
        for (int j = 0; j < subKeyvalpair; ++j) {
            std::string key = decodeString(content, currentPos);
            if (key == "_name") {
                name = decodeString(content, currentPos);
            } else {
                hidden = decodeString(content, currentPos);
            }
        }

        models.insert(std::pair<int, std::pair<std::string, std::string>>(
            modelId, std::pair<std::string, std::string>(name, hidden)));
    }

    shape.models = models;

    _voxScene.shapes.push_back(shape);
#ifdef DEBUG
    print(shape);
#endif
}

bool VoxReader::decodePosChunk(const uint8_t* content)
{
    uint32_t nbVoxels;
    memcpy(&nbVoxels, content, sizeof(uint32_t));
    VoxModel voxels(nbVoxels);
    memcpy(&voxels[0], content + 4, 4 * nbVoxels);
    printf("%lu voxels\n", voxels.size());
    _voxScene.voxels.push_back(voxels);
    return true;
}

bool VoxReader::decodePaletteChunk(const uint8_t* content, unsigned int size)
{
    VoxPalette palette;
    palette.reserve(size + 1);
    palette.push_back(0);
    memcpy(&palette[1], content, size * 4);
    _voxScene.palettes.push_back(palette);
    return true;
}

bool VoxReader::decodeMaterialChunk(const uint8_t* content)
{
    VoxMaterial material;
    int currentPos = 0;
    int materialId = decodeInt(content, currentPos);
    int nbKeys = decodeInt(content, currentPos);
    for (int i = 0; i < nbKeys; ++i) {
        std::string key = decodeString(content, currentPos);
        std::string value = decodeString(content, currentPos);
        material.setFromProperty(key, value);
    }

    _voxScene.materials[materialId] = material;
    return true;
}
