# VMs
VM Spin-up, Capture, Encode, and Stream 

### vm_spinUp

```C
virConnectPtr virConnectOpen(const char* name);
```

- name
  - Specifies the URI (Uniform Resource Identifier) of the hypervisor to connect to.
  - qemu:///system
  - Connects to system wide instance of the QEMU/KVM Hypervisor
- Returns
  - pointer to a ```virConnectPtr``` structure representing connection to hypervisor
  - If fail, returns ```NULL```


##### XML
- Dynamic allocation of RAM, vcpus, diskPath, etc.

```C
virDomainPtr virDomainDefineXML(virConnectPtr conn, const char* xml);
```

- conn
  - pointer to a connection object (```virConnectPtr```) that represents a connection to the hypervisor.
- xml
  - a string containing the XML configuration of the virtual machine. Specifies all VM properties.
- Returns
  - point to ```virDomainPtr``` object that represents the defined VM

```C
int virDomainCreate(virDomainPtr domain);
```

- domain
  - a pointer to an object representing the VM
    - where the object is
- Returns
  - 0 if the VM is successfully started
  - -1 if function fails


##### Useful Terminal Commands
```virsh dominfo *vmName*```

```virsh list -all```

```virsh undefine dynamic-vm```