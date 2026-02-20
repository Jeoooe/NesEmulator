#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

class CPULogChecker {
private:
    std::vector<std::pair<uint16_t, int>> logData;
    size_t currentIndex = 0;
    bool loadded = false;
    
public:
    bool loadLogFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::string line;
        logData.clear();
        currentIndex = 0;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            // 提取地址
            uint16_t addr = std::stoul(line.substr(0, 4), nullptr, 16);
            
            // 提取周期
            size_t cycPos = line.find("CYC:");
            if (cycPos != std::string::npos) {
                int cycle = std::stoi(line.substr(cycPos + 4));
                logData.push_back({addr, cycle});
            }
        }
        loadded = !logData.empty();
        return loadded;
    }
    
    // 输入当前执行的地址和周期数，返回是否匹配
    bool check(uint16_t address, int cycle) {
        if (currentIndex >= logData.size()) {
            return false;  // 执行了多余的指令
        }
        
        bool match = (logData[currentIndex].first == address && 
                     logData[currentIndex].second == cycle);
        
        currentIndex++;
        return match;
    }
    
    bool isComplete() const {
        return currentIndex == logData.size();
    }
    
    void reset() {
        currentIndex = 0;
        loadded = false;
    }

    bool is_loadded() const {
        return loadded;
    }
};