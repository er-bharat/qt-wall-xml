#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

const int SECONDS_IN_DAY = 86400;
const int TRANSITION_DURATION = 1800; // 30 minutes

bool isImageFile(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
}

int main() {
    std::string folderPath;
    std::cout << "Enter the full path to the folder containing images: ";
    std::getline(std::cin, folderPath);

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        std::cerr << "Invalid directory path.\n";
        return 1;
    }

    std::vector<fs::path> images;

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file() && isImageFile(entry.path())) {
            images.push_back(entry.path());
        }
    }

    if (images.size() < 2) {
        std::cerr << "Need at least 2 images to create dynamic wallpaper XML.\n";
        return 1;
    }

    std::sort(images.begin(), images.end());

    int totalImages = images.size();
    int totalTransitionTime = totalImages * TRANSITION_DURATION;
    int staticDuration = (SECONDS_IN_DAY - totalTransitionTime) / totalImages;

    std::string xmlPath = folderPath + "/dynamic_wallpaper.xml";
    std::ofstream xmlFile(xmlPath);
    if (!xmlFile.is_open()) {
        std::cerr << "Failed to write XML to: " << xmlPath << "\n";
        return 1;
    }

    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xmlFile << "<background>\n";
    xmlFile << "  <starttime>\n";
    xmlFile << "    <year>2001</year>\n";
    xmlFile << "    <month>1</month>\n";
    xmlFile << "    <day>1</day>\n";
    xmlFile << "    <hour>0</hour>\n";
    xmlFile << "    <minute>0</minute>\n";
    xmlFile << "    <second>0</second>\n";
    xmlFile << "  </starttime>\n\n";

    for (size_t i = 0; i < images.size(); ++i) {
        const std::string currentFile = images[i].string();
        const std::string nextFile = images[(i + 1) % images.size()].string();

        xmlFile << "  <static>\n";
        xmlFile << "    <duration>" << staticDuration << "</duration>\n";
        xmlFile << "    <file>" << currentFile << "</file>\n";
        xmlFile << "  </static>\n";
        xmlFile << "  <transition type=\"overlay\">\n";
        xmlFile << "    <duration>" << TRANSITION_DURATION << "</duration>\n";
        xmlFile << "    <from>" << currentFile << "</from>\n";
        xmlFile << "    <to>" << nextFile << "</to>\n";
        xmlFile << "  </transition>\n\n";
    }

    xmlFile << "</background>\n";
    xmlFile.close();

    std::cout << "Dynamic wallpaper XML created at: " << xmlPath << "\n";
    return 0;
}
