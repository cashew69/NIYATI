#include "engine/effects/vclouds/vcloud_noise.cpp"
#include <iostream>
#include <iomanip>

// Mock Logger
void Logger_Log(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

int main() {
    std::cout << "Testing Cloud Noise Functions..." << std::endl;
    
    float x = 0.5f, y = 0.5f, z = 0.5f;
    int freq = 4;
    
    float worley = vcVCloud_Worley(x, y, z, freq);
    std::cout << "Worley(0.5, 0.5, 0.5, 4): " << std::fixed << std::setprecision(4) << worley << std::endl;
    
    float alligator = vcVCloud_Alligator(x, y, z, freq);
    std::cout << "Alligator(0.5, 0.5, 0.5, 4): " << alligator << std::endl;
    
    vec3 curl = vcVCloud_Curl(x, y, z, 2.0f, 2);
    std::cout << "Curl(0.5, 0.5, 0.5, 2.0, 2): [" << curl.x << ", " << curl.y << ", " << curl.z << "]" << std::endl;
    
    float curlyAlligator = vcVCloud_CurlyAlligator(x, y, z, freq, 0.5f);
    std::cout << "CurlyAlligator(0.5, 0.5, 0.5, 4, 0.5): " << curlyAlligator << std::endl;

    if (worley >= 0.0f && worley <= 1.0f && alligator >= 0.0f && alligator <= 1.0f) {
        std::cout << "SUCCESS: Noise values in expected range [0, 1]" << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE: Noise values out of range!" << std::endl;
        return 1;
    }
}
