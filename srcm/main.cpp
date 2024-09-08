import chocobot.config;
import chocobot.branding;

#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    std::cout << chocobot::branding::project_home << std::endl;
    chocobot::config c;
    std::cout << c.bugreport_command << std::endl;
    return 0;
}
