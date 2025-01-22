#include <iostream>
#include "vm.hpp" // Include the header file for VM creation

int main() {
  // VM configuration parameters
  std::string vmName = "dynamic-vm";           // Name of the virtual machine
  int memoryMB = 2048;                         // Allocate 2 GB of memory
  int vcpus = 2;                               // Use 2 virtual CPUs
  std::string diskPath = "/var/lib/libvirt/images/dynamic-vm.qcow2"; // Path to disk image

  // Attempt to create and start the VM
  if (vm_spinUp(vmName, memoryMB, vcpus, diskPath)) {
    std::cout << "Successfully created and started the VM: " << vmName << std::endl;
  } else {
    std::cerr << "Failed to create and start the VM: " << vmName << std::endl;
  }

  return 0;
}
