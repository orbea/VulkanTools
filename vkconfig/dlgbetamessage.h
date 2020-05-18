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
 * Author: Richard S. Wright Jr. <richard@lunarg.com>
 * This is a temporary message that we are currently in "Beta" for this tool
 */

#ifndef DLGBETAMESSAGE_H
#define DLGBETAMESSAGE_H


#include <QDialog>

namespace Ui {
class dlgBetaMessage;
}

class dlgBetaMessage : public QDialog
{
    Q_OBJECT

public:
    explicit dlgBetaMessage(QWidget *parent = nullptr);
    ~dlgBetaMessage();

    bool bDontShow;

private:
    Ui::dlgBetaMessage *ui;

public Q_SLOTS:
    void on_pushButtonDismiss_clicked();
    void dontShowToggled(bool bToggled);
};

#endif // DLGBETAMESSAGE_H