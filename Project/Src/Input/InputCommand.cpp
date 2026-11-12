#include "LGF/Input.h"

#include <algorithm>
#include <map>
#include <utility>

using namespace LGF;

namespace {

	std::map<std::string, InputBindingList, std::less<>> g_commands;

	const InputBindingList* FindBindings(std::string_view command) {
		const auto iterator = g_commands.find(command);
		return iterator != g_commands.end() ? &iterator->second : nullptr;
	}

	template <class Predicate>
	bool AnyBinding(std::string_view command, Predicate predicate) {
		const InputBindingList* bindings = FindBindings(command);
		return bindings && std::ranges::any_of(*bindings, predicate);
	}

}

InputCommand::InputCommand(std::string command) :
	command_(std::move(command)) {
}

bool InputCommand::Trigger() const {
	return Input::Trigger(command_);
}

bool InputCommand::Press() const {
	return Input::Press(command_);
}

bool InputCommand::Release() const {
	return Input::Release(command_);
}

const std::string& InputCommand::Name() const {
	return command_;
}

bool Input::RegisterCommand(
	std::string_view command,
	const InputBindingList& bindings) {
	if (command.empty() || g_commands.contains(command)) {
		return false;
	}

	g_commands.emplace(command, bindings);
	return true;
}

bool Input::UnregisterCommand(std::string_view command) {
	return g_commands.erase(std::string{ command }) != 0u;
}

bool Input::HasCommand(std::string_view command) {
	return g_commands.contains(command);
}

void Input::ClearCommands() {
	g_commands.clear();
}

bool Input::SetBindings(
	std::string_view command,
	const InputBindingList& bindings) {
	auto iterator = g_commands.find(command);
	if (iterator == g_commands.end()) {
		return false;
	}

	iterator->second = bindings;
	return true;
}

bool Input::AddBinding(
	std::string_view command,
	const InputBinding& binding) {
	auto iterator = g_commands.find(command);
	if (iterator == g_commands.end()) {
		return false;
	}

	InputBindingList& bindings = iterator->second;
	if (std::ranges::find(bindings, binding) != bindings.end()) {
		return false;
	}

	bindings.push_back(binding);
	return true;
}

bool Input::RemoveBinding(
	std::string_view command,
	const InputBinding& binding) {
	auto iterator = g_commands.find(command);
	if (iterator == g_commands.end()) {
		return false;
	}

	InputBindingList& bindings = iterator->second;
	const auto bindingIterator = std::ranges::find(bindings, binding);
	if (bindingIterator == bindings.end()) {
		return false;
	}

	bindings.erase(bindingIterator);
	return true;
}

InputBindingList Input::GetBindings(std::string_view command) {
	const InputBindingList* bindings = FindBindings(command);
	return bindings ? *bindings : InputBindingList{};
}

bool Input::Trigger(std::string_view command) {
	return AnyBinding(command, [](const InputBinding& binding) {
		return std::visit([](const auto& button) {
			return button.Trigger();
		}, binding);
	});
}

bool Input::Press(std::string_view command) {
	return AnyBinding(command, [](const InputBinding& binding) {
		return std::visit([](const auto& button) {
			return button.Press();
		}, binding);
	});
}

bool Input::Release(std::string_view command) {
	return AnyBinding(command, [](const InputBinding& binding) {
		return std::visit([](const auto& button) {
			return button.Release();
		}, binding);
	});
}
