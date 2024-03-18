#include <cstdio>
#include <vector>
#include <algorithm>
#include <array>

namespace {
  std::vector<char> buffer;

  constexpr std::size_t BUFFER_SIZE = 100;

  std::array seek_to{
    27455,
    20549628,
    79341625,
    103058720,
    149879108,
    183031340,
  };
}

int main(const char** argv, int argc) {
  FILE* reading = fopen("C:\\Program Files (x86)\\World of Warcraft\\_retail_\\Logs\\WoWCombatLog-031824_103509.txt", "rb");
  
  buffer.resize(BUFFER_SIZE+1,'\0');
  
  for (auto const& seeking : seek_to) {
    fseek(reading, std::max<std::size_t>(seeking, 0), SEEK_SET);

    fread(buffer.data(), sizeof(char), BUFFER_SIZE, reading);

    fprintf(stdout, "%s\n---\n", buffer.data());
  }
}