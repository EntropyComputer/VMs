#include "command_handler.hpp"  // Include the header with the function declaration.
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include "vm.hpp"

// Include libvirt header.
#include <libvirt/libvirt.h>

// TODO: implement "shutdown" command

// The handle_command function takes a JSON command and a connection to the hypervisor,
// processes the command, and returns a JSON response.
//
// command_json has "command" and "params"
json handle_command(const json& command_json, virConnectPtr conn) {
    // Get the command from the request.
    std::string command = command_json["command"];
    json response;

    try {
        if (command == "create") {
            // Extract parameters from the JSON payload.
            std::string vmName = command_json["params"]["vmName"];
            int memoryMB = command_json["params"]["memoryMB"];
            int vcpus = command_json["params"]["vcpus"];

            // Automatically construct the disk path using the vmName.
            std::string diskPath = "/var/lib/libvirt/images/" + vmName + ".qcow2";

            // Check if a domain with this name already exists.
            virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
            if (dom) {
                virDomainFree(dom);
                throw std::invalid_argument("VM already exists. Use the 'start' command instead.");
            }

            // Attempt to create and start the VM.
            // Assume vm_spinUp is defined elsewhere and returns a bool indicating success.
            if (vm_spinUp(vmName, memoryMB, vcpus, diskPath)) {
                response["status"] = "success";
                response["message"] = "Successfully created and started VM " + vmName;
            } else {
                response["status"] = "error";
                response["message"] = "Failed to create and start VM " + vmName;
            }
        }
        else if (command == "start") {
            std::string vmName = command_json["params"]["vmName"];
            std::ostringstream startVM;
            startVM << "virsh start " << vmName;
            if (system(startVM.str().c_str()) != 0) {
                throw std::runtime_error("Failed to start VM: " + vmName);
            }
            response["status"] = "success";
            response["message"] = "VM " + vmName + " started (resumed) successfully.";
        }
        else if (command == "pause") {
            std::string vmName = command_json["params"]["vmName"];
            std::ostringstream stopVM;
            stopVM << "virsh managedsave " << vmName;
            if (system(stopVM.str().c_str()) != 0) {
                throw std::runtime_error("Failed to stop VM: " + vmName);
            }
            response["status"] = "success";
            response["message"] = "VM " + vmName + " stopped successfully.";
        }
        else if (command == "resume") {
            std::string vmName = command_json["params"]["vmName"];
            const char* home = getenv("HOME");
            const char* display = getenv("DISPLAY");
            const char* xauth = getenv("XAUTHORITY");

            std::ostringstream openVM;
            openVM << "env -i HOME=\"" << (home ? home : "") << "\" "
                   << "PATH=\"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\" "
                   << "DISPLAY=\"" << (display ? display : "") << "\" "
                   << "XAUTHORITY=\"" << (xauth ? xauth : (std::string(home ? home : "") + "/.Xauthority")) << "\" "
                   << "remote-viewer spice://localhost:5900";

            if (system(openVM.str().c_str()) != 0) {
                throw std::runtime_error("Failed to open VM: " + vmName);
            }
            response["status"] = "success";
            response["message"] = "VM " + vmName + " opened successfully.";
        } else {
            response["status"] = "error";
            response["message"] = "Unrecognized command: " + command;
        }
    }
    catch (const std::exception& ex) {
        response["status"] = "error";
        response["message"] = ex.what();
    }

    return response;
}
