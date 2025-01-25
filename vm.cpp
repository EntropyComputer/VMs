#include "vm.hpp"
#include <iostream>
#include <fstream>
#include <libvirt/libvirt.h>
#include <cstdlib>   // For system()
#include <unistd.h>  // For access(), unlink()

bool vm_spinUp(const std::string& vmName, int memoryMB, int vcpus, const std::string& diskPath) {
  // Step 1: Connect to the hypervisor
  virConnectPtr connection = virConnectOpen("qemu:///system");
  if (!connection) {
    std::cerr << "Failed to connect to hypervisor" << std::endl;
    return false;
  }

  // Step 2: If the VM’s disk image already exists, remove it
  if (access(diskPath.c_str(), F_OK) == 0) {
    if (unlink(diskPath.c_str()) != 0) {
      std::cerr << "Failed to remove existing disk image" << std::endl;
      virConnectClose(connection);
      return false;
    }
  }

  // Step 3: Create a new qcow2 image for the VM (20GB, based on ubuntu-preinstalled.qcow2)
  std::string baseImagePath = "/var/lib/libvirt/images/ubuntu-preinstalled.qcow2";
  std::string createImageCmd = "qemu-img create -f qcow2 -F qcow2 -b " + baseImagePath + " " + diskPath + " 20G";
  if (system(createImageCmd.c_str()) != 0) {
    std::cerr << "Failed to create qcow2 image" << std::endl;
    virConnectClose(connection);
    return false;
  }

  // ------------------------------------------------------------------
  //  Generate cloud-init seed ISO with user-data and meta-data
  // ------------------------------------------------------------------

  // Temporary file paths (unique per VM name)
  std::string userDataPath = "/tmp/user-data-" + vmName;
  std::string metaDataPath = "/tmp/meta-data-" + vmName;
  std::string isoPath      = "/tmp/seed-" + vmName + ".iso";

  // The SHA-512 hash below corresponds to the password "ubuntu".
  // Generated via:  mkpasswd -m sha-512 'ubuntu'
  // Please note: This is just an example. For real deployments, consider stronger credentials.
  std::string hashedUbuntuPassword = "$6$OEtKBfPUc9z1.3zh$BvDSIJzSmmPvmfGHN2nsBUA8t5kJgohJqa5Zad5X4iYND2tycDXcmWnV0mG2qib2DrtOorZ6qJjJHfvdzvoC1/";

  // 1) user-data (cloud-config) for "ubuntu"/"ubuntu"
  std::string userData = R"(#cloud-config

users:
  - name: ubuntu
    groups: sudo
    shell: /bin/bash
    lock_passwd: false
    passwd: )" + hashedUbuntuPassword + R"(
    sudo: ALL=(ALL) NOPASSWD:ALL

ssh_pwauth: true
chpasswd:
  expire: false
)";

  // 2) meta-data (just minimal info: instance-id, hostname)
  std::string metaData = "instance-id: " + vmName + "\n"
                         "local-hostname: " + vmName + "\n";

  // Write user-data to file
  {
    std::ofstream udFile(userDataPath);
    if (!udFile) {
      std::cerr << "Failed to create user-data file" << std::endl;
      virConnectClose(connection);
      return false;
    }
    udFile << userData;
  }

  // Write meta-data to file
  {
    std::ofstream mdFile(metaDataPath);
    if (!mdFile) {
      std::cerr << "Failed to create meta-data file" << std::endl;
      virConnectClose(connection);
      return false;
    }
    mdFile << metaData;
  }

  // Build the seed ISO with volume label "cidata"
  // Requires genisoimage or mkisofs. Install with "sudo apt-get install genisoimage"
  std::string buildIsoCmd = "genisoimage -output " + isoPath +
                            " -volid cidata -joliet -rock " +
                            userDataPath + " " + metaDataPath;
  if (system(buildIsoCmd.c_str()) != 0) {
    std::cerr << "Failed to create cloud-init ISO" << std::endl;
    virConnectClose(connection);
    return false;
  }

  // ---------------------------------------------------
  // Step 4: Dynamically generate the VM domain XML
  // ---------------------------------------------------
  // Attach both the primary disk (QCOW2) and the seed ISO (CD-ROM),
  // using VNC for graphics.

  std::string vmXML = R"(
<domain type='kvm'>
  <name>)" + vmName + R"(</name>
  <memory unit='MiB'>)" + std::to_string(memoryMB) + R"(</memory>
  <vcpu>)" + std::to_string(vcpus) + R"(</vcpu>
  <os>
    <type arch='x86_64' machine='pc-i440fx-5.2'>hvm</type>
    <boot dev='hd'/>
    <bootmenu enable='yes'/>
  </os>
  <devices>
    <!-- Main QCOW2 disk -->
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2'/>
      <source file=')" + diskPath + R"(' />
      <target dev='vda' bus='virtio'/>
    </disk>

    <!-- Cloud-init seed ISO (readonly) -->
    <disk type='file' device='cdrom'>
      <driver name='qemu' type='raw'/>
      <source file=')" + isoPath + R"(' />
      <target dev='hdc' bus='ide'/>
      <readonly/>
    </disk>

    <interface type='network'>
      <source network='default'/>
    </interface>

    <video>
      <model type='qxl' ram='65536' vram='65536' vgamem='16384' heads='1'>
        <acceleration accel3d='no'/>
      </model>
      <driver name='qemu'/>
    </video>

    <graphics type='vnc' autoport='yes' listen='0.0.0.0'>
      <listen type='address' address='0.0.0.0'/>
    </graphics>
  </devices>
</domain>
)";

  // Step 5: Define the domain with the generated XML
  virDomainPtr vm = virDomainDefineXML(connection, vmXML.c_str());
  if (!vm) {
    std::cerr << "Failed to define VM" << std::endl;
    virConnectClose(connection);
    return false;
  }

  // Step 6: Start (create) the VM
  if (virDomainCreate(vm) < 0) {
    std::cerr << "Failed to start VM" << std::endl;
    virDomainFree(vm);
    virConnectClose(connection);
    return false;
  }

  std::cout << "VM started successfully" << std::endl;

  // Step 7: Cleanup references
  virDomainFree(vm);
  virConnectClose(connection);

  // (Optional) Remove the seed ISO & user-data files if you want:
  // unlink(isoPath.c_str());
  // unlink(userDataPath.c_str());
  // unlink(metaDataPath.c_str());

  return true;
}
