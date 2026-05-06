/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020–2025 Alex Spataru
 *
 * This file is dual-licensed:
 *
 * - Under the GNU GPLv3 (or later) for builds that exclude Pro modules.
 * - Under the Serial Studio Commercial License for builds that include
 *   any Pro functionality.
 *
 * You must comply with the terms of one of these licenses, depending
 * on your use case.
 *
 * For GPL terms, see <https://www.gnu.org/licenses/gpl-3.0.html>
 * For commercial terms, see LICENSE_COMMERCIAL.md in the project root.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Misc {
/**
 * @brief Secure JSON parsing and validation with configurable bounds checking.
 */
class JsonValidator {
public:
  /** @brief Configurable bounds (file size, depth, array size) for JSON validation. */
  struct Limits {
    qsizetype maxFileSize;
    int maxDepth;
    int maxArraySize;

    Limits() : maxFileSize(10 * 1024 * 1024), maxDepth(128), maxArraySize(10000) {}
  };

  /** @brief Result of JSON parsing and validation: valid flag, error string, and document. */
  struct ValidationResult {
    bool valid = false;
    QString errorMessage;
    QJsonDocument document;
  };

  static ValidationResult parseAndValidate(const QByteArray& data);
  static ValidationResult parseAndValidate(const QByteArray& data, const Limits& limits);
  static bool validateStructure(const QJsonValue& value,
                                const Limits& limits,
                                int currentDepth = 0);

private:
  static bool validateObject(const QJsonObject& obj, const Limits& limits, int depth);
  static bool validateArray(const QJsonArray& arr, const Limits& limits, int depth);
};

/** @brief Default validation using standard security limits. */
inline JsonValidator::ValidationResult JsonValidator::parseAndValidate(const QByteArray& data)
{
  return parseAndValidate(data, Limits());
}

/** @brief Full JSON parsing and validation against the supplied limits. */
inline JsonValidator::ValidationResult JsonValidator::parseAndValidate(const QByteArray& data,
                                                                       const Limits& limits)
{
  ValidationResult result;

  // Validation Step 1: Check file size limit
  if (data.size() > limits.maxFileSize) [[unlikely]] {
    result.errorMessage = QString("JSON data exceeds maximum size limit of %1 MB")
                            .arg(limits.maxFileSize / (1024 * 1024));
    return result;
  }

  // Validation Step 2: Check for empty input
  if (data.isEmpty()) [[unlikely]] {
    result.errorMessage = "JSON data is empty";
    return result;
  }

  // Validation Step 3: Parse JSON syntax
  QJsonParseError parseError;
  result.document = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) [[unlikely]] {
    result.errorMessage = QString("JSON parse error at offset %1: %2")
                            .arg(parseError.offset)
                            .arg(parseError.errorString());
    return result;
  }

  // Validation Step 4: Validate structure depth and array sizes
  if (!validateStructure(result.document.isArray() ? QJsonValue(result.document.array())
                                                   : QJsonValue(result.document.object()),
                         limits)) [[unlikely]] {
    result.errorMessage =
      QString("JSON structure validation failed: exceeds depth (%1) or array size (%2) limits")
        .arg(limits.maxDepth)
        .arg(limits.maxArraySize);
    return result;
  }

  // All validations passed
  result.valid = true;
  return result;
}

/** @brief Recursively validates JSON value depth and structure. */
inline bool JsonValidator::validateStructure(const QJsonValue& value,
                                             const Limits& limits,
                                             int currentDepth)
{
  // Check recursion depth limit
  if (currentDepth > limits.maxDepth) [[unlikely]]
    return false;

  // Dispatch based on type
  if (value.isObject())
    return validateObject(value.toObject(), limits, currentDepth);
  else if (value.isArray())
    return validateArray(value.toArray(), limits, currentDepth);

  // Primitives are always valid
  return true;
}

/** @brief Validates a JSON object and all nested children. */
inline bool JsonValidator::validateObject(const QJsonObject& obj, const Limits& limits, int depth)
{
  // Check recursion depth limit
  if (depth > limits.maxDepth) [[unlikely]]
    return false;

  // Recursively validate each property value
  for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
    if (!validateStructure(it.value(), limits, depth + 1)) [[unlikely]]
      return false;

  return true;
}

/** @brief Validates a JSON array size and all nested elements. */
inline bool JsonValidator::validateArray(const QJsonArray& arr, const Limits& limits, int depth)
{
  // Check recursion depth limit
  if (depth > limits.maxDepth) [[unlikely]]
    return false;

  // Check array size limit
  if (arr.size() > limits.maxArraySize) [[unlikely]]
    return false;

  // Recursively validate each element
  for (const auto& element : arr)
    if (!validateStructure(element, limits, depth + 1)) [[unlikely]]
      return false;

  return true;
}

}  // namespace Misc
