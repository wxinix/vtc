/*!
C++ Virtualization Library of Traffic Control Cabinet
SDLC Frame Logger Utility

Copyright (C) 2023  Wuping Xin

MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#include <iostream>
#include <vtc/vtc.hpp>

int main()
{
  // Set up the logger to log SDLC frames.
  vtc::setup_logger(vtc::fs::current_path(), "sdlclogger");

  // Using the first SDLC device card.
  auto device = std::make_unique<vtc::serial::device::SerialDevice>("1");

  if (!device->ready()) {
    std::cout << "Device is not ready. Error message: " << device->err_what() << std::endl;
    return -1;
  }

  auto sdlc_enabled{true};

  std::thread(
      [&]() {
        while (sdlc_enabled) {
          auto count = device->read(vtc::serial::buffer);
          auto frame_str = vtc::BytesToHexStr(vtc::serial::buffer.data(), count);
          // The first byte is SDLC address, the second SDLC control byte, always 0x83, and the third FrameID.
          vtc::logger()->info("FrameID {} Addr {}: {}", vtc::serial::buffer[2], vtc::serial::buffer[0], frame_str);
        }
      }
  ).detach();

  std::cout << "Press any key to terminate..." << std::endl;
  std::cin.get();
  sdlc_enabled = false;
  std::cout << "Stopping logging SDLC frames...." << std::endl;
  return 0;
}
