/*
 * Copyright (c) 2020 Valve Corporation
 * Copyright (c) 2020 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 * - Richard S. Wright Jr.
 * - Christophe Riccio
 */

#include "configurator.h"
#include "settingstreemanager.h"

#include "../vkconfig_core/version.h"
#include "../vkconfig_core/platform.h"
#include "../vkconfig_core/util.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QApplication>

#include <cassert>

SettingsTreeManager::SettingsTreeManager()
    : _configuration_settings_tree(nullptr),
      _validation_presets_combo_box(nullptr),
      _validation_tree_item(nullptr),
      _validation_file_item(nullptr),
      _validation_log_file_item(nullptr),
      _validation_log_file_widget(nullptr),
      _validation_settings(nullptr) {}

void SettingsTreeManager::CreateGUI(QTreeWidget *build_tree) {
    assert(build_tree);

    // Do this first to make absolutely sure if thee is an old configuration still active
    // it's state gets saved.
    CleanupGUI();

    _configuration_settings_tree = build_tree;
    auto configuration = Configurator::Get().GetActiveConfiguration();

    build_tree->blockSignals(true);
    build_tree->clear();

    if (configuration->parameters.empty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, "No overridden or excluded layer");
        build_tree->addTopLevelItem(item);
    } else {
        // There will be one top level item for each layer
        for (std::size_t i = 0, n = configuration->parameters.size(); i < n; ++i) {
            Parameter &parameter = configuration->parameters[i];
            if (parameter.state != LAYER_STATE_OVERRIDDEN) continue;

            const std::vector<Layer> &available_layers = Configurator::Get().layers.available_layers;
            const std::vector<Layer>::const_iterator layer = Find(available_layers, parameter.name);

            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, parameter.name + (layer != available_layers.end() ? "" : " (Missing)"));
            _configuration_settings_tree->addTopLevelItem(item);
            _layer_items.push_back(item);

            if (layer == available_layers.end()) continue;

            // Handle the case were we get off easy. No settings.
            if (parameter.settings.empty()) {
                QTreeWidgetItem *child = new QTreeWidgetItem();
                child->setText(0, "No User Settings");
                item->addChild(child);
                continue;
            }

            if (parameter.name == "VK_LAYER_KHRONOS_validation") {
                _validation_tree_item = item;
                BuildKhronosTree(parameter.settings);
                continue;
            }

            // Generic is the only one left
            BuildGenericTree(item, parameter);
        }

        ///////////////////////////////////////////////////////////////////
        // The last item is just the excluded layers
        QTreeWidgetItem *excluded_layers = new QTreeWidgetItem();
        excluded_layers->setText(0, "Excluded Layers:");
        build_tree->addTopLevelItem(excluded_layers);

        for (std::size_t i = 0, n = configuration->parameters.size(); i < n; ++i) {
            Parameter &parameter = configuration->parameters[i];
            if (parameter.state != LAYER_STATE_EXCLUDED) continue;

            const std::vector<Layer>::const_iterator layer = Find(Configurator::Get().layers.available_layers, parameter.name);

            QTreeWidgetItem *child = new QTreeWidgetItem();
            child->setText(0, parameter.name + (layer != Configurator::Get().layers.available_layers.end() ? "" : " (Missing)"));
            excluded_layers->addChild(child);
        }

        // None excluded layer were found
        if (excluded_layers->childCount() == 0) {
            QTreeWidgetItem *child = new QTreeWidgetItem();
            child->setText(0, "None");
            excluded_layers->addChild(child);
        }
    }

    // Everyone is expanded.
    build_tree->resizeColumnToContents(0);
    SetTreeState(configuration->_setting_tree_state, 0, _configuration_settings_tree->invisibleRootItem());
    build_tree->blockSignals(false);
}

