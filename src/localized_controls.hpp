#pragma once
#include <mods/svc/ui.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace tracker {
// UiService's bool control formats its value as the host's English On/Off.
// Use its public two-option dropdown for this mod's controls, bridging the
// original callbacks without changing the bool preference or its owner.
// UI_CONTROL_SELECT cannot be used here: the host rejects it in settings
// panels without a contextual help pane. DROPDOWN supports both panel kinds.
class LocalizedToggleControls {
    struct Binding {
        UiControlGetFn get;
        UiControlSetFn set;
        UiPredicateFn disabled, modified;
        void* data;
        std::array<std::string, 2> labels;
        std::array<const char*, 2> options;

        bool matches(const UiControlDesc& desc) const {
            return get == desc.get && set == desc.set && disabled == desc.is_disabled &&
                   modified == desc.is_modified && data == desc.user_data;
        }
        static void read(ModContext* context, void* opaque, UiControlValue* value) {
            const auto& binding = *static_cast<const Binding*>(opaque);
            UiControlValue original = UI_CONTROL_VALUE_INIT;
            binding.get(context, binding.data, &original);
            value->int_value = original.bool_value ? 1 : 0;
        }
        static void write(ModContext* context, void* opaque, const UiControlValue* value) {
            const auto& binding = *static_cast<const Binding*>(opaque);
            UiControlValue original = UI_CONTROL_VALUE_INIT;
            original.bool_value = value->int_value != 0;
            binding.set(context, binding.data, &original);
        }
        static bool isDisabled(ModContext* context, void* opaque) {
            const auto& binding = *static_cast<const Binding*>(opaque);
            return binding.disabled(context, binding.data);
        }
        static bool isModified(ModContext* context, void* opaque) {
            const auto& binding = *static_cast<const Binding*>(opaque);
            return binding.modified(context, binding.data);
        }
    };
    // Reuse by original callback identity, not by transient UI handles. This
    // keeps callback addresses stable and does not grow on tab/panel rebuilds.
    std::vector<std::unique_ptr<Binding>> bindings_;

public:
    bool adapt(UiControlDesc& desc, bool enabled, std::string off, std::string on) {
        if (!enabled || desc.kind != UI_CONTROL_TOGGLE ||
            desc.binding != UI_BINDING_CALLBACKS || !desc.get || !desc.set) return false;
        auto found = std::find_if(bindings_.begin(), bindings_.end(),
            [&](const auto& binding) { return binding->matches(desc); });
        Binding* binding;
        if (found == bindings_.end()) {
            auto added = std::make_unique<Binding>();
            added->get = desc.get; added->set = desc.set;
            added->disabled = desc.is_disabled; added->modified = desc.is_modified;
            added->data = desc.user_data;
            binding = added.get();
            bindings_.push_back(std::move(added));
        } else binding = found->get();
        binding->labels = {std::move(off), std::move(on)};
        for (size_t i = 0; i < 2; ++i) binding->options[i] = binding->labels[i].c_str();
        desc.kind = UI_CONTROL_DROPDOWN;
        desc.get = Binding::read; desc.set = Binding::write;
        desc.is_disabled = binding->disabled ? Binding::isDisabled : nullptr;
        desc.is_modified = binding->modified ? Binding::isModified : nullptr;
        desc.user_data = binding;
        desc.options = binding->options.data(); desc.option_count = 2;
        return true;
    }
    size_t bindingCount() const { return bindings_.size(); }
};
} // namespace tracker
