#include "input_source.h"
#include <fstream>
#include <sstream>

InputSource::InputSource(const std::string& csvPath, const std::string& imageDir)
    : m_imageDir(imageDir)
{
    m_frames = loadSequence(csvPath);
}

bool InputSource::hasNext() const { return m_index < m_frames.size(); }
InputFrame InputSource::next() { return m_frames[m_index++]; }
size_t InputSource::size() const { return m_frames.size(); }
void InputSource::reset() { m_index = 0; }

std::vector<InputFrame> InputSource::loadSequence(const std::string& csvPath) {
    std::vector<InputFrame> frames;
    std::ifstream file(csvPath);
    if (!file.is_open()) return frames;

    std::string line;
    std::getline(file, line);  // skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        int commas = 0;
        for (char c : line) if (c == ',') commas++;

        std::stringstream ss(line);
        std::string image, zone, scene;
        std::getline(ss, image, ',');
        if (commas >= 2) std::getline(ss, zone, ',');
        std::getline(ss, scene, ',');

        auto trim = [](std::string& s) { if (!s.empty() && s.back() == '\r') s.pop_back(); };
        trim(image); trim(zone); trim(scene);

        InputFrame frame;
        frame.imageName = image;
        frame.imagePath = m_imageDir + "/" + image;
        frame.currentZone = zone;
        frame.sceneDesc = scene;
        frames.push_back(frame);
    }
    return frames;
}
