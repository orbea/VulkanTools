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

#include "layer.h"
#include "util.h"

#include <QFile>
#include <QMessageBox>

#include <cassert>

////////// A couple of utility functions for building/modifying enable/disable lists

///////////////////////////////////////////////////////////////////////////////
// delimted string is a comma delimited string. If value is found remove it
void RemoveString(QString& delimitedString, QString value) {
    // Well, it's not there now is it...
    if (!delimitedString.contains(value)) return;

    QStringList list = delimitedString.split(",");
    for (int i = 0; i < list.size(); i++)
        if (list[i] == value) {
            list.removeAt(i);
            break;
        }

    delimitedString = list.join(",");
}

/////////////////////////////////////////////////////////////////////////////////
// Pretty simple, add to list if it's not already in it
void AddString(QString& delimitedString, QString value) {
    // Do I have anything to do?
    if (delimitedString.contains(value))  // Nope
        return;

    if (!delimitedString.isEmpty()) delimitedString += ",";

    delimitedString += value;
}

Layer::Layer(LayerType type) : type(type), _rank(0), state(LAYER_STATE_APPLICATION_CONTROLLED) {}

Layer::Layer(LayerType type, QString full_path_to_file) : type(type) {
    bool loaded = Load(full_path_to_file);
    assert(loaded);
}

///////////////////////////////////////////////////////////////////////////////
/// Reports errors via a message box. This might be a bad idea?
bool Layer::Load(QString full_path_to_file) {
    // Open the file, should be text. Read it into a
    // temporary string.
    if (full_path_to_file.isEmpty()) return false;

    QFile file(full_path_to_file);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
        QMessageBox msgBox;
        msgBox.setText("Could not open layer file");
        msgBox.exec();
        return false;
    }

    QString jsonText = file.readAll();
    file.close();

    _layer_path = full_path_to_file;

    //////////////////////////////////////////////////////
    // Convert the text to a JSON document & validate it.
    // It does need to be a valid json formatted file.
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox msgBox;
        msgBox.setText(parseError.errorString());
        msgBox.exec();
        return false;
    }

    // Make sure it's not empty
    if (jsonDoc.isNull() || jsonDoc.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setText("Json document is empty!");
        msgBox.exec();
        return false;
    }

    // Populate key items about the layer
    QJsonObject jsonObject = jsonDoc.object();
    QJsonValue jsonValue = jsonObject.value("file_format_version");
    _file_format_version = jsonValue.toString();

    QJsonValue layerValue = jsonObject.value("layer");
    QJsonObject layerObject = layerValue.toObject();

    jsonValue = layerObject.value("name");
    _name = jsonValue.toString();

    jsonValue = layerObject.value("type");
    _type = jsonValue.toString();

    jsonValue = layerObject.value("library_path");
    _library_path = jsonValue.toString();

    jsonValue = layerObject.value("api_version");
    _api_version = jsonValue.toString();

    jsonValue = layerObject.value("implementation_version");
    _implementation_version = jsonValue.toString();

    jsonValue = layerObject.value("description");
    _description = jsonValue.toString();

    // The layer file is loaded
    return true;
}

void Layer::LoadSettingsFromJson(const QJsonObject& layer_settings_descriptors, std::vector<LayerSetting>& settings) {
    // Okay, how many settings do we have?
    QStringList settings_names = layer_settings_descriptors.keys();

    for (int i = 0, n = settings_names.size(); i < n; i++) {
        // The layer rank may or may not be here, but it is not a
        // user setting.
        if (settings_names[i] == QString("layer_rank")) continue;

        auto setting = settings.emplace(settings.end(), LayerSetting()); 

        setting->name = settings_names[i];

        QJsonValue settingValue = layer_settings_descriptors.value(settings_names[i]);
        QJsonObject settingObject = settingValue.toObject();

        // The easy stuff...
        QJsonValue value = settingObject.value("description");
        setting->description = value.toString();

        value = settingObject.value("name");
        setting->label = value.toString();

        // This is either a single value, or a comma delimted set of strings
        // selected from a nonexclusive list
        value = settingObject.value("default");
        if (value.isArray()) {
            QJsonArray array = value.toArray();
            for (int a = 0; a < array.size(); a++) {
                setting->value += array[a].toString();
                if (a != array.size() - 1) setting->value += ",";
            }

        } else
            setting->value = value.toString();

        ///////////////////////////////////////////////////////////////////////
        // Everything from here down revolves around the data type
        // Data types and values start getting a little more involved.
        value = settingObject.value("type");

        QString type_string = value.toString();

        setting->type = GetSettingType(type_string.toUtf8().constData());

        // Exclusive Enums
        if (setting->type == SETTING_EXCLUSIVE_LIST) {
            // Now we have a list of options, both the enum for the settings file, and the prompts
            value = settingObject.value("options");
            QJsonObject object = value.toObject();
            QStringList keys, values;
            keys = object.keys();
            for (int v = 0; v < keys.size(); v++) {
                setting->exclusive_values << keys[v];
                setting->exclusive_label << object.value(keys[v]).toString();
            }
        }
        // Pick one or more from a list
        else if (setting->type == SETTING_INCLUSIVE_LIST) {
            // Now we have a list of options, both the enum for the settings file, and the prompts
            value = settingObject.value("options");
            QJsonObject object = value.toObject();
            QStringList keys, values;
            keys = object.keys();
            for (int v = 0; v < keys.size(); v++) {
                setting->inclusive_values << keys[v];
                setting->inclusive_labels << object.value(keys[v]).toString();
            }
        }
    }
}
