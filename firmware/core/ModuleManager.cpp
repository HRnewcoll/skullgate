/**
 * @file ModuleManager.cpp
 * @brief Implementation of ModuleManager — module discovery, permission
 *        enforcement, and lifecycle management.
 */

#include "ModuleManager.h"
#include "../drivers/SdManager.h"

// ── Lab Mode configuration ────────────────────────────────────────────────────
// The PIN is intentionally simple for demo purposes.
// In production, store a bcrypt hash of the PIN in the SD flag file.
static constexpr char LAB_MODE_FLAG_PATH[] = "/lab_mode.flag";
static constexpr char LAB_MODE_DEFAULT_PIN[] = "1337";

namespace skullgate {

// ── ModuleRegistry ────────────────────────────────────────────────────────────

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry inst;
    return inst;
}

void ModuleRegistry::registerFactory(const String& id, ModuleFactory factory) {
    _factories.push_back({id, std::move(factory)});
    Serial.printf("[ModuleRegistry] Registered module: %s\n", id.c_str());
}

IModule* ModuleRegistry::create(const String& id) const {
    for (const auto& entry : _factories) {
        if (entry.first == id) return entry.second();
    }
    Serial.printf("[ModuleRegistry] No factory for: %s\n", id.c_str());
    return nullptr;
}

// ── ModuleManager ─────────────────────────────────────────────────────────────

ModuleManager::ModuleManager(CoreAPI& api)
    : _api(api), _labMode(false), _active(nullptr)
{}

int ModuleManager::loadAll() {
    int loaded = 0;

    // Iterate every registered module factory; each factory must have a
    // corresponding manifest parsed from the SD card or embedded string.
    // For simplicity, we ask each module to self-describe via its manifest().
    for (auto& mod_ptr : _modules) { delete mod_ptr; }
    _modules.clear();
    _manifests.clear();

    // Get list of registered module IDs from the registry
    // We use a simple approach: try to create each module, call manifest(),
    // validate permissions, then either keep or discard it.
    // In a full runtime-load system this would scan the SD /modules/ directory.

    // For each registered factory:
    const auto& factories = ModuleRegistry::instance();
    // We need to iterate — expose a temporary module to read its manifest
    // by creating a scratch instance (cheap, no hardware init yet).

    // NOTE: We create a scratch instance ONLY to read the manifest, then
    // destroy it and recreate for real if permissions pass.
    //
    // A better design would have the manifest separate from the instance,
    // but this keeps the IModule interface minimal.

    Serial.println("[ModuleManager] Scanning module registry...");

    // Re-read all registered modules via a proxy scan
    // We'll collect IDs by creating temporary instances
    std::vector<String> ids;
    // The ModuleRegistry doesn't expose its list directly, so we maintain
    // a local scan list. Modules register themselves with REGISTER_MODULE,
    // so we need to enumerate them here via the registry.
    // As a practical approach, we'll use the registry's create() function
    // with well-known IDs. For a general solution, we add a list() method.

    // Temporarily use the registry to get all IDs.
    // (We call the internal _factories list indirectly through the singleton.)
    auto& reg = ModuleRegistry::instance();

    // Use a scan of the registry's factory list via a helper approach.
    // Since ModuleRegistry stores pairs, we expose an iterator helper.
    //
    // Practical workaround: modules append their ID to a global list at
    // static-init time. We use a separate registry for IDs.
    //
    // For this implementation we enumerate with a direct factory call.
    // See ModuleManager::_tryLoad() which will be called per known module id.

    // For now, try to load all modules that have registered factories.
    // We do this by using a scan helper added to ModuleRegistry.
    for (const String& id : reg.listIds()) {
        IModule* scratch = reg.create(id);
        if (!scratch) continue;

        ModuleManifest mf = scratch->manifest();
        delete scratch;

        if (!_checkPermissions(mf)) {
            Serial.printf("[ModuleManager] Module '%s' denied (missing perms or Lab Mode required)\n",
                          id.c_str());
            continue;
        }

        // Create the real instance
        IModule* mod = reg.create(id);
        if (!mod) continue;

        // Pass the API with the module's granted permissions
        // Downcast to CoreAPIImpl to set permissions per-module
        // (In a more sophisticated system, we'd wrap CoreAPI per-module)
        if (!mod->init(_api)) {
            Serial.printf("[ModuleManager] Module '%s' init() returned false\n",
                          id.c_str());
            delete mod;
            continue;
        }

        _modules.push_back(mod);
        _manifests.push_back(mf);
        loaded++;
        Serial.printf("[ModuleManager] Loaded module: %s v%s\n",
                      mf.name.c_str(), mf.version.c_str());
    }

    Serial.printf("[ModuleManager] %d module(s) loaded\n", loaded);
    return loaded;
}

bool ModuleManager::activate(const String& id) {
    for (IModule* m : _modules) {
        if (m->manifest().id == id) {
            if (_active) _active->stop();
            _active = m;
            _active->start();
            return true;
        }
    }
    return false;
}

void ModuleManager::deactivate() {
    if (_active) {
        _active->stop();
        _active = nullptr;
    }
}

void ModuleManager::loop() {
    if (_active) _active->loop();
}

bool ModuleManager::unlockLabMode(const String& pin) {
    // Lab Mode requires BOTH an SD flag file AND the correct PIN.
    // This prevents accidental Lab Mode activation if a malicious SD is inserted.
    if (!_api.readFile(LAB_MODE_FLAG_PATH).length()) {
        Serial.println("[ModuleManager] Lab Mode flag file not found on SD");
        return false;
    }
    if (pin != LAB_MODE_DEFAULT_PIN) {
        Serial.println("[ModuleManager] Lab Mode PIN incorrect");
        return false;
    }
    _labMode = true;
    Serial.println("[ModuleManager] *** LAB MODE ACTIVATED ***");
    return true;
}

void ModuleManager::lockLabMode() {
    _labMode = false;
    Serial.println("[ModuleManager] Lab Mode deactivated — Recon-Only mode");
}

bool ModuleManager::_checkPermissions(const ModuleManifest& manifest) const {
    // If the module requires Lab Mode but Lab Mode is off, deny.
    if (manifest.lab_mode && !_labMode) return false;

    // Check each permission against the Lab Mode gate.
    for (const auto& perm : manifest.permissions) {
        bool requiresLab = false;
        for (int i = 0; LAB_MODE_PERMISSIONS[i] != nullptr; ++i) {
            if (perm == LAB_MODE_PERMISSIONS[i]) { requiresLab = true; break; }
        }
        if (requiresLab && !_labMode) return false;
    }
    return true;
}

bool ModuleManager::_parseManifeest(const String& json,
                                     ModuleManifest& out) const {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

    out.id          = doc["id"]          | "";
    out.name        = doc["name"]        | "";
    out.version     = doc["version"]     | "0.0.0";
    out.author      = doc["author"]      | "";
    out.description = doc["description"] | "";
    out.lab_mode    = doc["lab_mode"]    | false;
    out.valid       = !out.id.isEmpty();

    for (const char* p : doc["permissions"].as<JsonArray>()) {
        out.permissions.push_back(String(p));
    }
    return out.valid;
}

} // namespace skullgate
