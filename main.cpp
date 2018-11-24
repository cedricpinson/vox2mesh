#include <getopt.h>

#include "VoxReader.h"
#include "polygonize.h"

void printUsage()
{
    const char* text = "vox2obj is a tool to convert vox to obj\n"
                       "usages:\n"
                       " vox2obj input.vox output.obj\n"
                       "\n";

    printf("%s", text);
}

struct Options {
    const char* inputFile = 0;
    const char* outputFile = "output.obj";
    int cleanFaces = 1;
};

int parseArgument(Options&, int argc, char** argv)
{
    int opt;
    int optionIndex = 0;

    static const char* OPTSTR = "h";
    static const struct option OPTIONS[] = {
        {"help", no_argument, nullptr, 'h'}, {nullptr, 0, 0, 0} // termination of the option list
    };

    while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &optionIndex)) >= 0) {
        // const char *arg = optarg ? optarg : "";
        if (opt == -1)
            break;

        switch (opt) {
        default:
        case 'h':
            printUsage();
            exit(0);
            break;
        }
    }
    return optind;
}

int main(int argc, char** argv)
{

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

    VoxReader reader;
    if (!reader.readFile(options.inputFile)) {
        printf("error reading voxels\n");
        return 1;
    }

    std::vector<VoxelGroup> meshList;

    polygonize(meshList, reader.getVoxelScene());

    // write an obj
    return writeOBJ(meshList[0], options.outputFile);
}
