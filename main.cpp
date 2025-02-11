#include <iostream>
#include <libvirt/libvirt.h>
#include "vm.hpp"       // Contains vm_spinUp declaration.
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <stdexcept>    // For std::invalid_argument, std::runtime_error
#include <cstdlib>      // For system()
#include <string>

int main(int argc, char* argv[]) {
    // Check that at least one argument (the command) is provided.
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " spin_up <vmName> <memoryMB> <vcpus> <diskPath>\n"
                  << "  " << argv[0] << " start <vmName>\n";
        return 1;
    }

    // Open connection to the hypervisor.
    virConnectPtr conn = virConnectOpen("qemu:///system");
    if (!conn) {
        std::cerr << "Failed to connect to hypervisor" << std::endl;
        return 1;
    }

    // Get the command.
    std::string command = argv[1];

    if (command == "spin_up") {
      // Since diskPath is automatically constructed, we only require 4 arguments:
      // <program> spin_up <vmName> <memoryMB> <vcpus>
      if (argc < 5) {
          std::cerr << "Usage: " << argv[0] << " spin_up <vmName> <memoryMB> <vcpus>" << std::endl;
          virConnectClose(conn);
          return 1;
      }
  
      std::string vmName = argv[2];
      int memoryMB = std::stoi(argv[3]);  // Convert argument to integer.
      int vcpus = std::stoi(argv[4]);       // Convert argument to integer.
  
      // Automatically construct the disk path using the vmName.
      std::string diskPath = "/var/lib/libvirt/images/" + vmName + ".qcow2";
  
      // Display VM configuration.
      std::cout << "VM Configuration:" << std::endl;
      std::cout << "Name: " << vmName << std::endl;
      std::cout << "Memory: " << memoryMB << " MB" << std::endl;
      std::cout << "vCPUs: " << vcpus << std::endl;
      std::cout << "Disk Path: " << diskPath << std::endl;
  
      // Check if a domain with this name already exists.
      virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
      if (dom) {
          virDomainFree(dom);
          virConnectClose(conn);
          throw std::invalid_argument("Tried to spin up a VM that already exists. Use the \"start\" command instead.");
      }
  
      // Attempt to create and start the VM.
      if (vm_spinUp(vmName, memoryMB, vcpus, diskPath)) {
          std::cout << "Successfully created and started the VM: " << vmName << std::endl;
      } else {
          std::cerr << "Failed to create and start the VM: " << vmName << std::endl;
          virConnectClose(conn);
          return 1;
      }
      virConnectClose(conn);
      return 0;
  }
    else if (command == "start") {
      // Check for enough arguments.
      if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " start <vmName>" << std::endl;
        virConnectClose(conn);
        return 1;
      }

      // Build and run the "virsh start" command.
      std::string vmName = argv[2];
      std::ostringstream startVM;
      startVM << "virsh start " << vmName;
      if (system(startVM.str().c_str()) != 0) {
        virConnectClose(conn);
        throw std::runtime_error("Failed to start VM: " + vmName);
      }
      std::cout << "VM " << vmName << " started (resumed) successfully." << std::endl;
      virConnectClose(conn);
      return 0;
    } 
    else if (command == "stop") {
      if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " start <vmName>" << std::endl;
        virConnectClose(conn);
        return 1;
      }

      std::string vmName = argv[2];
      std::ostringstream stopVM;
      stopVM << "virsh managedsave " << vmName;
      if (system(stopVM.str().c_str()) != 0) {
        virConnectClose(conn);
        throw std::runtime_error("Failed to stop VM: " + vmName);
      }
      std::cout << "VM " << vmName << " stopped successfully." << std::endl;
      virConnectClose(conn);
      return 0;
    }
    else if (command == "open") {
      if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " open <vmName>" << std::endl;
        virConnectClose(conn);
        return 1;
      }

      std::string vmName = argv[2];

      const char* home = getenv("HOME");
      const char* display = getenv("DISPLAY");
      const char* xauth = getenv("XAUTHORITY");  // Get the XAUTHORITY variable
      
      std::ostringstream openVM;
      openVM << "env -i HOME=\"" << (home ? home : "") << "\" "
             << "PATH=\"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\" "
             << "DISPLAY=\"" << (display ? display : "") << "\" "
             << "XAUTHORITY=\"" << (xauth ? xauth : (std::string(home ? home : "") + "/.Xauthority")) << "\" "
             << "remote-viewer spice://localhost:5900";
      
      
      if (system(openVM.str().c_str()) != 0) {
        virConnectClose(conn);
        throw std::runtime_error("Failed to open VM: " + vmName);
      }
      std::cout << "VM " << vmName << " opened successfully." << std::endl;
      virConnectClose(conn);
      return 0;
    }
    else {
        virConnectClose(conn);
        throw std::invalid_argument("Unrecognized command: " + command);
    }

    return 0;
}