void SettingsTreeManager::BuildKhronosTree(std::vector<LayerSetting> &settings) {
    _validation_preset_item = new QTreeWidgetItem();
    _validation_preset_item->setText(0, "Validation Preset");
    QTreeWidgetItem *next_line = new QTreeWidgetItem();

    Configurator &configurator = Configurator::Get();
    std::vector<Layer> &available_layers = configurator.layers.available_layers;
    const std::vector<Layer>::const_iterator validation_layer = Find(available_layers, "VK_LAYER_KHRONOS_validation");

    _validation_presets_combo_box = new QComboBox();
    _validation_presets_combo_box->blockSignals(true);
    _validation_presets.clear();
    for (int i = ValidationPresetFirst; i <= ValidationPresetLast; ++i) {
        const ValidationPreset validation_preset = static_cast<ValidationPreset>(i);

        if (!HAS_SHADER_BASED &&
            (validation_preset == ValidationPresetGPUAssisted || validation_preset == ValidationPresetDebugPrintf)) {
            continue;
        }

        QString preset_name = configurator.GetValidationPresetLabel(validation_preset);

        // There is no preset for a user defined group of settings, so watch for blank.
        if (preset_name.isEmpty()) preset_name = "User Defined";

        _validation_presets_combo_box->addItem(preset_name);
        _validation_presets.push_back(validation_preset);
    }

    auto configuration = configurator.GetActiveConfiguration();
    _validation_presets_combo_box->setCurrentIndex(GetValidationPresentIndex(configuration->_preset));

    connect(_validation_presets_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(khronosPresetChanged(int)));
    _validation_tree_item->addChild(_validation_preset_item);
    _validation_preset_item->addChild(next_line);
    _configuration_settings_tree->setItemWidget(next_line, 0, _validation_presets_combo_box);

    QTreeWidgetItem *_validation_settingsitem = new QTreeWidgetItem();
    _validation_settingsitem->setText(0, "Individual Settings");
    _validation_preset_item->addChild(_validation_settingsitem);

    // This just finds the enables and disables
    _validation_settings = new KhronosSettingsAdvanced(_configuration_settings_tree, _validation_settingsitem, settings);

    // Get the Debug Action and log file settings (and they must exist)
    LayerSetting &debug_action = *FindSetting(settings, "debug_action");
    LayerSetting &log_file = *FindSetting(settings, "log_filename");

    // The debug action set of settings has it's own branch
    QTreeWidgetItem *debug_action_branch = new QTreeWidgetItem();
    debug_action_branch->setText(0, "Debug Action");
    _validation_tree_item->addChild(debug_action_branch);

    // Each debug action has it's own checkbox
    for (int i = 0, n = debug_action.inclusive_values.size(); i < n; ++i) {
        // Debug output is only for Windows
        if (!PLATFORM_WINDOWS && debug_action.inclusive_values[i] == "VK_DBG_LAYER_ACTION_DEBUG_OUTPUT") continue;

        QTreeWidgetItem *child = new QTreeWidgetItem();
        MultiEnumSettingWidget *this_control = new MultiEnumSettingWidget(debug_action, debug_action.inclusive_values[i]);
        this_control->setText(debug_action.inclusive_labels[i]);
        debug_action_branch->addChild(child);
        _configuration_settings_tree->setItemWidget(child, 0, this_control);
        this_control->setFont(_configuration_settings_tree->font());
        connect(this_control, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));

        // The log message action also has a child; the log file selection setting/widget
        // Note, this is usually last, but I'll check for it any way in case other new items are added
        if (debug_action.inclusive_values[i] == "VK_DBG_LAYER_ACTION_LOG_MSG") {  // log action?
            _validation_debug_action = this_control;
            _validation_log_file_item = new QTreeWidgetItem();
            child->addChild(_validation_log_file_item);
            _validation_log_file_widget = new FileSystemSettingWidget(_validation_log_file_item, log_file, SETTING_SAVE_FILE);
            _configuration_settings_tree->setItemWidget(_validation_log_file_item, 0, _validation_log_file_widget);
            _validation_log_file_item->setSizeHint(0, QSize(0, 28));
            _compound_widgets.push_back(child);

            connect(_validation_log_file_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            connect(_validation_debug_action, SIGNAL(stateChanged(int)), this, SLOT(khronosDebugChanged(int)));

            // Capture initial state, which reflects enabled/disabled
            _validation_log_file_widget->setDisabled(!_validation_debug_action->isChecked());
        }
    }

    // This is looking for the report flags
    for (std::size_t setting_index = 0, settings_count = settings.size(); setting_index < settings_count; setting_index++) {
        LayerSetting &layer_setting = settings[setting_index];

        // Multi-enum - report flags only
        if (layer_setting.key == "report_flags") {
            QTreeWidgetItem *sub_category = new QTreeWidgetItem;
            sub_category->setText(0, layer_setting.label);
            sub_category->setToolTip(0, layer_setting.description);
            _validation_tree_item->addChild(sub_category);

            for (int i = 0, n = layer_setting.inclusive_values.size(); i < n; ++i) {
                QTreeWidgetItem *child = new QTreeWidgetItem();
                MultiEnumSettingWidget *control = new MultiEnumSettingWidget(layer_setting, layer_setting.inclusive_values[i]);
                control->setText(layer_setting.inclusive_labels[i]);
                sub_category->addChild(child);
                _configuration_settings_tree->setItemWidget(child, 0, control);
                control->setFont(_configuration_settings_tree->font());
                connect(control, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            }
        } else if (layer_setting.key == "duplicate_message_limit") {
            if (validation_layer != available_layers.end()) {  // duplicate_message_limit is new with 1.2.148
                if (validation_layer->_api_version < Version("1.2.148")) continue;
            }

            QTreeWidgetItem *setting_item = new QTreeWidgetItem();
            StringSettingWidget *widget = new StringSettingWidget(setting_item, layer_setting);
            _validation_tree_item->addChild(setting_item);
            QTreeWidgetItem *place_holder = new QTreeWidgetItem();
            setting_item->addChild(place_holder);
            _configuration_settings_tree->setItemWidget(place_holder, 0, widget);
            connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
        }
    }

    // VUID message filtering
    for (std::size_t setting_index = 0, settings_count = settings.size(); setting_index < settings_count; setting_index++) {
        LayerSetting &layer_setting = settings[setting_index];

        if (layer_setting.type != SETTING_VUID_FILTER) {
            continue;
        }

        QTreeWidgetItem *mute_message_item = new QTreeWidgetItem;

        mute_message_item->setText(0, "Mute Message VUIDs");
        _validation_tree_item->addChild(mute_message_item);

        _vuid_search_widget = new VUIDSearchWidget(layer_setting.value);
        next_line = new QTreeWidgetItem();
        next_line->setSizeHint(0, QSize(0, 28));
        mute_message_item->addChild(next_line);
        _configuration_settings_tree->setItemWidget(next_line, 0, _vuid_search_widget);
        _compound_widgets.push_back(next_line);

        QTreeWidgetItem *pListItem = new QTreeWidgetItem();
        mute_message_item->addChild(pListItem);
        pListItem->setSizeHint(0, QSize(0, 200));
        _mute_message_widget = new MuteMessageWidget(layer_setting);
        _compound_widgets.push_back(pListItem);
        _configuration_settings_tree->setItemWidget(pListItem, 0, _mute_message_widget);

        connect(_vuid_search_widget, SIGNAL(itemSelected(const QString &)), _mute_message_widget, SLOT(addItem(const QString &)));
        connect(_vuid_search_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
        connect(_mute_message_widget, SIGNAL(itemRemoved(const QString &)), _vuid_search_widget,
                SLOT(addToSearchList(const QString &)));
        connect(_mute_message_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()), Qt::QueuedConnection);
    }

    // This really does go way down here.
    connect(_validation_settings, SIGNAL(settingChanged()), this, SLOT(OnPresetEdited()));
    _validation_presets_combo_box->blockSignals(false);

    _validation_tree_item->addChild(_validation_preset_item);
}

void SettingsTreeManager::khronosDebugChanged(int index) {
    (void)index;
    bool enabled = !(_validation_debug_action->isChecked());
    _configuration_settings_tree->blockSignals(true);
    _validation_log_file_item->setDisabled(enabled);
    _validation_log_file_widget->setDisabled(enabled);
    _configuration_settings_tree->blockSignals(false);
    OnSettingEdited();
}

void SettingsTreeManager::BuildGenericTree(QTreeWidgetItem *parent, Parameter &parameter) {
    std::vector<LayerSetting> &settings = parameter.settings;
    std::vector<Layer> &available_layers = Configurator::Get().layers.available_layers;

    for (std::size_t setting_index = 0, n = settings.size(); setting_index < n; setting_index++) {
        LayerSetting &setting = settings[setting_index];

        QTreeWidgetItem *setting_item = new QTreeWidgetItem();

        switch (setting.type) {
            case SETTING_BOOL:          // True false?
            case SETTING_BOOL_NUMERIC:  // True false? (with numeric output instead of text)
            {
                // Don't display "emulate_portability" setting if the layer doesn't support it
                if (setting.key == "emulate_portability" && parameter.name == "VK_LAYER_LUNARG_device_simulation") {
                    std::vector<Layer>::iterator layer = Find(available_layers, "VK_LAYER_LUNARG_device_simulation");
                    if (layer != available_layers.end()) {
                        if (Version(layer->_implementation_version) <= Version("1.3.0")) break;
                    }
                }

                BoolSettingWidget *widget = new BoolSettingWidget(setting, setting.type);
                parent->addChild(setting_item);
                _configuration_settings_tree->setItemWidget(setting_item, 0, widget);
                widget->setFont(_configuration_settings_tree->font());
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            } break;

            case SETTING_SAVE_FILE:    // Save a file?
            case SETTING_LOAD_FILE:    // Load a file?
            case SETTING_SAVE_FOLDER:  // Save to folder?
            {
                FileSystemSettingWidget *widget = new FileSystemSettingWidget(setting_item, setting, setting.type);
                parent->addChild(setting_item);
                QTreeWidgetItem *place_holder = new QTreeWidgetItem();
                place_holder->setSizeHint(0, QSize(0, 28));
                setting_item->addChild(place_holder);
                _configuration_settings_tree->setItemWidget(place_holder, 0, widget);
                _compound_widgets.push_back(place_holder);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            } break;

            case SETTING_EXCLUSIVE_LIST:  // Combobox - enum - just one thing
            {
                parent->addChild(setting_item);
                setting_item->setText(0, setting.label);
                QTreeWidgetItem *place_holder = new QTreeWidgetItem();
                setting_item->addChild(place_holder);

                EnumSettingWidget *enum_widget = new EnumSettingWidget(setting_item, setting);
                _configuration_settings_tree->setItemWidget(place_holder, 0, enum_widget);
                connect(enum_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            } break;

            case SETTING_STRING:  // Raw text field?
            {
                StringSettingWidget *widget = new StringSettingWidget(setting_item, setting);
                parent->addChild(setting_item);
                QTreeWidgetItem *place_holder = new QTreeWidgetItem();
                setting_item->addChild(place_holder);
                _configuration_settings_tree->setItemWidget(place_holder, 0, widget);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingEdited()));
            } break;

            default: {
                setting_item->setText(0, setting.label);
                setting_item->setToolTip(0, setting.description);
                parent->addChild(setting_item);
                assert(0);  // Unknown setting
            } break;
        }
    }
}

