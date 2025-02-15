// command_handler.hpp
#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <nlohmann/json.hpp>
#include <libvirt/libvirt.h>

using json = nlohmann::json;

// Processes the JSON command and returns a JSON response.
json handle_command(const json& command_json, virConnectPtr conn);

#endif // COMMAND_HANDLER_HPP