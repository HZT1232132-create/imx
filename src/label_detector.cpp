#include "label_detector.h"

RoiResult LabelDetector::detect(const cv::Mat& image) {
    RoiResult r;
    if (image.empty()) return r;

    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    int w = gray.cols;
    int h = gray.rows;

    r.qrRoi = findQrRegion(gray);
    r.textRoi = findTextRegion(gray, r.qrRoi);

    // Full label = union of qr + text regions
    int lx = std::min(r.qrRoi.x, r.textRoi.x);
    int ly = std::min(r.qrRoi.y, r.textRoi.y);
    int rx = std::max(r.qrRoi.x + r.qrRoi.width, r.textRoi.x + r.textRoi.width);
    int by = std::max(r.qrRoi.y + r.qrRoi.height, r.textRoi.y + r.textRoi.height);
    lx = std::max(0, lx - 10);
    ly = std::max(0, ly - 10);
    rx = std::min(w, rx + 10);
    by = std::min(h, by + 10);
    r.labelRoi = cv::Rect(lx, ly, rx - lx, by - ly);

    if (r.labelRoi.width > 20 && r.labelRoi.height > 20) {
        r.qrPatch = gray(r.qrRoi).clone();
        r.textPatch = gray(r.textRoi).clone();
        r.labelPatch = gray(r.labelRoi).clone();
        r.valid = true;
    }

    return r;
}

cv::Rect LabelDetector::findQrRegion(const cv::Mat& gray) {
    int w = gray.cols;
    int h = gray.rows;

    // QR codes are typically in the right portion of the label.
    // V1 heuristic: right 30%, middle 60% vertical.
    int qx = w * 2 / 3;
    int qy = h / 4;
    int qw = w - qx;
    int qh = h / 2;

    // Try to refine using contour detection
    cv::Mat edges;
    cv::Canny(gray, edges, 30, 100);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Look for square-ish contour in the right portion
    for (const auto& cnt : contours) {
        cv::Rect br = cv::boundingRect(cnt);
        if (br.x < qx - 20 || br.width < 20 || br.height < 20) continue;
        if (br.width < br.height * 0.5 || br.width > br.height * 2.5) continue;
        if (br.area() < 400 || br.area() > w * h * 0.3) continue;
        qx = br.x - 5;
        qy = br.y - 5;
        qw = br.width + 10;
        qh = br.height + 10;
        break;
    }

    qx = std::max(0, qx);
    qy = std::max(0, qy);
    qw = std::min(w - qx, qw);
    qh = std::min(h - qy, qh);

    return cv::Rect(qx, qy, qw, qh);
}

cv::Rect LabelDetector::findTextRegion(const cv::Mat& gray, const cv::Rect& qrRoi) {
    int w = gray.cols;
    int h = gray.rows;

    // Text is typically left of QR code, or bottom portion
    int tx = 5;
    int ty = h / 4;
    int tw = qrRoi.x > 10 ? qrRoi.x - 10 : w / 2;
    int th = h - ty;

    // If QR takes up most of the image, text might be at bottom
    if (tw < w * 0.2) {
        ty = h * 2 / 3;
        tw = w;
        th = h - ty;
    }

    tx = std::max(0, tx);
    ty = std::max(0, ty);
    tw = std::min(w - tx, tw);
    th = std::min(h - ty, th);

    return cv::Rect(tx, ty, tw, th);
}
