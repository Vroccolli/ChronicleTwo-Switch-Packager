#include "platform/legacy_encoding.hpp"
#include <iostream>
#include <string>
int main() {
    std::string line;
    while (std::getline(std::cin,line)) {
        try {
            std::string input;
            for (size_t i=1;i+1<line.size();i+=2) input+=static_cast<char>(std::stoi(line.substr(i,2),nullptr,16));
            std::optional<std::string> output;
            if (line[0]=='E') output=legacy_encoding::GameText(input);
            else output=legacy_encoding::ReadableText(input);
            if (!output) { std::cout<<"ERROR\n"; continue; }
            for (auto c:*output) { unsigned value=static_cast<unsigned char>(c); std::cout<<"0123456789abcdef"[value>>4]<<"0123456789abcdef"[value&15]; }
            std::cout<<'\n';
        } catch (const std::exception &) { std::cout<<"ERROR\n"; }
    }
}
