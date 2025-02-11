#include "vm.hpp"
#include <iostream>
#include <libvirt/libvirt.h>
#include <cstdlib>       // For system()
#include <unistd.h>      // For access() and unlink()
#include <filesystem>

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

bool vm_spinUp(const std::string& vmName, int memoryMB, int vcpus, const std::string& diskPath) {
    // Step 1: Connect to the hypervisor.
    virConnectPtr connection = virConnectOpen("qemu:///system");
    if (!connection) {
        std::cerr << "Failed to connect to hypervisor" << std::endl;
        return false;
    }

    // Step 2: Check if the disk image already exists; if so, remove it.
    if (access(diskPath.c_str(), F_OK) == 0) {
        std::cout << "Disk image already exists. Removing: " << diskPath << std::endl;
        if (unlink(diskPath.c_str()) != 0) {
            std::cerr << "Failed to remove existing disk image" << std::endl;
            virConnectClose(connection);
            return false;
        }
    }

    // Step 3: Create a new empty qcow2 disk (e.g. 40G).
    // QCOW = virtual disk make by QEMU
    std::cout << "Creating a new qcow2 disk (40G): " << diskPath << std::endl;
    std::string createImageCmd = "qemu-img create -f qcow2 " + diskPath + " 60G";
    if (system(createImageCmd.c_str()) != 0) {
        std::cerr << "Failed to create qcow2 image" << std::endl;
        virConnectClose(connection);
        return false;
    }

    // Step 4: Prepare the OVMF NVRAM file for UEFI boot.
    // OVMF Open Virtual Machine Firmware
    // a type of computer memory that keeps its data even when the computer is off
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
    // Note: The <input type='tablet' bus='usb'/> entry provides absolute pointer support,
    // which is recommended to reduce mouse lag in SPICE sessions.
    // Other potential optimizations:
    //   - Using virtio for disk and network devices (if the guest OS supports them) can improve performance.
    //   - Ensure that the guest has the proper drivers installed (e.g., QXL and SPICE guest tools) for best display performance.
    std::string windowsIsoPath = "/var/lib/libvirt/images/windows11.iso";
    std::string vmXML =
        std::string(R"XML(<domain type='kvm'>
  <name>)XML") + vmName +
        std::string(R"XML(</name>
  <memory unit='MiB'>)XML") + std::to_string(memoryMB) +
        std::string(R"XML(</memory>
  <vcpu>)XML") + std::to_string(vcpus) +
        std::string(R"XML(</vcpu>
  <os>
    <type arch='x86_64' machine='pc-q35-5.2'>hvm</type>
    <loader readonly='yes' type='pflash' secure='yes'>/usr/share/OVMF/OVMF_CODE_4M.secboot.fd</loader>
    <nvram>)XML") + ovmfDestination +
        std::string(R"XML(</nvram>
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
  <cpu mode='host-passthrough'/>
  <devices>
    <disk type='file' device='cdrom'>
      <driver name='qemu' type='raw'/>
      <source file=")XML") + windowsIsoPath +
        std::string(R"XML("/>
      <target dev='sdb' bus='sata'/>
      <readonly/>
    </disk>
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2'/>
      <source file=")XML") + diskPath +
        std::string(R"XML("/>
      <target dev='sda' bus='sata'/>
    </disk>
    <interface type='network'>
      <source network='default'/>
    </interface>
    <video>
      <model type='qxl' ram='65536' vram='65536' vgamem='16384' heads='1'>
        <acceleration accel3d='no'/>
      </model>
    </video>
    <graphics type='spice' autoport='yes' listen='0.0.0.0'>
      <listen type='address' address='0.0.0.0'/>
    </graphics>
    <input type='keyboard' bus='usb'/>
    <input type='tablet' bus='usb'/>  <!-- Changed from 'mouse' to 'tablet' for absolute pointer support -->
    <tpm model='tpm-tis'>
      <backend type='emulator'/>
    </tpm>
  </devices>
</domain>
)XML");

    // Step 6: Define the VM persistently.
    std::cout << "Defining the VM in libvirt..." << std::endl;
    virDomainPtr vm = virDomainDefineXML(connection, vmXML.c_str());
    if (!vm) {
        std::cerr << "Failed to define VM. Check the XML configuration." << std::endl;
        virConnectClose(connection);
        return false;
    }

    // Step 7: Start the VM.
    std::cout << "Starting the VM: " << vmName << std::endl;
    if (virDomainCreate(vm) < 0) {
        std::cerr << "Failed to start VM. Check QEMU logs for details." << std::endl;
        virDomainFree(vm);
        virConnectClose(connection);
        return false;
    }

    std::cout << "VM started successfully: " << vmName << std::endl;

    // Cleanup libvirt objects.
    virDomainFree(vm);
    virConnectClose(connection);

    return true;
}




