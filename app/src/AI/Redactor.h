/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace AI {

/**
 * @brief Redacts secrets from strings before they enter the model context.
 */
class Redactor {
public:
  static bool scrub(QString& text);
  static QJsonObject scrubObject(const QJsonObject& obj);
  static QJsonArray scrubArray(const QJsonArray& arr);
};

}  // namespace AI
