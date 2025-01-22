#ifndef VM
#define VM

#include <string>

// Function to create and start a virtual machine
// Parameters:
// - vmName: Name of the virtual machine
// - memoryMB: Amount of memory (in MB) to allocate to the VM
// - vcpus: Number of virtual CPUs for the VM
// - diskPath: Path to the disk image file (qcow2 or raw format)
// Returns:
// - true if the VM is successfully created and started
// - false otherwise
bool vm_spinUp(const std::string& vmName, int memoryMB, int vcpus, const std::string& diskPath);

#endif 