int SettingsTreeManager::GetValidationPresentIndex(const ValidationPreset preset) const {
    for (std::size_t i = 0, n = _validation_presets.size(); i < n; ++i) {
        if (_validation_presets[i] == preset) return static_cast<int>(i);
    }

    assert(0);
    return -1;
}

/// The user has selected a preset for this layer
void SettingsTreeManager::khronosPresetChanged(int preset_index) {
    assert(preset_index >= 0 && preset_index < _validation_presets.size());
    const ValidationPreset preset = _validation_presets[preset_index];

    Configurator &configurator = Configurator::Get();

    configurator.environment.Notify(NOTIFICATION_RESTART);

    // We really just don't care
    if (preset == ValidationPresetUserDefined) return;

    // The easiest way to do this is to create a new profile, and copy the layer over
    const QString preset_file = QString(":/resourcefiles/") + configurator.GetValidationPresetName(preset) + ".json";

    Configuration preset_configuration;
    const bool result = preset_configuration.Load(preset_file);
    assert(result);

    auto configuration = configurator.GetActiveConfiguration();
    auto parameter = FindParameter(configuration->parameters, "VK_LAYER_KHRONOS_validation");
    assert(parameter != configuration->parameters.end());

    // Reset just specific layer settings
    for (std::size_t i = 0, n = parameter->settings.size(); i < n; ++i) {
        LayerSetting &setting = parameter->settings[i];

        if (setting.key == "disables" || setting.key == "enables")
            setting.value = preset_configuration.parameters[0].settings[i].value;
    }

    configuration->_preset = preset;

    // Now we need to reload the Khronos tree item.
    _configuration_settings_tree->blockSignals(true);
    _configuration_settings_tree->setItemWidget(_validation_file_item, 1, nullptr);
    delete _validation_settings;

    QByteArray saved_state;
    QTreeWidgetItem *validation_preset_parent = _validation_preset_item->parent();
    GetTreeState(saved_state, validation_preset_parent);

    for (int i = 0, n = _validation_tree_item->childCount(); i < n; i++) {
        _validation_tree_item->takeChild(0);
    }

    BuildKhronosTree(parameter->settings);
    SetTreeState(saved_state, 0, validation_preset_parent);
    _configuration_settings_tree->blockSignals(false);
    OnSettingEdited();
}

