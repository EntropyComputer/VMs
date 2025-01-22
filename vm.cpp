#include "vm.hpp"
#include <iostream>
#include <libvirt/libvirt.h>

bool vm_spinUp(const std::string& vmName, int memoryMB, int vcpus, const std::string& diskPath) {
  // Step 1: Connect to the hypervisor
	virConnectPtr connection = virConnectOpen("qemu:///system");
	if (!connection) {
		std::cerr << "Failed to connect to hypervisor" << std::endl;
		return false;
	}

	// Step 2: Dynamically generate the XML configuration
  std::string vmXML = R"(
    <domain type='kvm'>
      <name>)" + vmName + R"(</name>
      <memory unit='MiB'>)" + std::to_string(memoryMB) + R"(</memory>
      <vcpu>)" + std::to_string(vcpus) + R"(</vcpu>
      <os>
        <type arch='x86_64' machine='pc-i440fx-5.2'>hvm</type>
        <boot dev='hd'/>
      </os>
      <devices>
        <disk type='file' device='disk'>
          <driver name='qemu' type='qcow2'/>
          <source file=')" + diskPath + R"('/>
          <target dev='vda' bus='virtio'/>
        </disk>
        <interface type='network'>
          <source network='default'/>
        </interface>
      </devices>
    </domain>
  )";

	// Step 3: Define the VM
  // Standard libvirt functions
	virDomainPtr vm = virDomainDefineXML(connection, vmXML.c_str());
	if (!vm) {
		std::cerr << "Failed to define VM" << std::endl;
		virConnectClose(connection);
		return false;
	}

	// Step 4: Start the VM
	if (virDomainCreate(vm) < 0) {
		std::cerr << "Failed to start VM" << std::endl;
		virDomainFree(vm);
		virConnectClose(connection);
		return false;
	}

	std::cout << "VM started successfully" << std::endl;

	// Step 5: Cleanup
	virDomainFree(vm);
	virConnectClose(connection);

	return true;
}


