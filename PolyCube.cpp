#include <iostream>
#include <fstream>
#include <string>
#include "hash_set8.hpp"
#include <thread>
#include <mutex>
#include <chrono>

std::mutex dedupeMutex;

const uint8_t rotationDescription[24][6] = {
    {0, 1, 2, 0, 1, 2},
    {0, 1, 2, 0, 4, 5},
    {0, 1, 2, 3, 1, 5},
    {0, 1, 2, 3, 4, 2},
    {1, 0, 2, 4, 0, 2},
    {1, 0, 2, 1, 3, 2},
    {2, 1, 0, 5, 1, 0},
    {2, 0, 1, 5, 0, 4},
    {2, 1, 0, 5, 4, 3},
    {2, 0, 1, 5, 3, 1},
    {1, 0, 2, 1, 0, 5},
    {1, 0, 2, 4, 3, 5},
    {2, 1, 0, 2, 1, 3},
    {2, 0, 1, 2, 0, 1},
    {2, 1, 0, 2, 4, 0},
    {2, 0, 1, 2, 3, 4},
    {0, 2, 1, 0, 2, 4},
    {1, 2, 0, 4, 2, 3},
    {0, 2, 1, 3, 2, 1},
    {1, 2, 0, 1, 2, 0},
    {0, 2, 1, 0, 5, 1},
    {1, 2, 0, 4, 5, 0},
    {0, 2, 1, 3, 5, 4},
    {1, 2, 0, 1, 5, 3}
};

int xyzTo1D(uint8_t n, uint8_t x, uint8_t y, uint8_t z)
{
    return z * n * n + y * n + x;
}

int getEncodeBit(uint8_t xlength, uint8_t ylength, uint8_t zlength, uint8_t x, uint8_t y, uint8_t z)
{
    return z * xlength * ylength + y * xlength + x;
}

struct polycube
{
    uint8_t xlength, ylength, zlength;

    //potential optimization here. instead of having a fixed size, dynamically allocate only the minimum necessary for each shape
    //should double memory efficiency
    uint64_t encoding[4];
};

bool extractBit(const polycube& shape, uint8_t x, uint8_t y, uint8_t z)
{
    int placement = getEncodeBit(shape.xlength, shape.ylength, shape.zlength, x, y, z);
    int placementMod = placement % 64;
    placement /= 64;
    return shape.encoding[placement] & (uint64_t)1 << placementMod;
}

bool operator==(const polycube& lhs, const polycube& rhs)
{
    return lhs.xlength == rhs.xlength && lhs.ylength == rhs.ylength && lhs.zlength == rhs.zlength &&
        lhs.encoding[0] == rhs.encoding[0] &&
        lhs.encoding[1] == rhs.encoding[1] &&
        lhs.encoding[2] == rhs.encoding[2] &&
        lhs.encoding[3] == rhs.encoding[3];
}

bool compareEncoding(const polycube& lhs, const polycube& rhs)
{
    for (int i = 0; i < 4; i++)
    {
        if (lhs.encoding[i] > rhs.encoding[i]) return true;
        else if (lhs.encoding[i] < rhs.encoding[i]) return false;
    }
    if (lhs.xlength > rhs.xlength) return true;
    if (lhs.xlength == rhs.xlength && lhs.ylength > rhs.ylength) return true;
    if (lhs.xlength == rhs.xlength && lhs.ylength == rhs.ylength && lhs.zlength > rhs.zlength) return true;
    return false;
}