// Any edit to these settings means we are not user defined
// (and that we need to save the settings)
void SettingsTreeManager::OnPresetEdited() {
    _validation_presets_combo_box->blockSignals(true);
    _validation_presets_combo_box->setCurrentIndex(GetValidationPresentIndex(ValidationPresetUserDefined));
    auto configuration = Configurator::Get().GetActiveConfiguration();
    configuration->_preset = ValidationPresetUserDefined;
    _validation_presets_combo_box->blockSignals(false);
    OnSettingEdited();
}

void SettingsTreeManager::GetTreeState(QByteArray &byte_array, QTreeWidgetItem *top_item) {
    if (top_item->isExpanded())
        byte_array.push_back('1');
    else
        byte_array.push_back('0');

    for (int i = 0; i < top_item->childCount(); i++) {
        GetTreeState(byte_array, top_item->child(i));
    }
}

int SettingsTreeManager::SetTreeState(QByteArray &byte_array, int index, QTreeWidgetItem *top_item) {
    // We very well could run out, on initial run, expand everything
    if (index > byte_array.length())
        top_item->setExpanded(true);
    else {
        top_item->setExpanded(byte_array[index++] == '1');
    }

    // Walk the children
    if (top_item->childCount() != 0) {
        for (int i = 0; i < top_item->childCount(); i++) {
            index = SetTreeState(byte_array, index, top_item->child(i));
        }
    }

    return index;
}

