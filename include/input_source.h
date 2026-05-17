#pragma once

#include <string>
#include <vector>

struct InputFrame {
    std::string imagePath;
    std::string imageName;
    std::string currentZone;
    std::string sceneDesc;
};

class InputSource {
public:
    explicit InputSource(const std::string& csvPath, const std::string& imageDir);

    bool hasNext() const;
    InputFrame next();
    size_t size() const;
    void reset();

private:
    std::vector<InputFrame> m_frames;
    size_t m_index = 0;
    std::string m_imageDir;

    std::vector<InputFrame> loadSequence(const std::string& csvPath);
};
