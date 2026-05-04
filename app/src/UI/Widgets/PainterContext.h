/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2026 Alex Spataru
 *
 * Pro feature -- requires the Serial Studio Commercial License.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#ifdef BUILD_COMMERCIAL

#  include <QBrush>
#  include <QColor>
#  include <QFont>
#  include <QObject>
#  include <QPainter>
#  include <QPainterPath>
#  include <QPen>
#  include <QString>
#  include <QStringList>

namespace Widgets {

/**
 * @brief Canvas2D-shaped facade over QPainter exposed to the painter widget JS.
 *
 * The owning Painter widget calls beginFrame() before invoking the user's
 * paint(ctx, w, h) and endFrame() after. While "active", every Q_INVOKABLE
 * method translates to a QPainter operation. drawImage() resolves source
 * paths through a small sandbox: qrc:/ resources, the project file's
 * directory, and QStandardPaths::DocumentsLocation are allowed; everything
 * else is rejected so user JS cannot read C:/Windows or hidden config dirs.
 */
class PainterContext : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString fillStyle READ fillStyle WRITE setFillStyle)
  Q_PROPERTY(QString strokeStyle READ strokeStyle WRITE setStrokeStyle)
  Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth)
  Q_PROPERTY(QString lineCap READ lineCap WRITE setLineCap)
  Q_PROPERTY(QString lineJoin READ lineJoin WRITE setLineJoin)
  Q_PROPERTY(QString font READ fontSpec WRITE setFontSpec)
  Q_PROPERTY(QString textAlign READ textAlign WRITE setTextAlign)
  Q_PROPERTY(QString textBaseline READ textBaseline WRITE setTextBaseline)
  Q_PROPERTY(qreal globalAlpha READ globalAlpha WRITE setGlobalAlpha)

public:
  explicit PainterContext(QObject* parent = nullptr);
  ~PainterContext() override = default;

  void beginFrame(QPainter* painter, qreal width, qreal height);
  void endFrame();

  void setProjectDirectory(const QString& dir);

  [[nodiscard]] QString fillStyle() const;
  [[nodiscard]] QString strokeStyle() const;
  [[nodiscard]] qreal lineWidth() const;
  [[nodiscard]] QString lineCap() const;
  [[nodiscard]] QString lineJoin() const;
  [[nodiscard]] QString fontSpec() const;
  [[nodiscard]] QString textAlign() const;
  [[nodiscard]] QString textBaseline() const;
  [[nodiscard]] qreal globalAlpha() const;

  void setFillStyle(const QString& spec);
  void setStrokeStyle(const QString& spec);
  void setLineWidth(qreal w);
  void setLineCap(const QString& cap);
  void setLineJoin(const QString& join);
  void setFontSpec(const QString& spec);
  void setTextAlign(const QString& align);
  void setTextBaseline(const QString& baseline);
  void setGlobalAlpha(qreal a);

public slots:
  void save();
  void restore();
  void translate(qreal x, qreal y);
  void rotate(qreal radians);
  void scale(qreal sx, qreal sy);
  void resetTransform();

  void beginPath();
  void closePath();
  void moveTo(qreal x, qreal y);
  void lineTo(qreal x, qreal y);
  void rect(qreal x, qreal y, qreal w, qreal h);
  void arc(qreal x, qreal y, qreal r, qreal startRad, qreal endRad, bool counterClockwise = false);
  void quadraticCurveTo(qreal cpx, qreal cpy, qreal x, qreal y);
  void bezierCurveTo(qreal c1x, qreal c1y, qreal c2x, qreal c2y, qreal x, qreal y);

  void fill();
  void stroke();
  void clip();

  void fillRect(qreal x, qreal y, qreal w, qreal h);
  void strokeRect(qreal x, qreal y, qreal w, qreal h);
  void clearRect(qreal x, qreal y, qreal w, qreal h);

  void fillText(const QString& text, qreal x, qreal y);
  void strokeText(const QString& text, qreal x, qreal y);
  [[nodiscard]] qreal measureTextWidth(const QString& text) const;

  void drawImage(const QString& src, qreal x, qreal y);
  void drawImageScaled(const QString& src, qreal x, qreal y, qreal w, qreal h);

  [[nodiscard]] qreal width() const noexcept;
  [[nodiscard]] qreal height() const noexcept;

private:
  struct State {
    QBrush fillBrush;
    QPen strokePen;
    QFont font;
    QString fillSpec;
    QString strokeSpec;
    QString fontSpecCached;
    QString textAlign;
    QString textBaseline;
    qreal globalAlpha;
  };

  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] QColor parseColor(const QString& spec) const;
  [[nodiscard]] QFont parseFontSpec(const QString& spec) const;
  [[nodiscard]] QString resolveImagePath(const QString& src) const;
  [[nodiscard]] QPointF alignTextOrigin(const QString& text, qreal x, qreal y) const;

  QPainter* m_painter;
  qreal m_width;
  qreal m_height;
  QPainterPath m_path;
  State m_state;
  QString m_projectDir;
};

}  // namespace Widgets

#endif  // BUILD_COMMERCIAL