void SettingsTreeManager::CleanupGUI() {
    if (_configuration_settings_tree == nullptr)  // Was not initialized
        return;

    Configurator &configurator = Configurator::Get();

    std::vector<Configuration>::iterator configuration = configurator.GetActiveConfiguration();
    if (configuration == configurator.available_configurations.end()) return;

    // Get the state of the last tree, and save it!
    configuration->_setting_tree_state.clear();
    GetTreeState(configuration->_setting_tree_state, _configuration_settings_tree->invisibleRootItem());
    const bool result = configuration->Save(Configurator::Get().path.GetFullPath(PATH_CONFIGURATION, configuration->name));
    assert(result);

    // If a Khronos layer is present, it needs cleanup up from custom controls before
    // it's cleared or deleted.
    auto parameter = FindParameter(configuration->parameters, "VK_LAYER_KHRONOS_validation");
    if (parameter != configuration->parameters.end())
        _configuration_settings_tree->setItemWidget(_validation_file_item, 1, nullptr);

    _validation_file_item = nullptr;

    if (_validation_settings) delete _validation_settings;

    for (std::size_t i = 0; i < _compound_widgets.size(); i++)
        _configuration_settings_tree->setItemWidget(_compound_widgets[i], 1, nullptr);

    _compound_widgets.clear();

    _configuration_settings_tree->clear();
    _configuration_settings_tree = nullptr;
    _validation_presets_combo_box = nullptr;
    _validation_tree_item = nullptr;
    _validation_debug_action = nullptr;
    _validation_preset_item = nullptr;
    _validation_settings = nullptr;
    _validation_log_file_widget = nullptr;
    _validation_log_file_item = nullptr;
}

// The setting has been edited and should be saved
void SettingsTreeManager::OnSettingEdited() {
    Configurator &configurator = Configurator::Get();

    std::vector<Configuration>::iterator configuration = configurator.GetActiveConfiguration();

    const bool result = configuration->Save(configurator.path.GetFullPath(PATH_CONFIGURATION, configuration->name));
    assert(result);

    configurator.environment.Notify(NOTIFICATION_RESTART);
    configurator.RefreshConfiguration();
}
