/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * Redistribution, modification, or use of this file in any form
 * is permitted only under the terms of a valid commercial license
 * obtained from the author.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QSettings>
#include <QString>

#include "Licensing/SimpleCrypt.h"

namespace AI {

/**
 * @brief AI provider identifiers, index-aligned with the QML combobox model.
 */
enum class ProviderId : int {
  Anthropic = 0,
  OpenAI    = 1,
  Gemini    = 2,
  DeepSeek  = 3,
  Local     = 4,
};

/** @brief Total number of AI providers wired into the Assistant. */
inline constexpr int kProviderCount = 5;

/**
 * @brief RAII helper that best-effort scrubs a QString on destruction.
 *
 * Qt's QString is copy-on-write, so this is obfuscation-grade only:
 * it clears the unique buffer this instance holds, not aliased copies
 * elsewhere in the program. Use only for short-lived plaintext copies
 * on the API key path.
 */
class ZeroOnDestroy {
public:
  explicit ZeroOnDestroy(QString& target) noexcept;
  ~ZeroOnDestroy();
  ZeroOnDestroy(ZeroOnDestroy&&)                 = delete;
  ZeroOnDestroy(const ZeroOnDestroy&)            = delete;
  ZeroOnDestroy& operator=(ZeroOnDestroy&&)      = delete;
  ZeroOnDestroy& operator=(const ZeroOnDestroy&) = delete;

private:
  QString& m_ref;
};

/**
 * @brief Per-machine encrypted storage for BYOK API keys.
 *
 * Keys live under QSettings group "ai/keys" with sub-keys "anthropic",
 * "openai", "gemini". Ciphertext is produced by Licensing::SimpleCrypt
 * keyed off Licensing::MachineID::machineSpecificKey() with
 * ProtectionHash integrity. Storage is per-machine: copying the
 * settings file to another machine yields unrecoverable values.
 *
 * Threat model: this is obfuscation-grade, not enterprise crypto. It
 * stops casual snooping and accidental leakage through cloud-synced
 * config backups. A determined local attacker with binary access can
 * reverse the key derivation. A future migration to qtkeychain keeps
 * this same setKey/key/clearKey/hasKey surface unchanged.
 */
class KeyVault {
public:
  KeyVault();

  [[nodiscard]] bool hasKey(ProviderId provider) const;
  [[nodiscard]] bool hasAnyKey() const;
  [[nodiscard]] QString key(ProviderId provider) const;

  void setKey(ProviderId provider, const QString& plaintext);
  void clearKey(ProviderId provider);
  void clearAllKeys();

  [[nodiscard]] static QString redact(const QString& key);

private:
  [[nodiscard]] static QString settingsKey(ProviderId provider);

private:
  mutable QSettings m_settings;
  mutable Licensing::SimpleCrypt m_simpleCrypt;
};

}  // namespace AI
