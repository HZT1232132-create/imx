#include "input_source.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>

/**
 * Simple USB camera frame source implementing IFrameSource-compatible interface.
 *
 * Usage: ./sorting_sim camera 0  # camera index 0
 *
 * On FRDM-i.MX93: replace with GStreamer/MIPI-CSI pipeline.
 */
class CameraSource {
public:
    CameraSource(int deviceId, const std::string& zone)
        : m_deviceId(deviceId), m_zone(zone) {}

    bool open() {
        if (!m_cap.open(m_deviceId)) {
            std::cerr << "[CAMERA] Cannot open device " << m_deviceId << "\n";
            return false;
        }
        m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        std::cout << "[CAMERA] Opened device " << m_deviceId
                  << " (" << m_cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                  << m_cap.get(cv::CAP_PROP_FRAME_HEIGHT) << ")\n";
        return true;
    }

    bool hasNext() const {
        return m_cap.isOpened();
    }

    InputFrame next() {
        InputFrame frame;
        m_cap >> m_frame;
        if (!m_frame.empty()) {
            m_frameIdx++;
            frame.imagePath = "";  // live frame, no file path
            frame.imageName = "camera_frame_" + std::to_string(m_frameIdx);
            frame.currentZone = m_zone;
            frame.sceneDesc = "USB Camera Live";
            // Store the frame data for retrieval
            m_lastFrame = m_frame;
        }
        return frame;
    }

    cv::Mat getFrame() const { return m_lastFrame; }
    int size() const { return -1; }  // infinite

    void setZone(const std::string& z) { m_zone = z; }

private:
    cv::VideoCapture m_cap;
    int m_deviceId;
    std::string m_zone;
    cv::Mat m_frame;
    cv::Mat m_lastFrame;
    int m_frameIdx = 0;
};
