/*
    Not used yet
*/
#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "utils/mathlib.hpp"

namespace smd
{

    struct Bone
    {
        int id;
        std::string name;
        int parent;
    };

    struct Pose
    {
        int time;
        int parentbone;
        Vector3 pos;
        Vector3 rot;
    };

    struct Vertex
    {
        int parentbone;
        Vector3 pos;
        Vector3 normal;
        float u;
        float v;
    };

    struct Triangle
    {
        std::string texture;
        std::array<Vertex, 3> vertices;
    };

    struct Model
    {
        int version;
        std::vector<Bone> bones;
        std::vector<Pose> poses;
        std::vector<Triangle> triangles;
    };

    class SMDParser
    {
    public:
        Model parse(const std::string &filepath)
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + filepath);
            }

            Model smd;
            std::string line;

            while (std::getline(file, line))
            {
                std::istringstream iss(line);
                std::string token;
                iss >> token;

                if (token == "version")
                {
                    iss >> smd.version;
                    if (smd.version != 1)
                    {
                        throw std::runtime_error("Unsupported SMD version: " + std::to_string(smd.version));
                    }
                }
                else if (token == "nodes")
                {
                    parseNodes(file, smd);
                }
                else if (token == "skeleton")
                {
                    parseSkeleton(file, smd);
                }
                else if (token == "triangles")
                {
                    parseTriangles(file, smd);
                }
            }
            return smd;
        }

    private:
        void parseNodes(std::ifstream &file, Model &smd)
        {
            std::string line;
            while (std::getline(file, line) && line != "end")
            {
                Bone bone;
                std::istringstream boneStream(line);
                boneStream >> bone.id;
                if (boneStream.peek() == ' ')
                {
                    boneStream.ignore();
                }
                if (boneStream.peek() == '"')
                {
                    boneStream.ignore();
                    std::getline(boneStream, bone.name, '"');
                }
                boneStream >> bone.parent;
                smd.bones.push_back(bone);
            }
        }

        void parseSkeleton(std::ifstream &file, Model &smd)
        {
            std::string line;
            while (std::getline(file, line) && line != "end")
            {
                if (line.find("time") != std::string::npos)
                    continue;
                Pose pose;
                std::istringstream poseStream(line);
                poseStream >> pose.parentbone >> pose.pos.x >> pose.pos.y >> pose.pos.z >> pose.rot.x >> pose.rot.y >> pose.rot.z;
                smd.poses.push_back(pose);
            }
        }

        void parseTriangles(std::ifstream &file, Model &smd)
        {
            std::string line;
            while (std::getline(file, line) && !line.empty())
            {
                Triangle triangle;
                triangle.texture = line;
                for (int i = 0; i < 3; ++i)
                {
                    std::getline(file, line);
                    std::istringstream vertexStream(line);
                    Vertex vertex;
                    vertexStream >> vertex.parentbone >> vertex.pos.x >> vertex.pos.y >> vertex.pos.z >> vertex.normal.x >> vertex.normal.y >> vertex.normal.z >> vertex.u >> vertex.v;
                    triangle.vertices[i] = vertex;
                }
                smd.triangles.push_back(triangle);
            }
        }
    };

    Model load_smd(const std::filesystem::path &path)
    {
        SMDParser parser;
        return parser.parse(path.string());
    }

} // namespace smd