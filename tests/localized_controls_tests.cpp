#include "localized_controls.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct State { bool value = false, disabled = false, modified = false; int writes = 0; ModContext* context = nullptr; };
void get(ModContext* context, void* opaque, UiControlValue* value) {
    auto& state = *static_cast<State*>(opaque); state.context = context; value->bool_value = state.value;
}
void set(ModContext* context, void* opaque, const UiControlValue* value) {
    auto& state = *static_cast<State*>(opaque); state.context = context; state.value = value->bool_value; ++state.writes;
}
bool disabled(ModContext*, void* opaque) { return static_cast<State*>(opaque)->disabled; }
bool modified(ModContext*, void* opaque) { return static_cast<State*>(opaque)->modified; }
UiControlDesc control(State& state) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE; desc.label = "tracker-owned toggle";
    desc.help_rml = "<b>help</b>"; desc.tooltip = "tooltip";
    desc.get = get; desc.set = set; desc.user_data = &state;
    desc.is_disabled = disabled; desc.is_modified = modified;
    return desc;
}
}
int main() {
    try {
        tracker::LocalizedToggleControls controls;
        State state;
        auto original = control(state), desc = original;
        require(!controls.adapt(desc, false, "끄기", "켜기") && desc.kind == original.kind &&
                desc.get == get && desc.set == set && desc.user_data == &state && controls.bindingCount() == 0,
                "English mode modified a native toggle");
        require(controls.adapt(desc, true, "끄기", "켜기") && desc.kind == UI_CONTROL_DROPDOWN &&
                desc.option_count == 2 && std::string(desc.options[0]) == "끄기" && std::string(desc.options[1]) == "켜기",
                "Korean value labels do not use the dropdown supported in settings panels");
        require(desc.label == original.label && desc.help_rml == original.help_rml && desc.tooltip == original.tooltip &&
                desc.binding == original.binding && desc.config_var == original.config_var,
                "toggle adapter replaced unrelated descriptor fields");
        auto* context = reinterpret_cast<ModContext*>(uintptr_t(1234));
        UiControlValue value = UI_CONTROL_VALUE_INIT;
        desc.get(context, desc.user_data, &value);
        require(value.int_value == 0 && state.context == context && state.writes == 0, "false/read callback changed state");
        value.int_value = 1; desc.set(context, desc.user_data, &value);
        require(state.value && state.writes == 1 && state.context == context, "true/write did not reach original boolean owner");
        desc.get(context, desc.user_data, &value);
        require(value.int_value == 1 && state.writes == 1, "true/read changed original preference");
        value.int_value = 0; desc.set(context, desc.user_data, &value);
        require(!state.value && state.writes == 2, "false/write changed callback count or value");
        state.disabled = true; state.modified = true;
        require(desc.is_disabled(context, desc.user_data) && desc.is_modified(context, desc.user_data),
                "adapter lost disabled/modified predicates or their user_data");
        state.disabled = false; state.modified = false;
        require(!desc.is_disabled(context, desc.user_data) && !desc.is_modified(context, desc.user_data),
                "predicates failed to reflect changed original state");
        const auto firstHandle = desc.user_data;
        for (int rebuild = 0; rebuild < 500; ++rebuild) {
            auto rebuilt = original;
            require(controls.adapt(rebuilt, true, "끄기", "켜기") && rebuilt.user_data == firstHandle,
                    "panel rebuild invalidated callback identity");
        }
        require(controls.bindingCount() == 1, "panel rebuild leaked callback bindings");
        State other; auto separate = control(other);
        separate.is_disabled = separate.is_modified = nullptr;
        require(controls.adapt(separate, true, "끄기", "켜기") && !separate.is_disabled && !separate.is_modified,
                "optional predicates became callable with null originals");
        value.int_value = 1; separate.set(context, separate.user_data, &value);
        require(other.value && !state.value && other.writes == 1, "independent preferences shared adapter state");
        desc.get(context, desc.user_data, &value);
        require(value.int_value == 0, "a new binding invalidated an existing callback");
        auto config = original; config.binding = UI_BINDING_CONFIG_VAR; config.config_var = 42;
        auto button = original; button.kind = UI_CONTROL_BUTTON;
        auto incomplete = original; incomplete.get = nullptr;
        require(!controls.adapt(config, true, "끄기", "켜기") && config.config_var == 42 &&
                !controls.adapt(button, true, "끄기", "켜기") &&
                !controls.adapt(incomplete, true, "끄기", "켜기"), "unrelated or unsupported control was adapted");
        std::cout << "Tracker-local Korean toggle labels preserve boolean callbacks, predicates and rebuild lifetimes.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
