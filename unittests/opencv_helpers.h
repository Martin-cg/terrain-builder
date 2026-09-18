#pragma once

#include <cstdint>

#include <opencv2/opencv.hpp>

// An image whose blue and green channels rise with x and y..
inline cv::Mat make_gradient(const uint32_t width, const uint32_t height) {
    cv::Mat image(height, width, CV_8UC3);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t blue = (x * 255) / width;
            const uint8_t green = (y * 255) / height;
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(blue, green, 128);
        }
    }

    return image;
}

// Whether `a` and `b` have the same size and type, and every channel's mean absolute difference stays below `tolerance`.
inline bool mat_are_close(const cv::Mat &a, const cv::Mat &b, const double tolerance) {
    if (a.size() != b.size() || a.type() != b.type()) {
        return false;
    }

    cv::Mat difference;
    cv::absdiff(a, b, difference);
    const cv::Scalar mean_difference = cv::mean(difference);
    for (int channel = 0; channel < a.channels(); channel++) {
        if (mean_difference[channel] >= tolerance) {
            return false;
        }
    }

    return true;
}

// modified from https://stackoverflow.com/a/32440830/6304917
inline bool mat_equals(const cv::Mat mat1, const cv::Mat mat2) {
    if (mat1.dims != mat2.dims ||
        mat1.size != mat2.size ||
        mat1.elemSize() != mat2.elemSize()) {
        return false;
    }

    if (mat1.isContinuous() && mat2.isContinuous()) {
        return std::memcmp(mat1.ptr(), mat2.ptr(), mat1.total() * mat1.elemSize()) == 0;
    } else {
        const cv::Mat *arrays[] = {&mat1, &mat2, 0};
        uchar *ptrs[2];
        cv::NAryMatIterator it(arrays, ptrs, 2);
        for (unsigned int p = 0; p < it.nplanes; p++, ++it) {
            if (memcmp(it.ptrs[0], it.ptrs[1], it.size * mat1.elemSize()) != 0) {
                return false;
            }
        }

        return true;
    }
}
