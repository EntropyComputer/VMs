#include "vm.hpp"
#include <iostream>
#include <libvirt/libvirt.h>
#include <cstdlib>       // For system()
#include <unistd.h>      // For access() and unlink()
#include <filesystem>
#include <sstream>
#include <chrono>
#include <thread>

bool copyOvmfFile(const std::string& source, const std::string& destination);

bool vm_spinUp(const std::string& vmName, int memoryMB, int vcpus, const std::string& diskPath) {
  // Step 1: Connect to the hypervisor.
  virConnectPtr connection = virConnectOpen("qemu:///system");
  if (!connection) {
    std::cerr << "Failed to connect to hypervisor" << std::endl;
    return false;
  }

  // Step 2
  if (!std::filesystem::exists(diskPath)) {
    // Creates a qcow2 overlay disk for each user, based on the golden image
    // All writes and changes made by the user are stored in their personal overlay disk.
    // This is automatic. Every time the Windows VM does a "write" operation, QEMU/KVM intercepts it
    // and stores the change in the user's qcow2 file.
    std::cout << "Creating new qcow2 disk: " << diskPath << std::endl;
    std::string goldenImagePath = "/var/lib/libvirt/images/golden-windows11.qcow2";
    std::ostringstream createImageCmd;
    createImageCmd << "qemu-img create -f qcow2 -b " << goldenImagePath << " -F qcow2 " << diskPath << " 60G";
    if (system(createImageCmd.str().c_str()) != 0) {
      std::cerr << "Failed to create qcow2 overlay image" << std::endl;
      virConnectClose(connection);
      return false;
    }
  } else {
    // if the golden image has been instantiated for the user, use the version we already made
    std::cout << "Using existing persistent disk: " << diskPath << std::endl;
  }

  // Step 4: Prepare the OVMF NVRAM file for UEFI boot.
  std::string ovmfSource      = "/usr/share/OVMF/OVMF_VARS_4M.fd";
  std::string nvramDir        = "/home/jjquaratiello/nvram/";
  std::string ovmfDestination = nvramDir + vmName + "_VARS.fd";

  if (!std::filesystem::exists(nvramDir)) {
    std::cerr << "Error: NVram directory " << nvramDir 
              << " does not exist. Please create it before running the application." << std::endl;
    virConnectClose(connection);
    return false;
  }

  if (!copyOvmfFile(ovmfSource, ovmfDestination)) {
    std::cerr << "Failed to prepare the OVMF NVRAM file for VM: " << vmName << std::endl;
    virConnectClose(connection);
    return false;
  }

  // Step 5: Construct the XML configuration.
  // Note: The XML below attaches a CDROM device (pointing to a Windows ISO)
  // which you can use to install drivers if needed, and sets up secure boot, TPM, etc.

  // xml is all attributes
  // std::string windowsIsoPath = "/var/lib/libvirt/images/windows11.iso";
  std::string vmXML = std::string(R"XML(<domain type='kvm'>
    <name>)XML") + vmName + std::string(R"XML(</name>
    <memory unit='MiB'>)XML") + std::to_string(memoryMB) + std::string(R"XML(</memory>
    <vcpu>)XML") + std::to_string(vcpus) + std::string(R"XML(</vcpu>  
    <memoryBacking>
      <hugepages/>
      <allocation mode='immediate'/>
    </memoryBacking>
    <memtune>
      <hard_limit unit='MiB'>)XML") + std::to_string(memoryMB) + std::string(R"XML(</hard_limit>
    </memtune>
    <os>
      <type arch='x86_64' machine='pc-q35-5.2'>hvm</type>
      <loader readonly='yes' type='pflash' secure='yes'>/usr/share/OVMF/OVMF_CODE_4M.secboot.fd</loader>
      <nvram>)XML") + ovmfDestination + std::string(R"XML(</nvram>
      <boot dev='cdrom' order='1'/>
      <boot dev='hd' order='2'/>
    </os>
    <features>
      <acpi/>
      <apic/>
      <smm state='on'/>
      <hyperv>
        <relaxed state='on'/>
        <vapic state='on'/>
        <spinlocks state='on' retries='8191'/>
      </hyperv>
    </features>
    <cpu mode='host-passthrough' check='none'>
      <topology sockets='1' dies='1' cores=')XML") + std::to_string(vcpus) + std::string(R"XML(' threads='1'/>
      <cache mode='passthrough'/>
    </cpu>
    <devices>
      <disk type='file' device='disk'>
        <driver name='qemu' type='qcow2' cache='none' io='native' discard='unmap'/>
        <source file=")XML") + diskPath + std::string(R"XML("/>
        <target dev='vda' bus='virtio'/>
      </disk>
      <interface type='network'>
        <source network='default'/>
        <model type='virtio'/>
        <driver name='vhost' queues='4'/>
      </interface>
      <video>
        <model type='qxl' ram='262144' vram='262144' vgamem='32768' heads='1'>
          <acceleration accel3d='no'/>
        </model>
      </video>
      <graphics type='spice' autoport='yes' listen='0.0.0.0'>
        <listen type='address' address='0.0.0.0'/>
      </graphics>
      <input type='keyboard' bus='usb'/>
      <input type='tablet' bus='usb'/> 
      <tpm model='tpm-tis'>
        <backend type='emulator'/>
      </tpm>
    </devices>
  </domain>)XML");

  // Step 6: Define the VM persistently.
  std::cout << "Defining the VM in libvirt..." << std::endl;
  virDomainPtr vm = virDomainDefineXML(connection, vmXML.c_str());
  if (!vm) {
    std::cerr << "Failed to define VM. Check the XML configuration." << std::endl;
    virConnectClose(connection);
    return false;
  }

  // Step 7: Start the VM.
  if (virDomainCreate(vm) < 0) {
    std::cerr << "Failed to start VM. Check QEMU logs for details." << std::endl;
    virDomainFree(vm);
    virConnectClose(connection);
    return false;
  }
  
  std::cout << "VM started successfully: " << vmName << std::endl;

  // Return the vm
  return vm;

  // OLD: This cleans up libvirt objects, but we need those
  // virDomainFree(vm);
  // virConnectClose(connection);

  return true;
}


// Helper function: Copy the OVMF variables file to a writable location.
bool copyOvmfFile(const std::string& source, const std::string& destination) {
  try {
    std::filesystem::copy_file(source, destination,
        std::filesystem::copy_options::overwrite_existing);
    return true;
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Error copying OVMF file: " << e.what() << std::endl;
    return false;
  }
}

