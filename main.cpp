#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <libvirt/libvirt.h>
#include "vm.hpp"       // Contains vm_spinUp declaration.
#include <vector>
#include <thread>
#include <chrono>
#include <stdexcept>    // For std::invalid_argument, std::runtime_error
#include <cstdlib>      // For system()
#include <string>
#include <boost/asio/local/stream_protocol.hpp>
using boost::asio::local::stream_protocol;

#include "command_handler.hpp"

// TODO: refactor cli mode to call "command handler"
int run_cli_mode(int argc, char* argv[], virConnectPtr conn) {
  // Check that at least one argument (the command) is provided.
  if (argc < 2) {
      std::cerr << "Usage:\n"
                << "  " << argv[0] << " spin_up <vmName> <memoryMB> <vcpus> <diskPath>\n"
                << "  " << argv[0] << " start <vmName>\n";
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
  else if (command == "golden-image") {
    if (argc < 2) {
      std::cerr << "Usage: " << argv[0] << " golden-image" << std::endl;
      virConnectClose(conn);
      return 1;
    }

    // Check if a domain with this name already exists.
    virDomainPtr dom = virDomainLookupByName(conn, "goldenImage");
    if (dom) {
        virDomainFree(dom);
        virConnectClose(conn);
        throw std::invalid_argument("Tried to spin up a VM that already exists. Use the \"start\" command instead.");
    }

    // Attempt to create and start the VM.
    if (vm_spinUpGoldenImage()) {
        std::cout << "Successfully created and started the Golden Image VM" << std::endl;
    } else {
        std::cerr << "Failed to create and start the Golden Image VM" << std::endl;
        virConnectClose(conn);
        return 1;
    }
    virConnectClose(conn);
    return 0;
  }
  else {
    virConnectClose(conn);
    throw std::invalid_argument("Unrecognized command: " + command);
  }

  return 0;
}

int run_daemon_mode(virConnectPtr conn) {
    // Remove any existing socket file.
    std::remove("/tmp/vm_manager.sock");

    boost::asio::io_context io_context;
    stream_protocol::endpoint endpoint("/tmp/vm_manager.sock");
    stream_protocol::acceptor acceptor(io_context, endpoint);

    std::cout << "VM Manager daemon is running, listening on /tmp/vm_manager.sock" << std::endl;

    // Main loop: continuously accept and handle incoming connections.
    while (true) {
        try {
            stream_protocol::socket socket(io_context);
            acceptor.accept(socket);

            // Read data from the socket until a newline (message delimiter).
            boost::asio::streambuf buf;
            boost::asio::read_until(socket, buf, "\n");
            std::istream is(&buf);
            std::string request_line;
            std::getline(is, request_line);

            // Parse the JSON command.
            json command_json = json::parse(request_line);

            // Process the command and get a JSON response.
            json response_json = handle_command(command_json, conn);

            // Serialize the response and send it back over the socket.
            std::string response_str = response_json.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(response_str));

            // Socket will close when it goes out of scope.
        } catch (const std::exception& ex) {
            std::cerr << "Error handling connection: " << ex.what() << std::endl;
        }
    }

    return 0;  // (This point is never reached, but added for completeness.)
}


int main(int argc, char* argv[]) {
  // Open connection to the hypervisor.
  virConnectPtr conn = virConnectOpen("qemu:///system");
  if (!conn) {
      std::cerr << "Failed to connect to hypervisor" << std::endl;
      return 1;
  }

  // If any command-line arguments are provided, run in CLI mode
  if (argc > 1) {
      int ret = run_cli_mode(argc, argv, conn);
      return ret;
  }
  else {
      // No arguments: run as daemon.
      int ret = run_daemon_mode(conn);
      virConnectClose(conn);
      return ret;
  }
}
