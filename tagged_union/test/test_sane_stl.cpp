#include <map>
#include <string>
#include <iostream>

int main(void) {
    std::map<std::string,int> map;
    const int val = 123;
    map.insert(std::pair<std::string,int>("kaka", val));
    if (map.count("kaka") != 1) {
        std::cerr << "wrong count" << std::endl;
        return 1;
    }
    for (auto it = map.find("kaka"); it != map.end(); it++) {
        std::cout << "map.find(\"kaka\"): " << it->second << std::endl;
        if (val != it->second) {
            std::cerr << "wrong value" << std::endl;
            return 1;
        }
    }
    std::cout << "Great Success!" << std::endl;
    return 0;
}