template <>
struct std::hash<polycube>
{
    std::size_t operator()(const polycube& cube) const
    {
        //chatgpt generated hash function. could be a potential optimization. try to reduce hash collisions

        std::size_t seed = 0;

        // Combine dimension information into the seed
        seed ^= std::hash<uint8_t>{}(cube.xlength) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(cube.ylength) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(cube.zlength) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        // Combine encoding into the seed
        for (int i = 0; i < 4; ++i) {
            seed ^= std::hash<uint64_t>{}(cube.encoding[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        return seed;
    }
};

//multithreading not fully tested. can't be sure if this produces valid results
//hard coded input path
void multithreadRemoveDuplicates(emhash8::HashSet<polycube>* hashSet, std::string threadName)
{
    std::unique_lock<std::mutex> lock(dedupeMutex, std::defer_lock);
    std::ifstream inputFile("D:/polycube/output16" +threadName+ ".polycubes", std::ios_base::binary);
    char* readBuffer=new char[409600];
    inputFile.rdbuf()->pubsetbuf(readBuffer, 409600);
    polycube savedPolycube;
    while (inputFile.read((char*)&savedPolycube.xlength, 1))
    {
        inputFile.read((char*)&savedPolycube.ylength, 1);
        inputFile.read((char*)&savedPolycube.zlength, 1);

        memset(&savedPolycube.encoding, 0, sizeof(savedPolycube.encoding));

        int byteCount = savedPolycube.xlength * savedPolycube.ylength * savedPolycube.zlength;
        byteCount--;
        byteCount /= 8;
        byteCount++;
        inputFile.read((char*)savedPolycube.encoding, byteCount);
        if (hashSet->contains(savedPolycube))
        {
            lock.lock();
            hashSet->erase(savedPolycube);
            lock.unlock();
        }
    }
    inputFile.close();
    delete[] readBuffer;
}

//this is the major bottleneck at larger n
//dumping hashset contents to disk first requires scanning through the entire file and removing duplicates first
//this can get very costly as n increases but not an issue if the entire set is contained in ram
//returns how many shapes were added
uint64_t DumpToFile(emhash8::HashSet<polycube> &hashSet, std::string fileName)
{
    std::ifstream inputFile(fileName, std::ios_base::binary);
    char readBuffer[409600];
    inputFile.rdbuf()->pubsetbuf(readBuffer, 409600);
    polycube savedPolycube;
    while (inputFile.read((char*)&savedPolycube.xlength, 1))
    {
        inputFile.read((char*)&savedPolycube.ylength, 1);
        inputFile.read((char*)&savedPolycube.zlength, 1);

        memset(&savedPolycube.encoding, 0, sizeof(savedPolycube.encoding));

        int byteCount = savedPolycube.xlength * savedPolycube.ylength * savedPolycube.zlength;
        byteCount--;
        byteCount /= 8;
        byteCount++;
        inputFile.read((char*)savedPolycube.encoding, byteCount);
        hashSet.erase(savedPolycube);
    }
    inputFile.close();
    memset(readBuffer, 0, 409600);

    uint64_t shapeCount=0;
    std::ofstream outputFile(fileName, std::ios_base::binary | std::ios_base::app);
    outputFile.rdbuf()->pubsetbuf(readBuffer, 409600);
    while (hashSet.size() > 0)
    {
        auto shape = hashSet.begin();
        outputFile.write((char*) & shape->xlength, 1);
        outputFile.write((char*)&shape->ylength, 1);
        outputFile.write((char*)&shape->zlength, 1);

        int byteCount = shape->xlength * shape->ylength * shape->zlength;
        byteCount--;
        byteCount /= 8;
        byteCount++;
        outputFile.write((char*)shape->encoding, byteCount);

        hashSet.erase(shape);
        shapeCount++;
    }
    return shapeCount;
}

int main(int argc, char** argv)
{
    //parse command line arguments
    //little to no error checking so use them correctly please
     
     

    //defaults listed here
    int n = 2;
    int uptoN = 16;
    std::string fileName = "output1.polycubes";
    uint64_t hashsetLimit = 450000000; //filled up my 32gb machine. if this value is too high, it will crash

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && i < argc - 1 && strlen(argv[i])==2)
        {
            if (argv[i][1] == 'b')
            {//b for begin at this n, loads file for n-1 specified in -f argument
                n = std::atoi(argv[i + 1]);
            }
            else if (argv[i][1] == 'f')
            {//path for file used when starting at n>2
                fileName = argv[i + 1];
            }
            else if (argv[i][1] == 'n')
            {//calculate up to this n
                uptoN = std::atoi(argv[i+1]);
            }
            else if (argv[i][1] == 'l')
            {//limit hash set size to this number
                hashsetLimit = std::stoull(argv[i + 1]);
            }
        }

    }

    if (n == 2)
    {//need to create beginning file for n=1. literally just 4 bytes each being 1
        std::ofstream beginningFile("output1.polycubes", std::ios_base::binary);
        char one = 1;
        beginningFile.write(&one, 1);
        beginningFile.write(&one, 1);
        beginningFile.write(&one, 1);
        beginningFile.write(&one, 1);
    }
    uint64_t shapeCount = 1;
    emhash8::HashSet<polycube> currentCubeMap({});

    //crashes here if limit is set too high. 32gb can handle about 450 million if nothing else if open
    //I tried putting this in a try catch block but it didn't work
    currentCubeMap.reserve(hashsetLimit);
    
    polycube rotations[7][24];
    polycube shape;

    auto start = std::chrono::high_resolution_clock::now();
    char readBuffer[409600];
    memset(readBuffer, 0, 409600);
    while (n <= uptoN)
    {
        uint64_t progressCount = 0;
        uint64_t prevSize = shapeCount;
        shapeCount = 0;
        bool* padded = new bool[(n + 1) * (n + 1) * (n + 1)]();
        std::ifstream inputFile(fileName, std::ios_base::binary);
        memset(readBuffer, 0, 409600);
        inputFile.rdbuf()->pubsetbuf(readBuffer, 409600);
        fileName = "output" + std::to_string(n) + ".polycubes";

        //this is where all the calculations happen
        while (inputFile.read((char*)&shape.xlength, 1))
        {
            progressCount++;
            if (progressCount % 100000 == 0)
            {
                std::cout << "\r" << ((double)progressCount / (double)prevSize) * 100 << "%                    ";
            }

            //read each shape of the previous n from a file
            inputFile.read((char*)&shape.ylength, 1);
            inputFile.read((char*)&shape.zlength, 1);

            memset(&shape.encoding, 0, sizeof(shape.encoding));

            int byteCount = shape.xlength * shape.ylength * shape.zlength;
            byteCount--;
            byteCount /= 8;
            byteCount++;
            inputFile.read((char*)shape.encoding, byteCount);
            memset(padded, 0, (n + 1) * (n + 1) * (n + 1));
            memset(rotations, 0, sizeof(rotations));

            uint8_t lengths[7][3] = {
                {shape.xlength,shape.ylength,shape.zlength},
                {shape.xlength + 1,shape.ylength,shape.zlength},
                {shape.xlength,shape.ylength + 1,shape.zlength},
                {shape.xlength,shape.ylength,shape.zlength + 1},
                {shape.xlength + 1,shape.ylength,shape.zlength},
                {shape.xlength,shape.ylength + 1,shape.zlength},
                {shape.xlength,shape.ylength,shape.zlength + 1}
            };

            //convert the int encoding of the shape into a 3d array of bools
            //also generates all 24 rotations of the shape
            for (uint8_t z = 0; z < shape.zlength; z++)
            {
                for (uint8_t y = 0; y < shape.ylength; y++)
                {
                    for (uint8_t x = 0; x < shape.xlength; x++)
                    {
                        if (extractBit(shape, x, y, z))
                        {
                            //need to generate 7 sets of 24 rotations as the int encoding of the shape doesn't make it easy to expand it when a new cube gets added
                            uint8_t coords[7][6] = {
                                {x, y, z, shape.xlength - 1 - x, shape.ylength - 1 - y, shape.zlength - 1 - z}, //dimensions do not expand
                                {x + 1, y, z, shape.xlength - 1 - x, shape.ylength - 1 - y, shape.zlength - 1 - z}, //x expands, shift needed
                                {x, y + 1, z, shape.xlength - 1 - x, shape.ylength - 1 - y, shape.zlength - 1 - z}, //y expands, shift needed
                                {x, y, z + 1, shape.xlength - 1 - x, shape.ylength - 1 - y, shape.zlength - 1 - z}, //z expands, shift needed
                                {x, y, z, shape.xlength - x, shape.ylength - 1 - y, shape.zlength - 1 - z}, //x expands, shift not needed
                                {x, y, z, shape.xlength - 1 - x, shape.ylength - y, shape.zlength - 1 - z}, //y expands, shift not needed
                                {x, y, z, shape.xlength - 1 - x, shape.ylength - 1 - y, shape.zlength - z}  //z expands, shift not needed
                            };
                            padded[xyzTo1D(n + 1, x + 1, y + 1, z + 1)] = extractBit(shape, x, y, z);
                            for (int i = 0; i < 7; i++)
                            {
                                for (int j = 0; j < 24; j++)
                                {
                                    int encodeBit = getEncodeBit(lengths[i][rotationDescription[j][0]],
                                        lengths[i][rotationDescription[j][1]],
                                        lengths[i][rotationDescription[j][2]],
                                        coords[i][rotationDescription[j][3]],
                                        coords[i][rotationDescription[j][4]],
                                        coords[i][rotationDescription[j][5]]);
                                    int modBit = encodeBit % 64;
                                    encodeBit /= 64;
                                    rotations[i][j].encoding[encodeBit] += (uint64_t)1 << modBit;
                                }
                            }
                        }
                    }
                }
            }

            //set new dimensions
            for (int i = 0; i < 7; i++)
            {
                for (int j = 0; j < 24; j++)
                {
                    rotations[i][j].xlength = lengths[i][rotationDescription[j][0]];
                    rotations[i][j].ylength = lengths[i][rotationDescription[j][1]];
                    rotations[i][j].zlength = lengths[i][rotationDescription[j][2]];
                }
            }

            //here all the new shapes get created by checking each spot and seeing if it's a valid place to put a new cube
            for (uint8_t z = 0; z < shape.zlength + 2; z++)
            {
                for (uint8_t y = 0; y < shape.ylength + 2; y++)
                {
                    for (uint8_t x = 0; x < shape.xlength + 2; x++)
                    {
                        if ((!padded[xyzTo1D(n + 1, x, y, z)]) &&
                            ((x != 0 && padded[xyzTo1D(n + 1, x - 1, y, z)]) ||
                                (x != n && padded[xyzTo1D(n + 1, x + 1, y, z)]) ||
                                (y != 0 && padded[xyzTo1D(n + 1, x, y - 1, z)]) ||
                                (y != n && padded[xyzTo1D(n + 1, x, y + 1, z)]) ||
                                (z != 0 && padded[xyzTo1D(n + 1, x, y, z - 1)]) ||
                                (z != n && padded[xyzTo1D(n + 1, x, y, z + 1)])))
                        {
                            uint8_t set;
                            if (x == 0) set = 1;
                            else if (y == 0) set = 2;
                            else if (z == 0) set = 3;
                            else if (x == shape.xlength + 1) set = 4;
                            else if (y == shape.ylength + 1) set = 5;
                            else if (z == shape.zlength + 1) set = 6;
                            else set = 0;
                            //converted padded coordinates to cropped coordinates
                            uint8_t coords[7][6] = {
                                {x - 1, y - 1, z - 1, shape.xlength - x, shape.ylength - y, shape.zlength - z}, //dimensions do not expand
                                {0, y - 1, z - 1, shape.xlength, shape.ylength - y, shape.zlength - z}, //x expands, shift needed
                                {x - 1, 0, z - 1, shape.xlength - x, shape.ylength, shape.zlength - z}, //y expands, shift needed
                                {x - 1, y - 1, 0, shape.xlength - x, shape.ylength - y, shape.zlength}, //z expands, shift needed
                                {shape.xlength, y - 1, z - 1, 0, shape.ylength - y, shape.zlength - z}, //x expands, shift not needed
                                {x - 1, shape.ylength, z - 1, shape.xlength - x, 0, shape.zlength - z}, //y expands, shift not needed
                                {x - 1, y - 1, shape.zlength, shape.xlength - x, shape.ylength - y, 0}  //z expands, shift not needed
                            };
                            uint8_t maxIndex = 0;

                            //add the new cube to each rotation
                            for (int i = 0; i < 24; i++)
                            {
                                int encodeBit = getEncodeBit(rotations[set][i].xlength,
                                    rotations[set][i].ylength,
                                    rotations[set][i].zlength,
                                    coords[set][rotationDescription[i][3]],
                                    coords[set][rotationDescription[i][4]],
                                    coords[set][rotationDescription[i][5]]);
                                int modBit = encodeBit % 64;
                                encodeBit /= 64;
                                rotations[set][i].encoding[encodeBit] += (uint64_t)1 << modBit;

                                //only care about the rotations with the maximum value
                                //because if 2 shapes are just rotations of each other then they both have the same rotation with the max value
                                //which makes comparing very easy
                                if (compareEncoding(rotations[set][i], rotations[set][maxIndex])) maxIndex = i;
                            }

                            currentCubeMap.insert(rotations[set][maxIndex]);

                            //need to reset the shape for the next one
                            for (int i = 0; i < 24; i++)
                            {
                                int encodeBit = getEncodeBit(rotations[set][i].xlength,
                                    rotations[set][i].ylength,
                                    rotations[set][i].zlength,
                                    coords[set][rotationDescription[i][3]],
                                    coords[set][rotationDescription[i][4]],
                                    coords[set][rotationDescription[i][5]]);
                                int modBit = encodeBit % 64;
                                encodeBit /= 64;
                                rotations[set][i].encoding[encodeBit] -= (uint64_t)1 << modBit;
                            }
                        }
                    }
                }
            }
            if (currentCubeMap.size() >= hashsetLimit)
            {
                shapeCount+=DumpToFile(currentCubeMap, fileName);
            }
        }
        shapeCount+=DumpToFile(currentCubeMap, fileName);
        delete[] padded;
        std::cout << "\r" << n << ": " << shapeCount << "                       \n";
        n++;
    }
    auto elapsed = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start);
    std::cout << "elapsed time: " << elapsed.count() << std::endl;
    return 0;
}