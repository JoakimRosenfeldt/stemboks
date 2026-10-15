// Mixxx owns MIDI transport, script execution, and control connections.
var StemboksSBX = {};

StemboksSBX.create = function(inputs, outputs) {
    var group = "[Stemboks]";
    var modifiers = {};
    var connections = [];
    var routes = {};
    var held = {};
    inputs.forEach(function(binding, index) {
        var address = binding.status + ":" + binding.number;
        if (!routes[address]) routes[address] = [];
        binding.index = index;
        routes[address].push(binding);
    });

    function allowed(binding) {
        if (!binding.modifier) return true;
        var inverted = binding.modifier.charAt(0) === "!";
        var name = inverted ? binding.modifier.slice(1) : binding.modifier;
        return !!modifiers[name] !== inverted;
    }

    function feedback(binding, value) {
        if (!allowed(binding) || !isFinite(value)) return;
        var fraction = binding.threshold ? (value >= binding.minimum && value <= binding.maximum ? 1 : 0) :
            binding.maximum !== binding.minimum ? (value - binding.minimum) / (binding.maximum - binding.minimum) : 0;
        var velocity = Math.round(Math.max(0, Math.min(1, fraction)) * 127);
        var status = (binding.status & 0xF0) === 0xB0 ? binding.status :
            (velocity ? 0x90 : 0x80) | (binding.status & 0x0F);
        midi.sendShortMsg(status, binding.number, velocity);
    }

    function refreshFeedback() {
        outputs.forEach(function(binding) {
            feedback(binding, engine.getValue(group, binding.control));
        });
    }

    function apply(binding, raw, pressed) {
        var active = allowed(binding);
        if (binding.kind === "modifier") {
            if (active) modifiers[binding.control] = pressed;
            return;
        }
        if (binding.kind === "effect-selection") {
            var unit = binding.control.split(".").slice(0, 3).join(".");
            if (pressed && active) {
                engine.setValue(group, unit + ".select." + binding.effect, 1);
                if (binding.offOnRelease) {
                    held[binding.index] = unit + ".enabled";
                    engine.setValue(group, unit + ".enabled", 1);
                }
            } else if (!pressed && held[binding.index]) {
                engine.setValue(group, held[binding.index], 0);
                delete held[binding.index];
            }
            return;
        }
        if (binding.mode === "momentary") {
            if (pressed && active) {
                held[binding.index] = binding.control;
                engine.setValue(group, binding.control, binding.outMax);
            } else if (!pressed && (active || held[binding.index])) {
                engine.setValue(group, binding.control, binding.outMin);
                delete held[binding.index];
            }
            return;
        }
        if (!active) return;
        var note = (binding.status & 0xF0) === 0x90;
        if (note && !pressed) return;
        if (binding.mode === "absolute") {
            var fraction = Math.max(0, Math.min(1, (raw - binding.inMin) / (binding.inMax - binding.inMin)));
            engine.setValue(group, binding.control, binding.outMin + fraction * (binding.outMax - binding.outMin));
        } else if (binding.mode === "relative") {
            var step = note ? 1 : raw - 64;
            if (binding.twosComplement) step = raw >= 64 ? raw - 128 : raw;
            engine.setValue(group, binding.control,
                engine.getValue(group, binding.control) + step * (binding.outMax - binding.outMin));
        } else if (binding.mode === "toggle" && pressed) {
            engine.setValue(group, binding.control, engine.getValue(group, binding.control) !== 0 ? 0 : 1);
        } else if (binding.mode === "trigger" && pressed) {
            engine.setValue(group, binding.control, binding.outMax);
        }
    }

    return {
        init: function() {
            modifiers = {};
            held = {};
            outputs.forEach(function(binding) {
                var connection = engine.makeConnection(group, binding.control, function(value) {
                    feedback(binding, value);
                });
                connections.push(connection);
                connection.trigger();
            });
        },
        input: function(channel, control, value, status) {
            var type = status & 0xF0;
            var canonical = type === 0x80 ? status + 0x10 : status;
            var raw = type === 0xE0 ? control + (value << 7) : type === 0x80 ? 0 : value;
            var pressed = type !== 0x80 && raw > 0;
            var bindings = routes[canonical + ":" + (type === 0xE0 ? 0 : control)] || [];
            for (var pass = 0; pass < 2; ++pass) {
                bindings.forEach(function(binding) {
                    if ((binding.kind === "modifier") === (pass === 0)) apply(binding, raw, pressed);
                });
            }
            if (bindings.some(function(binding) { return binding.kind === "modifier"; })) refreshFeedback();
        },
        shutdown: function() {
            connections.forEach(function(connection) { connection.disconnect(); });
            connections = [];
            Object.keys(held).forEach(function(index) {
                var binding = inputs[index];
                engine.setValue(group, held[index], binding.kind === "effect-selection" ? 0 : binding.outMin);
            });
            held = {};
            outputs.forEach(function(binding) {
                midi.sendShortMsg((binding.status & 0xF0) === 0xB0 ? binding.status :
                    0x80 | (binding.status & 0x0F), binding.number, 0);
            });
        }
    };
};
