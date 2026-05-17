#include "input_source.h"
#include <fstream>
#include <sstream>

InputSource::InputSource(const std::string& csvPath, const std::string& imageDir)
    : m_imageDir(imageDir)
{
    m_frames = loadSequence(csvPath);
}

bool InputSource::hasNext() const {
    return m_index < m_frames.size();
}

InputFrame InputSource::next() {
    return m_frames[m_index++];
}

size_t InputSource::size() const {
    return m_frames.size();
}

void InputSource::reset() {
    m_index = 0;
}

std::vector<InputFrame> InputSource::loadSequence(const std::string& csvPath) {
    std::vector<InputFrame> frames;
    std::ifstream file(csvPath);
    if (!file.is_open()) return frames;

    std::string line;
    // skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string image, zone, scene;

        std::getline(ss, image, ',');
        std::getline(ss, zone, ',');
        std::getline(ss, scene, ',');

        // trim trailing \r
        if (!scene.empty() && scene.back() == '\r') scene.pop_back();
        if (!zone.empty() && zone.back() == '\r') zone.pop_back();
        if (!image.empty() && image.back() == '\r') image.pop_back();

        InputFrame frame;
        frame.imageName = image;
        frame.imagePath = m_imageDir + "/" + image;
        frame.currentZone = zone;
        frame.sceneDesc = scene;
        frames.push_back(frame);
    }
    return frames;
}
