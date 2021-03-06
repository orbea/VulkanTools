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
 * - Richard S. Wright Jr. <richard@lunarg.com>
 * - Christophe Riccio <christophe@lunarg.com>
 */

#pragma once

#include "../vkconfig_core/layer_setting.h"

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>

class MuteMessageWidget : public QWidget {
    Q_OBJECT

   public:
    explicit MuteMessageWidget(LayerSetting &layer_setting);

   public Q_SLOTS:
    void addItem(const QString &item);  // Added from combo box
    void removePushed();                // Remove button

   Q_SIGNALS:
    void itemChanged();
    void itemRemoved(const QString &item);

   private:
    MuteMessageWidget(const MuteMessageWidget &) = delete;
    MuteMessageWidget &operator=(const MuteMessageWidget &) = delete;

    void resizeEvent(QResizeEvent *event) override;

    LayerSetting &_layer_setting;
    QListWidget *_list_widget;
    QPushButton *_remove_button;
};
